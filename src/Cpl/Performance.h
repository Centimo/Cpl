/*
* Common Purpose Library (http://github.com/ermig1979/Cpl).
*
* Copyright (c) 2021-2026 Yermalayeu Ihar.
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

#pragma once

#include "Cpl/Defs.h"
#include "Cpl/Time.h"
#include "Cpl/Utils.h"
#include "Cpl/String.h"

#include <mutex>
#include <map>
#include <unordered_map>
#include <thread>
#include <memory>

#if defined(_MSC_VER)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__GNUC__)
#include <sys/time.h>
#else
#error Platform is not supported!
#endif

#if defined(CPL_PERF_ENABLE)
namespace Cpl
{
    /*! @ingroup cpl_performance
    * \class PerformanceHistogram
    * \brief Adaptive histogram of measured time samples used to estimate quantiles.
    * \note Samples are TimeCounter ticks. Quantile() converts the estimated tick value to milliseconds.
    *       A histogram with size 0 is disabled and stores no samples.
    *       The class is compiled only when CPL_PERF_ENABLE is defined.
    */
    class PerformanceHistogram
    {
        typedef std::vector<uint64_t> Histogram;

        uint64_t _shift, _max;
        Histogram _histogram;

        CPL_INLINE void Expand()
        {
            size_t target = 0;
            for (size_t source = 0; source < _histogram.size(); source += 2, target += 1)
                _histogram[target] = _histogram[source + 0] + _histogram[source + 1];
            for (; target < _histogram.size(); target += 1)
                _histogram[target] = 0;
            _shift++;
            _max *= 2;
        }

    public:
        /*!
        * \fn PerformanceHistogram(uint32_t size = 0)
        * \brief Constructs a histogram with the given number of bins.
        * \param [in] size - Bin count. 0 disables the histogram. The count is rounded up to an even value.
        */
        CPL_INLINE PerformanceHistogram(uint32_t size = 0)
            : _shift(0)
            , _max(size)
            , _histogram(AlignHi(size, 2), 0)
        {
        }

        /*!
        * \fn bool Enable() const
        * \brief Checks whether the histogram stores samples.
        * \return true if the histogram has at least one bin, false if it was constructed with size 0.
        */
        CPL_INLINE bool Enable() const
        {
            return _histogram.size() > 0;
        }

        /*!
        * \fn void Add(uint64_t value)
        * \brief Adds a sample. The bin range is expanded until value fits.
        * \param [in] value - Sample in TimeCounter ticks.
        */
        CPL_INLINE void Add(uint64_t value)
        {
            while (value >= _max)
                Expand();
            _histogram[value >> _shift]++;
        }

        /*!
        * \fn void Merge(const PerformanceHistogram& other)
        * \brief Adds the samples of another histogram of the same bin count.
        * \param [in] other - Histogram to merge. Must have the same number of bins.
        */
        CPL_INLINE void Merge(const PerformanceHistogram& other)
        {
            assert(_histogram.size() == other._histogram.size());
            while (other._shift > _shift)
                Expand();
            size_t step = size_t(1) << (_shift - other._shift);
            for (size_t o = 0, t = 0; o < other._histogram.size(); t++, o += step)
            {
                for (size_t i = o, n = std::min(o + step, other._histogram.size()); i < n; ++i)
                    _histogram[t] += other._histogram[i];
            }
        }

        /*!
        * \fn double Quantile(double quantile) const
        * \brief Estimates the sample value at the given percentile and converts it to milliseconds.
        * \param [in] quantile - Percentile in the range 0..100. Values outside that range are clamped.
        * \return Interpolated duration in milliseconds, or the upper bound of the last bin when the percentile is 100.
        */
        CPL_INLINE double Quantile(double quantile) const
        {
            quantile = std::max(0.0, std::min(quantile, 100.0));
            uint64_t total = 0;
            for (size_t i = 0; i < _histogram.size(); i++)
                total += _histogram[i];
            uint64_t threshold = uint64_t(quantile * total / 100.0), lower = 0, upper = 0;
            size_t index = 0;
            for (; index < _histogram.size(); index++)
            {
                lower = upper;
                upper += _histogram[index];
                if (upper >= threshold)
                    break;
            }
            uint64_t step = uint64_t(1) << _shift;
            if (index == _histogram.size())
                return Miliseconds(_histogram.size() * step);
            uint64_t value = index * step;
            if (upper > lower)
                value += (threshold - lower) * step / (upper - lower);
            return Miliseconds(value);
        }
    };

    //-----------------------------------------------------------------------------------------------------

    /*! @ingroup cpl_performance
    * \class PerformanceMeasurer
    * \brief Accumulates timed samples for one named region and reports statistics.
    * \note Durations are measured with TimeCounter and reported in milliseconds.
    *       The class is compiled only when CPL_PERF_ENABLE is defined.
    */
    class PerformanceMeasurer
    {
        String	_name;
        int64_t _start, _current, _total, _min, _max;
        int64_t _count, _flop;
        uint32_t _hist;
        bool _entered, _paused;
        PerformanceHistogram _histogram;
        mutable std::mutex _mutex;

        // Delegation target of the copy constructor: the guard is created by the delegating constructor
        // and keeps pm locked while its fields are read.
        CPL_INLINE PerformanceMeasurer(const PerformanceMeasurer& pm, const std::lock_guard<std::mutex>&)
            : _name(pm._name)
            , _start(pm._start)
            , _current(pm._current)
            , _total(pm._total)
            , _min(pm._min)
            , _max(pm._max)
            , _count(pm._count)
            , _flop(pm._flop)
            , _hist(pm._hist)
            , _entered(pm._entered)
            , _paused(pm._paused)
            , _histogram(pm._histogram)
        {
        }

    public:
        /*!
        * \fn PerformanceMeasurer(const String& name, int64_t flop = 0, uint32_t hist = 0)
        * \brief Constructs a measurer for the given name.
        * \param [in] name - Region name used in reports and when merging samples.
        * \param [in] flop - Floating-point operations performed in one sample. 0 disables GFlops().
        * \param [in] hist - Histogram bin count passed to PerformanceHistogram. 0 disables quantile statistics.
        */
        CPL_INLINE PerformanceMeasurer(const String& name, int64_t flop = 0, uint32_t hist = 0)
            : _name(name)
            , _start(0)
            , _current(0)
            , _total(0)
            , _min(std::numeric_limits<int64_t>::max())
            , _max(std::numeric_limits<int64_t>::min())
            , _count(0)
            , _flop(flop)
            , _hist(hist)
            , _entered(false)
            , _paused(false)
            , _histogram(hist)
        {
        }

        /*!
        * \fn PerformanceMeasurer(const PerformanceMeasurer& pm)
        * \brief Copies another measurer, including its accumulated statistics.
        * \param [in] pm - Measurer to copy.
        */
        CPL_INLINE PerformanceMeasurer(const PerformanceMeasurer& pm)
            : PerformanceMeasurer(pm, std::lock_guard<std::mutex>(pm._mutex))
        {
        }

        /*!
        * \fn PerformanceMeasurer& operator = (const PerformanceMeasurer& pm)
        * \brief Replaces the name and the accumulated statistics with a copy of another measurer's.
        * \param [in] pm - Measurer to copy.
        * \return Reference to this measurer.
        */
        CPL_INLINE PerformanceMeasurer& operator = (const PerformanceMeasurer& pm)
        {
            const PerformanceMeasurer copy(pm);
            std::lock_guard<std::mutex> lock(_mutex);
            _name = copy._name;
            _flop = copy._flop;
            _count = copy._count;
            _start = copy._start;
            _current = copy._current;
            _total = copy._total;
            _min = copy._min;
            _max = copy._max;
            _hist = copy._hist;
            _entered = copy._entered;
            _paused = copy._paused;
            _histogram = copy._histogram;
            return *this;
        }

        /*!
        * \fn void Enter()
        * \brief Starts a sample if one is not already running.
        *        Records TimeCounter() as the start of the current interval.
        */
        CPL_INLINE void Enter()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (!_entered)
            {
                _entered = true;
                _paused = false;
                _start = TimeCounter();
            }
        }

        /*!
        * \fn void Leave(bool pause = false)
        * \brief Stops the current interval. Optionally commits it as a completed sample.
        * \param [in] pause - If false, add the interval to the totals, update min/max/histogram and reset the current interval.
        *                     If true, keep the accumulated interval so Enter() can resume the same sample.
        */
        CPL_INLINE void Leave(bool pause = false)
        {
            std::lock_guard<std::mutex> lock(_mutex);
            if (_entered || _paused)
            {
                if (_entered)
                {
                    _entered = false;
                    _current += TimeCounter() - _start;
                }
                if (!pause)
                {
                    _total += _current;
                    _min = std::min(_min, _current);
                    _max = std::max(_max, _current);
                    ++_count;
                    if (_histogram.Enable())
                        _histogram.Add(_current);
                    _current = 0;
                }
                _paused = pause;
            }
        }

        /*!
        * \fn void Merge(const PerformanceMeasurer& other)
        * \brief Adds the completed samples of another measurer with the same name.
        * \param [in] other - Measurer to merge. Must have the same Name().
        */
        CPL_INLINE void Merge(const PerformanceMeasurer& other)
        {
            const PerformanceMeasurer copy(other);
            std::lock_guard<std::mutex> lock(_mutex);
            assert(_name == copy._name);
            _count += copy._count;
            _total += copy._total;
            _min = std::min(_min, copy._min);
            _max = std::max(_max, copy._max);
            if (_histogram.Enable())
                _histogram.Merge(copy._histogram);
        }

        /*!
        * \fn void Reset()
        * \brief Clears the accumulated statistics, keeping identity, name, flop count and histogram bin count.
        * \note Resets in place instead of destroying the object, so a pointer obtained from PerformanceStorage
        *       before the reset stays valid afterwards.
        */
        CPL_INLINE void Reset()
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _start = 0;
            _current = 0;
            _total = 0;
            _min = std::numeric_limits<int64_t>::max();
            _max = std::numeric_limits<int64_t>::min();
            _count = 0;
            _entered = false;
            _paused = false;
            _histogram = PerformanceHistogram(_hist);
        }

        /*!
        * \fn double Average() const
        * \brief Returns the mean sample duration.
        * \return Total() / Count() in milliseconds, or 0 if no samples were recorded.
        */
        CPL_INLINE double Average() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _count ? (Miliseconds(_total) / _count) : 0;
        }

        /*!
        * \fn double GFlops() const
        * \brief Returns the average throughput in gigaflops.
        * \return flop * Count() / Total() / 1e6, or 0 if flop is 0 or no samples were recorded.
        */
        CPL_INLINE double GFlops() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _count && _flop && _total > 0 ? (double(_flop) * _count / Miliseconds(_total) / 1000000.0) : 0;
        }

        /*!
        * \fn double Min() const
        * \brief Returns the shortest completed sample.
        * \return Minimum duration in milliseconds, or 0 if no samples were recorded.
        */
        CPL_INLINE double Min() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _count ? Miliseconds(_min) : 0.0;
        }

        /*!
        * \fn double Max() const
        * \brief Returns the longest completed sample.
        * \return Maximum duration in milliseconds, or 0 if no samples were recorded.
        */
        CPL_INLINE double Max() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _count ? Miliseconds(_max) : 0.0;
        }

        /*!
        * \fn double Quantile(double quantile) const
        * \brief Estimates a percentile of the completed samples from the optional histogram.
        * \param [in] quantile - Percentile in the range 0..100.
        * \return Estimated duration in milliseconds, or 0 if there are no samples or the histogram is disabled.
        */
        CPL_INLINE double Quantile(double quantile) const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _count && _histogram.Enable() ? _histogram.Quantile(quantile) : 0.0;
        }

        /*!
        * \fn double Total() const
        * \brief Returns the sum of all completed samples.
        * \return Total duration in milliseconds.
        */
        CPL_INLINE double Total() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return Miliseconds(_total);
        }

        /*!
        * \fn size_t Count() const
        * \brief Returns the number of completed samples.
        * \return Sample count.
        */
        CPL_INLINE size_t Count() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return (size_t)_count;
        }

        /*!
        * \fn String Name() const
        * \brief Returns the region name passed to the constructor.
        * \return Measurer name.
        */
        CPL_INLINE String Name() const
        {
            std::lock_guard<std::mutex> lock(_mutex);
            return _name;
        }

        /*!
        * \fn String ToStr() const
        * \brief Formats the accumulated statistics as a single-line report.
        * \return String of the form "total ms / count = average ms {min; max[; q50; q90; q99]}[ GFlops]".
        */
        CPL_INLINE String ToStr() const
        {
            const PerformanceMeasurer copy(*this);
            std::stringstream ss;
            ss << Cpl::ToStr(copy.Total(), 0) << " ms" << " / " << copy.Count() << " = " << Cpl::ToStr(copy.Average(), 3) << " ms";
            ss << " {min = " << Cpl::ToStr(copy.Min(), 3);
            ss << "; max = " << Cpl::ToStr(copy.Max(), 3);
            if (copy._histogram.Enable())
            {
                ss << "; q50 = " << Cpl::ToStr(copy.Quantile(50.0), 3);
                ss << "; q90 = " << Cpl::ToStr(copy.Quantile(90.0), 3);
                ss << "; q99 = " << Cpl::ToStr(copy.Quantile(99.0), 3);
            }
            ss << "}";
            if (copy._flop)
                ss << " " << Cpl::ToStr(copy.GFlops(), 1) << " GFlops";
            return ss.str();
        }
    };

    //-----------------------------------------------------------------------------------------------------

    /*! @ingroup cpl_performance
    * \class PerformanceHolder
    * \brief RAII helper that starts a PerformanceMeasurer on construction and stops it on destruction.
    * \note Leave() is not called if the pointer is null. The class is compiled only when CPL_PERF_ENABLE is defined.
    */
    class PerformanceHolder
    {
        PerformanceMeasurer* _pm;

    public:
        /*!
        * \fn PerformanceHolder(PerformanceMeasurer* pm, bool enter = true)
        * \brief Stores the measurer pointer and optionally starts a sample.
        * \param [in] pm - Measurer to control, or NULL to make every method a no-op.
        * \param [in] enter - If true and pm is not NULL, call Enter() immediately.
        */
        inline PerformanceHolder(PerformanceMeasurer* pm, bool enter = true)
            : _pm(pm)
        {
            if (_pm && enter)
                _pm->Enter();
        }

        /*!
        * \fn void Enter()
        * \brief Starts a sample on the stored measurer.
        */
        inline void Enter()
        {
            if (_pm)
                _pm->Enter();
        }

        /*!
        * \fn void Leave(bool pause)
        * \brief Stops the current interval on the stored measurer.
        * \param [in] pause - Forwarded to PerformanceMeasurer::Leave. true keeps the sample open for a later Enter().
        */
        inline void Leave(bool pause)
        {
            if (_pm)
                _pm->Leave(pause);
        }

        /*!
        * \fn ~PerformanceHolder()
        * \brief Commits the current sample by calling Leave() without pause.
        */
        inline ~PerformanceHolder()
        {
            if (_pm)
                _pm->Leave();
        }
    };

    //-----------------------------------------------------------------------------------------------------

    /*! @ingroup cpl_performance
    * \class PerformanceStorage
    * \brief Thread-local registry of PerformanceMeasurer objects used by the performance macros.
    * \note Each thread has its own map. When a thread exits, its completed samples are merged into a map of
    *       finished threads and its own map is released. Merged(), Merged(name) and Report() combine the samples
    *       of the live and the finished threads. The class is compiled only when CPL_PERF_ENABLE is defined.
    */
    class PerformanceStorage
    {
    public:
        /*!
        * \typedef Pm
        * \brief Alias for PerformanceMeasurer.
        */
        typedef PerformanceMeasurer Pm;

        /*!
        * \typedef PmPtr
        * \brief Shared pointer to a PerformanceMeasurer.
        */
        typedef std::shared_ptr<Pm> PmPtr;

        /*!
        * \typedef FunctionMap
        * \brief Map from region name to the measurer for that name.
        */
        typedef std::map<String, PmPtr> FunctionMap;

        /*!
        * \fn PerformanceStorage()
        * \brief Constructs an empty storage with no thread maps.
        */
        PerformanceStorage()
            : _state(std::make_shared<State>())
        {
        }

        PerformanceStorage(const PerformanceStorage&) = delete;
        PerformanceStorage& operator = (const PerformanceStorage&) = delete;

        /*!
        * \fn PerformanceMeasurer* Get(const String& name, int64_t flop = 0, uint32_t hist = 0)
        * \brief Returns the measurer for name in the current thread, creating it on first use.
        * \param [in] name - Region name.
        * \param [in] flop - Floating-point operations per sample. Used only when the measurer is created.
        * \param [in] hist - Histogram bin count. Used only when the measurer is created.
        * \return Pointer to the thread-local measurer for name.
        */
        CPL_INLINE PerformanceMeasurer* Get(const String& name, int64_t flop = 0, uint32_t hist = 0)
        {
            ThreadData& thread = ThisThread();
            std::lock_guard<std::mutex> lock(thread.mutex);
            PerformanceMeasurer* pm = NULL;
            FunctionMap::iterator it = thread.map.find(name);
            if (it == thread.map.end())
            {
                pm = new PerformanceMeasurer(name, flop, hist);
                thread.map[name].reset(pm);
            }
            else
                pm = it->second.get();
            return pm;
        }

        /*!
        * \fn PerformanceMeasurer* Get(const String func, const String& desc, int64_t flop = 0, uint32_t hist = 0)
        * \brief Returns the measurer for a function-plus-description name in the current thread.
        *        The stored name is `func{ desc }`.
        * \param [in] func - Function name, typically CPL_FUNCTION.
        * \param [in] desc - Block description appended to the function name.
        * \param [in] flop - Floating-point operations per sample. Used only when the measurer is created.
        * \param [in] hist - Histogram bin count. Used only when the measurer is created.
        * \return Pointer to the thread-local measurer for the combined name.
        */
        CPL_INLINE PerformanceMeasurer* Get(const String func, const String& desc, int64_t flop = 0, uint32_t hist = 0)
        {
            return Get(func + "{ " + desc + " }", flop, hist);
        }

        /*!
        * \fn FunctionMap Merged() const
        * \brief Combines every thread-local measurer by name.
        * \return Map of name to a measurer that holds the merged samples of all threads.
        */
        FunctionMap Merged() const
        {
            FunctionMap merged;
            std::lock_guard<std::mutex> lock(_state->mutex);
            MergeInto(merged, _state->finished);
            for (ThreadMap::const_iterator thread = _state->threads.begin(); thread != _state->threads.end(); ++thread)
            {
                std::lock_guard<std::mutex> threadLock(thread->second->mutex);
                MergeInto(merged, thread->second->map);
            }
            return merged;
        }

        /*!
        * \fn PerformanceMeasurer Merged(const String & name) const
        * \brief Combines the measurers named name from every thread that has recorded at least one sample.
        * \param [in] name - Region name to merge.
        * \return Measurer with the merged samples, or an empty measurer with that name if none were found.
        */
        PerformanceMeasurer Merged(const String & name) const
        {
            PerformanceMeasurer merged(name);
            std::lock_guard<std::mutex> lock(_state->mutex);
            MergeNamed(merged, _state->finished, name);
            for (ThreadMap::const_iterator thread = _state->threads.begin(); thread != _state->threads.end(); ++thread)
            {
                std::lock_guard<std::mutex> threadLock(thread->second->mutex);
                MergeNamed(merged, thread->second->map, name);
            }
            return merged;
        }

        /*!
        * \fn void Clear()
        * \brief Resets the accumulated statistics of every thread-local measurer and drops the samples of finished threads.
        * \note Measurers of live threads are reset in place, not destroyed, so a pointer previously returned by Get()
        *       stays valid after Clear() is called.
        */
        void Clear()
        {
            std::lock_guard<std::mutex> lock(_state->mutex);
            _state->finished.clear();
            for (ThreadMap::iterator thread = _state->threads.begin(); thread != _state->threads.end(); ++thread)
            {
                std::lock_guard<std::mutex> threadLock(thread->second->mutex);
                for (FunctionMap::iterator function = thread->second->map.begin(); function != thread->second->map.end(); ++function)
                    function->second->Reset();
            }
        }

        /*!
        * \fn String Report() const
        * \brief Builds a multi-line report of every named region that has completed at least one sample.
        * \return One `name: ToStr()` line per non-empty measurer, separated by newlines.
        */
        String Report() const
        {
            FunctionMap merged = Merged();
            std::stringstream report;
            for (FunctionMap::const_iterator function = merged.begin(); function != merged.end(); ++function)
            {
                const PerformanceMeasurer& pm = *function->second;
                if (pm.Count())
                    report << function->first << ": " << pm.ToStr() << std::endl;
            }
            return report.str();
        }

        /*!
        * \fn static PerformanceStorage& Global()
        * \brief Returns the process-wide storage singleton used by the performance macros.
        * \return Reference to the global PerformanceStorage instance.
        */
        static PerformanceStorage& Global()
        {
            static PerformanceStorage storage;
            return storage;
        }

    private:
        // Measurers of one thread. The mutex orders Get() in the owner thread against Merged() and Clear().
        struct ThreadData
        {
            mutable std::mutex mutex;
            FunctionMap map;
        };
        typedef std::shared_ptr<ThreadData> ThreadDataPtr;
        // Live threads keyed by registration number; a thread id is not used as a key because the
        // system reuses it after the thread exits.
        typedef std::map<uint64_t, ThreadDataPtr> ThreadMap;

        // Shared with the thread-local cache entries through a weak_ptr: the exit-time merge of a thread
        // either keeps the state alive until it is done or, if the storage is already gone, skips it.
        struct State
        {
            std::mutex mutex;
            ThreadMap threads;
            FunctionMap finished;
            uint64_t registration;

            State()
                : registration(0)
            {
            }
        };
        std::shared_ptr<State> _state;

        struct ThreadCacheEntry
        {
            std::weak_ptr<State> state;
            uint64_t key;
            ThreadDataPtr data;
        };

        // Thread-local map from storage to its record for this thread. Its destructor runs at thread exit
        // and merges the completed samples of every live storage into the finished map.
        struct ThreadCache
        {
            std::unordered_map<const PerformanceStorage*, ThreadCacheEntry> entries;

            ~ThreadCache()
            {
                for (std::unordered_map<const PerformanceStorage*, ThreadCacheEntry>::iterator it = entries.begin(); it != entries.end(); ++it)
                    Detach(it->second);
            }
        };

        static void MergeInto(FunctionMap& target, const FunctionMap& source)
        {
            for (FunctionMap::const_iterator function = source.begin(); function != source.end(); ++function)
            {
                FunctionMap::iterator existing = target.find(function->first);
                if (existing == target.end())
                    target[function->first].reset(new PerformanceMeasurer(*function->second));
                else
                    existing->second->Merge(*function->second);
            }
        }

        static void MergeNamed(PerformanceMeasurer& merged, const FunctionMap& source, const String& name)
        {
            FunctionMap::const_iterator function = source.find(name);
            if (function == source.end() || function->second->Average() == 0)
                return;
            if (merged.Average() == 0)
                merged = *function->second;
            else
                merged.Merge(*function->second);
        }

        // A storage created at the address of a destroyed one must not reuse the old entry, so the
        // entry has to point at this storage's own state, not merely at a state that is still alive.
        CPL_INLINE bool OwnsEntry(const ThreadCacheEntry& entry) const
        {
            return !entry.state.owner_before(_state) && !_state.owner_before(entry.state) && !entry.state.expired();
        }

        static void Detach(const ThreadCacheEntry& entry)
        {
            std::shared_ptr<State> state = entry.state.lock();
            if (!state)
                return;
            std::lock_guard<std::mutex> lock(state->mutex);
            {
                std::lock_guard<std::mutex> threadLock(entry.data->mutex);
                MergeInto(state->finished, entry.data->map);
            }
            state->threads.erase(entry.key);
        }

        CPL_INLINE ThreadData& ThisThread()
        {
            static thread_local ThreadCache cache;
            std::unordered_map<const PerformanceStorage*, ThreadCacheEntry>::iterator it = cache.entries.find(this);
            if (it != cache.entries.end() && OwnsEntry(it->second))
                return *it->second.data;
            for (std::unordered_map<const PerformanceStorage*, ThreadCacheEntry>::iterator stale = cache.entries.begin(); stale != cache.entries.end();)
            {
                if (stale->second.state.expired())
                    stale = cache.entries.erase(stale);
                else
                    ++stale;
            }
            std::lock_guard<std::mutex> lock(_state->mutex);
            ThreadCacheEntry entry;
            entry.state = _state;
            entry.key = _state->registration++;
            entry.data = std::make_shared<ThreadData>();
            _state->threads[entry.key] = entry.data;
            cache.entries[this] = entry;
            return *entry.data;
        }
    };
}

