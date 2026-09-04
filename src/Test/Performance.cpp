/*
* Tests for Common Purpose Library (http://github.com/ermig1979/Cpl).
*
* Copyright (c) 2021-2025 Yermalayeu Ihar.
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#include "Test/Test.h"

#include "Cpl/Performance.h"

#include <atomic>
#include <cmath>

namespace Test
{
    static void TestFuncV0()
    {
        CPL_PERF_FUNC();
        std::this_thread::sleep_for(std::chrono::milliseconds(45));
    }

    static void TestFuncV1()
    {
        CPL_PERF_FUNC();
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    static void TestFuncV2()
    {
        CPL_PERF_FUNCF(1000 * 1000 * 1000);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    static void TestFuncV3()
    {
        CPL_PERF_INIT(pm, "1 & 3");

        CPL_PERF_START(pm);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        CPL_PERF_PAUSE(pm);

        std::this_thread::sleep_for(std::chrono::milliseconds(15));

        CPL_PERF_START(pm);
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
        CPL_PERF_PAUSE(pm);
    }

    static void TestFuncV4()
    {
        CPL_PERF_FUNCFH(0, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    bool PerformanceSimpleTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        Cpl::PerformanceStorage::Global().Clear();
#endif
        for (size_t i = 0; i < 5; ++i)
            TestFuncV0();

        for (size_t i = 0; i < 10; ++i)
            TestFuncV1();

        for (size_t i = 0; i < 15; ++i)
            TestFuncV2();

        for (size_t i = 0; i < 5; ++i)
            TestFuncV3();

        for (size_t i = 0; i < 50; ++i)
            TestFuncV4();

#if defined(CPL_PERF_ENABLE)
        CPL_LOG_SS(Verbose, std::endl << Cpl::PerformanceStorage::Global().Report());
#endif
        return true;
    }

    static void TestFuncV5()
    {
        std::stringstream ss;
        ss << std::this_thread::get_id();
        CPL_PERF_BEGFH(ss.str(), 0, 100);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    bool PerformanceStdThreadTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        Cpl::PerformanceStorage::Global().Clear();
#endif

        const size_t n = 10, t = 10;
        typedef std::thread Thread;
        typedef std::vector<Thread> Threads;
        Threads threads;

        for (size_t j = 0; j < t; ++j)
        {
            for (size_t i = 0; i < n; ++i)
                threads.push_back(Thread(&TestFuncV5));

            for (size_t i = 0; i < threads.size(); ++i)
                if (threads[i].joinable())
                    threads[i].join();
        }
#if defined(CPL_PERF_ENABLE)
        CPL_LOG_SS(Verbose, std::endl << Cpl::PerformanceStorage::Global().Report());
#endif
        return true;
    }

    bool PerformanceClearTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        Cpl::PerformanceStorage::Global().Clear();
#endif
        TestFuncV0();
#if defined(CPL_PERF_ENABLE)
        CPL_LOG_SS(Verbose, std::endl << Cpl::PerformanceStorage::Global().Report());
#endif
#if defined(CPL_PERF_ENABLE)
        Cpl::PerformanceStorage::Global().Clear();
#endif
        TestFuncV0();
#if defined(CPL_PERF_ENABLE)
        CPL_LOG_SS(Verbose, std::endl << Cpl::PerformanceStorage::Global().Report());
#endif
        return true;
}

    bool PerformanceHistogramExpandTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        Cpl::PerformanceHistogram histogram(4);
        const uint64_t samples[] = { 0, 0, 1, 1, 2, 3, 5, 6, 7, 7 };
        for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
            histogram.Add(samples[i]);

        // Bins after the expansion are [4, 2, 1, 3] with step 2; 25% of 10 samples is reached inside bin 0: 0 + 2/4 * 2.
        double quantile = histogram.Quantile(25.0);
        double expected = Cpl::Miliseconds(1);
        if (std::abs(quantile - expected) > 1e-9)
        {
            CPL_LOG_SS(Error, "PerformanceHistogramExpand: Quantile(25) expected " << expected << ", got " << quantile);
            return false;
        }
        return true;
#else
        CPL_LOG_SS(Warning, "PerformanceHistogramExpand: skipped, CPL_PERF_ENABLE is not defined.");
        return true;
#endif
    }

    //-------------------------------------------------------------------------------------------------

    bool PerformanceHistogramQuantileTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        return RunIsolated([]() -> bool
        {
            Cpl::PerformanceHistogram histogram(4);
            const uint64_t samples[] = { 0, 0, 1, 1, 2, 3 };
            for (size_t i = 0; i < sizeof(samples) / sizeof(samples[0]); ++i)
                histogram.Add(samples[i]);

            // Bins are [2, 2, 1, 1] with step 1; 90% of 6 samples is reached inside bin 2: 2 + 1/1 * 1.
            double quantile = histogram.Quantile(90.0);
            double expected = Cpl::Miliseconds(3);
            if (std::abs(quantile - expected) > 1e-9)
            {
                CPL_LOG_SS(Error, "PerformanceHistogramQuantile: Quantile(90) expected " << expected << ", got " << quantile);
                return false;
            }
            return true;
        });
#else
        CPL_LOG_SS(Warning, "PerformanceHistogramQuantile: skipped, CPL_PERF_ENABLE is not defined.");
        return true;
#endif
    }

    //-------------------------------------------------------------------------------------------------

    bool PerformanceClearWhileHolderAliveTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        return RunIsolated([]() -> bool
        {
            Cpl::PerformanceStorage::Global().Clear();
            Cpl::PerformanceMeasurer* pm = Cpl::PerformanceStorage::Global().Get("PerformanceClearWhileHolderAlive");
            {
                Cpl::PerformanceHolder holder(pm);
                Cpl::PerformanceStorage::Global().Clear();
            }
            return true;
        });
#else
        CPL_LOG_SS(Warning, "PerformanceClearWhileHolderAlive: skipped, CPL_PERF_ENABLE is not defined.");
        return true;
#endif
    }

    //-------------------------------------------------------------------------------------------------

    bool PerformanceStorageSeparateInstancesTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        return RunIsolated([]() -> bool
        {
            Cpl::PerformanceStorage storage1;
            Cpl::PerformanceStorage storage2;

            Cpl::PerformanceMeasurer* pm1 = storage1.Get("PerformanceStorageSeparateInstances1");
            pm1->Enter();
            pm1->Leave();

            Cpl::PerformanceMeasurer* pm2 = storage2.Get("PerformanceStorageSeparateInstances2");
            pm2->Enter();
            pm2->Leave();

            if (storage1.Merged().size() != 1 || storage2.Merged().size() != 1)
            {
                CPL_LOG_SS(Error, "PerformanceStorageSeparateInstances: expected 1 measurer in each storage, got "
                    << storage1.Merged().size() << " and " << storage2.Merged().size());
                return false;
            }
            return true;
        });
#else
        CPL_LOG_SS(Warning, "PerformanceStorageSeparateInstances: skipped, CPL_PERF_ENABLE is not defined.");
        return true;
#endif
    }

    //-------------------------------------------------------------------------------------------------

    bool PerformanceStorageConcurrentReportTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        return RunIsolated([]() -> bool
        {
            const size_t workers = 4, samples = 2000;
            Cpl::PerformanceStorage storage;
            std::atomic<size_t> running(workers);
            std::vector<std::thread> threads;
            for (size_t worker = 0; worker < workers; ++worker)
            {
                threads.push_back(std::thread([&storage, &running, worker]()
                {
                    const Cpl::String prefix = "PerformanceStorageConcurrentReport" + std::to_string(worker) + "_";
                    for (size_t sample = 0; sample < samples; ++sample)
                        Cpl::PerformanceHolder holder(storage.Get(prefix + std::to_string(sample)));
                    running--;
                }));
            }
            size_t reports = 0;
            while (running > 0)
            {
                storage.Report();
                storage.Merged("PerformanceStorageConcurrentReport0_0");
                storage.Clear();
                ++reports;
            }
            for (size_t worker = 0; worker < workers; ++worker)
                threads[worker].join();
            if (reports == 0)
            {
                CPL_LOG_SS(Error, "PerformanceStorageConcurrentReport: the reporting loop did not overlap the workers.");
                return false;
            }
            return true;
        }, 60000);
#else
        CPL_LOG_SS(Warning, "PerformanceStorageConcurrentReport: skipped, CPL_PERF_ENABLE is not defined.");
        return true;
#endif
    }

    //-------------------------------------------------------------------------------------------------

    bool PerformanceStorageThreadIdReuseTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        return RunIsolated([]() -> bool
        {
            const Cpl::String name = "PerformanceStorageThreadIdReuse";
            Cpl::PerformanceStorage storage;
            std::thread::id firstId, secondId;
            double secondTotal = 0.0;

            std::thread first([&storage, &name, &firstId]()
            {
                firstId = std::this_thread::get_id();
                storage.Get(name)->Enter();
            });
            first.join();

            std::this_thread::sleep_for(std::chrono::milliseconds(200));

            std::thread second([&storage, &name, &secondId, &secondTotal]()
            {
                secondId = std::this_thread::get_id();
                Cpl::PerformanceMeasurer* pm = storage.Get(name);
                pm->Enter();
                pm->Leave();
                secondTotal = pm->Total();
            });
            second.join();

            if (firstId != secondId)
            {
                CPL_LOG_SS(Warning, "PerformanceStorageThreadIdReuse: the thread id was not reused, nothing to check.");
                return true;
            }
            if (secondTotal >= 100.0)
            {
                CPL_LOG_SS(Error, "PerformanceStorageThreadIdReuse: the second thread inherited the first thread's open sample, total " << secondTotal << " ms.");
                return false;
            }
            return true;
        });
#else
        CPL_LOG_SS(Warning, "PerformanceStorageThreadIdReuse: skipped, CPL_PERF_ENABLE is not defined.");
        return true;
#endif
    }

    //-------------------------------------------------------------------------------------------------

#if defined(CPL_TEST_NORETURN)
    static void* TestFuncV6(void*)
    {
    }

    bool PerformanceNoReturnTest(const Options& options)
    {
        TestFuncV6(NULL);

        return true;
    }
#endif
}

#if defined(__linux__)
#include <pthread.h>

namespace Test
{
    static void* TestFuncV7(void*)
    {
        CPL_PERF_FUNC();
        return 0;
    }  

    bool PerformancePthreadTest(const Options& options)
    {
#if defined(CPL_PERF_ENABLE)
        Cpl::PerformanceStorage::Global().Clear();
#endif
        for (int i = 0; i < 1; ++i)
        {
            pthread_t thread_run;
            pthread_create(&thread_run, NULL, TestFuncV7, NULL);
            pthread_join(thread_run, NULL);
        }
#if defined(CPL_PERF_ENABLE)
        CPL_LOG_SS(Verbose, std::endl << Cpl::PerformanceStorage::Global().Report());
#endif
        return true;
    }
}
#endif
