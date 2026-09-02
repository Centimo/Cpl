/*
* Tests for Common Purpose Library (http://github.com/ermig1979/Cpl).
*
* Copyright (c) 2021-2021 Yermalayeu Ihar.
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

#include "Cpl/Yaml.h"
#include "Cpl/Param.h"

namespace Test
{
    bool YamlSimpleTest(const Options& options)
    {
        const std::string data =
            "data1: \n"
            "  123\n"
            "data2: Hello world\n"
            "data3:\n"
            "   - key1: 123\n"
            "     key2: Test\n"
            "   - Hello world\n"
            "   - 123\n"
            "   - 123.4\n";

        Cpl::Yaml::Node root;
        try
        {
            Cpl::Yaml::Parse(root, data);
        }
        catch (const Cpl::Yaml::Exception e)
        {
            std::cout << "Exception " << e.GetType() << ": " << e.what() << std::endl;
            return false;
        }

        std::cout << root["data1"].As<int>(0) << std::endl;
        std::cout << root["data2"].As<std::string>() << std::endl;
        std::cout << root["data3"][0]["key1"].As<int>(0) << std::endl;
        std::cout << root["data3"][0]["key2"].As<std::string>() << std::endl;
        std::cout << root["data3"][1].As<std::string>() << std::endl;
        std::cout << root["data3"][2].As<int>(0) << std::endl;
        std::cout << root["data3"][3].As<float>(0.0f) << std::endl;
        return true;
    }

    //---------------------------------------------------------------------------------------------

    bool YamlParamTest(const Options& options)
    {
        struct SubParam
        {
            CPL_PARAM_VALUE(Int, id, 1);
            CPL_PARAM_VALUE(String, desc, "no");
        };

        struct TestParam
        {
            CPL_PARAM_VALUE(String, name, "Name");
            CPL_PARAM_VALUE(Int, value, 0);
            CPL_PARAM_VALUE(Strings, letters, Strings({ "A", "B", "C" }));
            CPL_PARAM_STRUCT(SubParam, sub);
            CPL_PARAM_STRUCT(SubParam, orig);
            CPL_PARAM_LIMITED(Int, lim, 3, 0, 5);
            CPL_PARAM_VECTOR(SubParam, subs);
            CPL_PARAM_MAP(String, SubParam, dict);
        };

        CPL_PARAM_HOLDER(TestParamHolder, TestParam, test);

        TestParamHolder test, loaded;

        test().name() = "Changed";
        test().sub().desc() = "description";
        test().lim() = 4;
        test().subs().resize(3);
        test().subs()[0].id() = 7;
        test().subs()[1].desc() = "seven";
        test().dict()["A"].desc() = "A";
        test().dict()["B"];

        test.Save(options.OutputPath("yaml_short.yml"), false, Cpl::ParamFormatYaml);

        test.Save(options.OutputPath("yaml_full.yml"), true, Cpl::ParamFormatYaml);

        if (!loaded.Load(options.OutputPath("yaml_short.yml"), Cpl::ParamFormatYaml))
            return false;
        if (!loaded.Equal(test))
        {
            CPL_LOG_SS(Error, "loaded short != original");
            loaded.Save(options.OutputPath("yaml_short_loaded.yml"), false, Cpl::ParamFormatYaml);
            loaded.Save(options.OutputPath("yaml_full_loaded.yml"), true, Cpl::ParamFormatYaml);
            return false;
        }

        if (!loaded.Load(options.OutputPath("yaml_full.yml"), Cpl::ParamFormatYaml))
            return false;
        if (!loaded.Equal(test))
        {
            CPL_LOG_SS(Error, "loaded full != original");
            loaded.Save(options.OutputPath("yaml_short_loaded.yml"), false, Cpl::ParamFormatYaml);
            loaded.Save(options.OutputPath("yaml_full_loaded.yml"), true, Cpl::ParamFormatYaml);
            return false;
        }

        return true;
    }

    //---------------------------------------------------------------------------------------------

    static bool ParseYaml(Cpl::Yaml::Node& root, const std::string& text)
    {
        try
        {
            Cpl::Yaml::Parse(root, text);
        }
        catch (const Cpl::Yaml::Exception& e)
        {
            CPL_LOG_SS(Error, "Parse failed with exception " << e.GetType() << ": " << e.what() << " for text: " << text);
            return false;
        }
        return true;
    }

    static bool SerializeYaml(const Cpl::Yaml::Node& root, std::string& text)
    {
        try
        {
            Cpl::Yaml::Serialize(root, text);
        }
        catch (const Cpl::Yaml::Exception& e)
        {
            CPL_LOG_SS(Error, "Serialize failed with exception " << e.GetType() << ": " << e.what());
            return false;
        }
        return true;
    }

    static const char* TypeName(Cpl::Yaml::Node::eType type)
    {
        switch (type)
        {
        case Cpl::Yaml::Node::None: return "None";
        case Cpl::Yaml::Node::SequenceType: return "Sequence";
        case Cpl::Yaml::Node::MapType: return "Map";
        case Cpl::Yaml::Node::ScalarType: return "Scalar";
        }
        return "Unknown";
    }

    static bool CheckScalar(const Cpl::Yaml::Node& node, const std::string& expected, const std::string& what)
    {
        if (!node.IsScalar())
        {
            CPL_LOG_SS(Error, what << ": expected scalar '" << expected << "', got node of type " << TypeName(node.Type()));
            return false;
        }
        const std::string actual = node.As<std::string>();
        if (actual != expected)
        {
            CPL_LOG_SS(Error, what << ": expected '" << expected << "', got '" << actual << "'");
            return false;
        }
        return true;
    }

    bool YamlSequenceIteratorKeyTest(const Options& options)
    {
        return RunIsolated([]() -> bool
        {
            Cpl::Yaml::Node root;
            root.PushBack() = "a";
            root.PushBack() = "b";

            for (auto it = root.Begin(); it != root.End(); it++)
            {
                const std::string& key = (*it).first;
                if (!key.empty())
                {
                    CPL_LOG_SS(Error, "Sequence item key must be empty, got '" << key << "'");
                    return false;
                }
            }

            const Cpl::Yaml::Node& constRoot = root;
            for (auto it = constRoot.Begin(); it != constRoot.End(); it++)
            {
                const std::string& key = (*it).first;
                if (!key.empty())
                {
                    CPL_LOG_SS(Error, "Sequence item key (const iterator) must be empty, got '" << key << "'");
                    return false;
                }
            }
            return true;
        });
    }

    bool YamlSequencePushFrontTest(const Options& options)
    {
        return RunIsolated([]() -> bool
        {
            Cpl::Yaml::Node root;
            root.PushBack() = "a";
            root.PushFront() = "b";
            if (root.Size() != 2)
            {
                CPL_LOG_SS(Error, "Expected size 2, got " << root.Size());
                return false;
            }
            return CheckScalar(root[0], "b", "root[0]") && CheckScalar(root[1], "a", "root[1]");
        });
    }

    bool YamlSequenceInsertTest(const Options& options)
    {
        const bool middle = RunIsolated([]() -> bool
        {
            Cpl::Yaml::Node root;
            root.PushBack() = "a";
            root.PushBack() = "b";
            root.PushBack() = "c";
            root.Insert(1) = "x";
            if (root.Size() != 4)
            {
                CPL_LOG_SS(Error, "Insert(1): expected size 4, got " << root.Size());
                return false;
            }
            return CheckScalar(root[0], "a", "root[0]") && CheckScalar(root[1], "x", "root[1]")
                && CheckScalar(root[2], "b", "root[2]") && CheckScalar(root[3], "c", "root[3]");
        });
        const bool append = RunIsolated([]() -> bool
        {
            Cpl::Yaml::Node root;
            root.PushBack() = "a";
            root.PushBack() = "b";
            root.Insert(5) = "x";
            if (root.Size() != 3)
            {
                CPL_LOG_SS(Error, "Insert(5): expected size 3, got " << root.Size());
                return false;
            }
            return CheckScalar(root[2], "x", "root[2]");
        });
        return middle && append;
    }

    bool YamlNodeAssignFromChildTest(const Options& options)
    {
        return RunIsolated([]() -> bool
        {
            Cpl::Yaml::Node root;
            root["child"]["key"] = "value";
            root = root["child"];
            if (!root.IsMap() || root.Size() != 1)
            {
                CPL_LOG_SS(Error, "Expected a map with one entry, got type " << TypeName(root.Type()) << " with size " << root.Size());
                return false;
            }
            return CheckScalar(root["key"], "value", "root[\"key\"]");
        });
    }

    bool YamlEmptyNodeSharedTest(const Options& options)
    {
        return RunIsolated([]() -> bool
        {
            Cpl::Yaml::Node a;
            a.PushBack() = "a0";
            a[5] = "x";

            Cpl::Yaml::Node b;
            b.PushBack() = "b0";
            const Cpl::Yaml::Node& outOfRange = b[7];
            if (!outOfRange.IsNone())
            {
                CPL_LOG_SS(Error, "b[7] must be an empty node, got type " << TypeName(outOfRange.Type())
                    << " with value '" << outOfRange.As<std::string>() << "'");
                return false;
            }
            if (a.Size() != 1 || b.Size() != 1)
            {
                CPL_LOG_SS(Error, "Sizes changed: a " << a.Size() << ", b " << b.Size());
                return false;
            }
            return true;
        });
    }

    bool YamlIteratorOfScalarTest(const Options& options)
    {
        return RunIsolated([]() -> bool
        {
            Cpl::Yaml::Node scalar;
            scalar = "abc";
            size_t count = 0;
            for (auto it = scalar.Begin(); it != scalar.End(); it++)
            {
                if (++count > 10)
                    break;
            }
            if (count != 0)
            {
                CPL_LOG_SS(Error, "Iterating a scalar node visited " << count << " items, expected 0");
                return false;
            }

            Cpl::Yaml::Node none;
            count = 0;
            for (auto it = none.Begin(); it != none.End(); it++)
            {
                if (++count > 10)
                    break;
            }
            if (count != 0)
            {
                CPL_LOG_SS(Error, "Iterating an empty node visited " << count << " items, expected 0");
                return false;
            }

            const Cpl::Yaml::Node& constScalar = scalar;
            count = 0;
            for (auto it = constScalar.Begin(); it != constScalar.End(); it++)
            {
                if (++count > 10)
                    break;
            }
            if (count != 0)
            {
                CPL_LOG_SS(Error, "Iterating a scalar node (const) visited " << count << " items, expected 0");
                return false;
            }
            return true;
        });
    }

    bool YamlPlainScalarWithQuoteTest(const Options& options)
    {
        Cpl::Yaml::Node root;
        if (!ParseYaml(root, "desc: it's fine\n"))
            return false;
        if (!CheckScalar(root["desc"], "it's fine", "desc"))
            return false;

        if (!ParseYaml(root, "desc: say \"hi\" now\n"))
            return false;
        if (!CheckScalar(root["desc"], "say \"hi\" now", "desc"))
            return false;

        if (!ParseYaml(root, "- it's fine\n"))
            return false;
        return CheckScalar(root[0], "it's fine", "[0]");
    }

    bool YamlSerializeQuotedValueTest(const Options& options)
    {
        const std::string value = "say \"hi\"";
        Cpl::Yaml::Node root;
        root["a"] = value;

        std::string text;
        if (!SerializeYaml(root, text))
            return false;
        if (text.find("\\\"") == std::string::npos)
        {
            CPL_LOG_SS(Error, "Serialized text must escape the inner quotes, got: " << text);
            return false;
        }

        Cpl::Yaml::Node loaded;
        if (!ParseYaml(loaded, text))
            return false;
        return CheckScalar(loaded["a"], value, "a");
    }

    bool YamlBlockScalarQuotesTest(const Options& options)
    {
        Cpl::Yaml::Node root;
        if (!ParseYaml(root, "key: |\n  \"abc\n"))
            return false;
        const std::string literal = root["key"].As<std::string>();
        if (literal != "\"abc\n" && literal != "\"abc")
        {
            CPL_LOG_SS(Error, "Literal block must keep its quote characters: expected '\"abc', got '" << literal << "'");
            return false;
        }

        if (!ParseYaml(root, "key: >\n  'x' and 'y'\n"))
            return false;
        const std::string folded = root["key"].As<std::string>();
        if (folded != "'x' and 'y'\n" && folded != "'x' and 'y'")
        {
            CPL_LOG_SS(Error, "Folded block must keep its quote characters: expected \"'x' and 'y'\", got '" << folded << "'");
            return false;
        }
        return true;
    }

    bool YamlSequenceEraseTest(const Options& options)
    {
        Cpl::Yaml::Node root;
        root.PushBack() = "a";
        root.PushBack() = "b";
        root.PushBack() = "c";
        root.Erase(0);
        if (root.Size() != 2)
        {
            CPL_LOG_SS(Error, "Expected size 2, got " << root.Size());
            return false;
        }
        if (!CheckScalar(root[0], "b", "root[0]") || !CheckScalar(root[1], "c", "root[1]"))
            return false;

        size_t count = 0;
        for (auto it = root.Begin(); it != root.End(); it++)
            count++;
        if (count != 2)
        {
            CPL_LOG_SS(Error, "Iteration visited " << count << " items, expected 2");
            return false;
        }
        return true;
    }

    bool YamlNestedInlineSequenceTest(const Options& options)
    {
        Cpl::Yaml::Node root;
        if (!ParseYaml(root, "- - a\n  - b\n- c\n"))
            return false;
        if (!root.IsSequence() || root.Size() != 2)
        {
            CPL_LOG_SS(Error, "Expected a sequence of 2, got type " << TypeName(root.Type()) << " of size " << root.Size());
            return false;
        }
        if (!root[0].IsSequence() || root[0].Size() != 2)
        {
            CPL_LOG_SS(Error, "root[0] must be a sequence of 2, got type " << TypeName(root[0].Type())
                << " with value '" << root[0].As<std::string>() << "'");
            return false;
        }
        return CheckScalar(root[0][0], "a", "root[0][0]") && CheckScalar(root[0][1], "b", "root[0][1]")
            && CheckScalar(root[1], "c", "root[1]");
    }

    bool YamlParseDirectoryTest(const Options& options)
    {
        const std::string directory = options.output;
        return RunIsolated([directory]() -> bool
        {
            Cpl::Yaml::Node root;
            try
            {
                Cpl::Yaml::Parse(root, directory.c_str());
            }
            catch (const Cpl::Yaml::Exception& e)
            {
                if (e.GetType() == Cpl::Yaml::Exception::OperationError)
                    return true;
                CPL_LOG_SS(Error, "Expected OperationException for directory '" << directory << "', got type " << e.GetType() << ": " << e.what());
                return false;
            }
            catch (const std::exception& e)
            {
                CPL_LOG_SS(Error, "Expected Cpl::Yaml::Exception for directory '" << directory << "', got " << e.what());
                return false;
            }
            CPL_LOG_SS(Error, "Parsing directory '" << directory << "' must throw Cpl::Yaml::Exception, got a node of type " << TypeName(root.Type()));
            return false;
        });
    }

    bool YamlSerializeMultilineScalarTest(const Options& options)
    {
        const std::string value = "line1\nline2\n";

        Cpl::Yaml::Node root;
        root = value;
        std::string text;
        if (!SerializeYaml(root, text))
            return false;
        Cpl::Yaml::Node loaded;
        if (!ParseYaml(loaded, text) || !CheckScalar(loaded, value, "root scalar"))
        {
            CPL_LOG_SS(Error, "Serialized root scalar: " << text);
            return false;
        }

        Cpl::Yaml::Node map;
        map["a"] = value;
        if (!SerializeYaml(map, text))
            return false;
        if (!ParseYaml(loaded, text) || !CheckScalar(loaded["a"], value, "map value"))
        {
            CPL_LOG_SS(Error, "Serialized map: " << text);
            return false;
        }
        return true;
    }
}