/*! @ingroup cpl_performance
* \def CPL_PERF_FUNCFH(flop, hist)
* \brief Times the remainder of the current function and records flop and a histogram.
* \param flop - Floating-point operations performed by one call.
* \param hist - Histogram bin count. 0 disables quantile statistics.
*/
#define CPL_PERF_FUNCFH(flop, hist) Cpl::PerformanceHolder CPL_CAT(__ph, __LINE__)(Cpl::PerformanceStorage::Global().Get(CPL_FUNCTION, (int64_t)(flop), (hist)))

/*! @ingroup cpl_performance
* \def CPL_PERF_FUNCF(flop)
* \brief Times the remainder of the current function and records flop. No histogram is collected.
* \param flop - Floating-point operations performed by one call.
*/
#define CPL_PERF_FUNCF(flop) CPL_PERF_FUNCFH(flop, 0)

/*! @ingroup cpl_performance
* \def CPL_PERF_FUNC()
* \brief Times the remainder of the current function. No flop count or histogram is collected.
*/
#define CPL_PERF_FUNC() CPL_PERF_FUNCFH(0, 0)

/*! @ingroup cpl_performance
* \def CPL_PERF_BEGFH(desc, flop, hist)
* \brief Times the current scope as a named block of the current function and records flop and a histogram.
*        The stored name is `CPL_FUNCTION{ desc }`.
* \param desc - Block description.
* \param flop - Floating-point operations performed by one sample.
* \param hist - Histogram bin count. 0 disables quantile statistics.
*/
#define CPL_PERF_BEGFH(desc, flop, hist) Cpl::PerformanceHolder CPL_CAT(__ph, __LINE__)(Cpl::PerformanceStorage::Global().Get(CPL_FUNCTION, desc, (int64_t)(flop), (hist)))

