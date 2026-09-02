/*
* Tests for Common Purpose Library (http://github.com/ermig1979/Cpl).
*
* Copyright (c) 2021-2026 Yermalayeu Ihar,
*               2021-2022 Andrey Drogolyub.
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

#include "Cpl/Args.h"

namespace Test
{
    struct TestArgs : public Cpl::ArgsParser
    {
        TestArgs(int argc, char* argv[])
            : Cpl::ArgsParser(argc, argv, true) {}

        bool Check(const Cpl::String& name) { return HasArg(name); }
        bool Check(const String& name0, const String& name1) { return HasArg(name0, name1); }
    };

    bool HasArgTest(const Options& options)
    {
        int argc = 2;
        std::vector<std::string> args = {"test", "-h"};

        char* argv[] = { const_cast<char*>(args[0].c_str()), const_cast<char*>(args[1].c_str()) };

        TestArgs a(argc, argv);

        return a.Check("-h");
    }   

    bool HasArg1Test(const Options& options)
    {
        int argc = 2;
        std::vector<std::string> args = { "test", "-h" };

        char* argv[] = { const_cast<char*>(args[0].c_str()), const_cast<char*>(args[1].c_str()) };

        TestArgs a(argc, argv);

        return !a.Check("-g");
    }

    bool HasArg2Test(const Options& options)
    {
        int argc = 2;
        std::vector<std::string> args = { "test", "-h" };

        char* argv[] = { const_cast<char*>(args[0].c_str()), const_cast<char*>(args[1].c_str()) };

        TestArgs a(argc, argv);

        return a.Check("-h", "-?");
    }

    struct TestArgsGetter : public Cpl::ArgsParser
    {
        TestArgsGetter(int argc, char* argv[])
            : Cpl::ArgsParser(argc, argv, false) {}

        Strings Values(const Strings& names) { return GetArgs(names, Strings(), false); }
    };

    bool ArgsAliasPrefixConsumesTokenTest(const Options& options)
    {
        std::vector<std::string> args = { "test", "-help", "value1", "value2" };
        std::vector<char*> argv;
        for (auto& a : args)
            argv.push_back(const_cast<char*>(a.c_str()));

        TestArgsGetter parser((int)argv.size(), argv.data());
        Strings values = parser.Values({ "-h", "-help" });

        if (values.size() != 1 || values[0] != "value1")
        {
            std::stringstream got;
            for (size_t i = 0; i < values.size(); ++i)
                got << (i ? ", " : "") << "\"" << values[i] << "\"";
            CPL_LOG_SS(Error, "GetArgs({\"-h\",\"-help\"}) on a single \"-help value1 value2\" token expected "
                << "{\"value1\"}, got {" << got.str() << "}");
            return false;
        }
        return true;
    }
}