/*! @ingroup cpl_performance
* \def CPL_PERF_BEGF(desc, flop)
* \brief Times the current scope as a named block of the current function and records flop. No histogram is collected.
* \param desc - Block description.
* \param flop - Floating-point operations performed by one sample.
*/
#define CPL_PERF_BEGF(desc, flop) CPL_PERF_BEGFH(desc, flop, 0)

/*! @ingroup cpl_performance
* \def CPL_PERF_BEG(desc)
* \brief Times the current scope as a named block of the current function.
* \param desc - Block description.
*/
#define CPL_PERF_BEG(desc) CPL_PERF_BEGFH(desc, 0, 0)

/*! @ingroup cpl_performance
* \def CPL_PERF_IFFH(cond, desc, flop, hist)
* \brief Times the current scope as a named block when cond is true. Records flop and a histogram.
* \param cond - Condition that must be true to start measuring.
* \param desc - Block description.
* \param flop - Floating-point operations performed by one sample.
* \param hist - Histogram bin count. 0 disables quantile statistics.
*/
#define CPL_PERF_IFFH(cond, desc, flop, hist) Cpl::PerformanceHolder CPL_CAT(__ph, __LINE__)((cond) ? Cpl::PerformanceStorage::Global().Get(CPL_FUNCTION, desc, (int64_t)(flop), (hist)) : NULL)

/*! @ingroup cpl_performance
* \def CPL_PERF_IFF(cond, desc, flop)
* \brief Times the current scope as a named block when cond is true. Records flop. No histogram is collected.
* \param cond - Condition that must be true to start measuring.
* \param desc - Block description.
* \param flop - Floating-point operations performed by one sample.
*/
#define CPL_PERF_IFF(cond, desc, flop) CPL_PERF_IFFH(cond, desc, flop, 0)

/*! @ingroup cpl_performance
* \def CPL_PERF_IF(cond, desc)
* \brief Times the current scope as a named block when cond is true.
* \param cond - Condition that must be true to start measuring.
* \param desc - Block description.
*/
#define CPL_PERF_IF(cond, desc) CPL_PERF_IFFH(cond, desc, 0, 0)

/*! @ingroup cpl_performance
* \def CPL_PERF_END(desc)
* \brief Commits the current sample of the named block of the current function.
* \param desc - Block description previously used with CPL_PERF_BEG or CPL_PERF_INIT.
*/
#define CPL_PERF_END(desc) Cpl::PerformanceStorage::Global().Get(CPL_FUNCTION, desc)->Leave();

/*! @ingroup cpl_performance
* \def CPL_PERF_INITFH(name, desc, flop, hist)
* \brief Creates a PerformanceHolder for a named block without starting a sample.
*        Use CPL_PERF_START and CPL_PERF_PAUSE to measure selected intervals.
* \param name - Identifier of the holder variable.
* \param desc - Block description.
* \param flop - Floating-point operations performed by one sample.
* \param hist - Histogram bin count. 0 disables quantile statistics.
*/
#define CPL_PERF_INITFH(name, desc, flop, hist) Cpl::PerformanceHolder name(Cpl::PerformanceStorage::Global().Get(CPL_FUNCTION, desc, (int64_t)(flop), (hist)), false);

/*! @ingroup cpl_performance
* \def CPL_PERF_INITF(name, desc, flop)
* \brief Creates a PerformanceHolder for a named block without starting a sample. No histogram is collected.
* \param name - Identifier of the holder variable.
* \param desc - Block description.
* \param flop - Floating-point operations performed by one sample.
*/
#define CPL_PERF_INITF(name, desc, flop) CPL_PERF_INITFH(name, desc, flop, 0);

/*! @ingroup cpl_performance
* \def CPL_PERF_INIT(name, desc)
* \brief Creates a PerformanceHolder for a named block without starting a sample.
* \param name - Identifier of the holder variable.
* \param desc - Block description.
*/
#define CPL_PERF_INIT(name, desc) CPL_PERF_INITFH(name, desc, 0, 0);

/*! @ingroup cpl_performance
* \def CPL_PERF_START(name)
* \brief Starts or resumes a sample on a holder created by CPL_PERF_INIT.
* \param name - Holder variable created by CPL_PERF_INIT, CPL_PERF_INITF or CPL_PERF_INITFH.
*/
#define CPL_PERF_START(name) name.Enter(); 

/*! @ingroup cpl_performance
* \def CPL_PERF_PAUSE(name)
* \brief Pauses a sample on a holder created by CPL_PERF_INIT without committing it.
*        Time spent after this call is excluded until the next CPL_PERF_START.
* \param name - Holder variable created by CPL_PERF_INIT, CPL_PERF_INITF or CPL_PERF_INITFH.
*/
#define CPL_PERF_PAUSE(name) name.Leave(true);

#else

#define CPL_PERF_FUNCFH(flop, hist)
#define CPL_PERF_FUNCF(flop)
#define CPL_PERF_FUNC()

#define CPL_PERF_BEGFH(desc, flop, hist)
#define CPL_PERF_BEGF(desc, flop)
#define CPL_PERF_BEG(desc)

#define CPL_PERF_IFFH(cond, desc, flop, hist)
#define CPL_PERF_IFF(cond, desc, flop)
#define CPL_PERF_IF(cond, desc)

#define CPL_PERF_END(desc)

#define CPL_PERF_INITFH(name, desc, flop, hist)
#define CPL_PERF_INITF(name, desc, flop)
#define CPL_PERF_INIT(name, desc)

#define CPL_PERF_START(name)
#define CPL_PERF_PAUSE(name)

#endif
