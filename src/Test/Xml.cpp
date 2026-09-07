/*
* Tests for Common Purpose Library (http://github.com/ermig1979/Cpl).
*
* Copyright (c) 2021-2026 Yermalayeu Ihar,
*               2023-2023 Daniil Germanenko.
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

#include "Cpl/Xml.h"
#include <cstring>
#include <iostream>
#include <sstream>
#include <string>

namespace Test
{
    using MemPool = Cpl::Xml::MemoryPool<char>;
    bool XmlAllocateStringTest(const Options& options)
    {
        MemPool pool;

        constexpr int defaultSize = 8;
        {
            auto t1 = pool.AllocateString(nullptr, defaultSize);
            if (t1[defaultSize] != 0)
                return false;

        }

        const std::string testString("Some test string in 34 byte length");
        { 

            auto t1 = pool.AllocateString(testString.data(), defaultSize);

            for (size_t i = 0; i < defaultSize; i++){
                if (t1[i] != testString[i])
                    return false;
            }
            
            if (t1[defaultSize] != 0)
                return false;

            auto t2 = pool.AllocateString(testString.data(), defaultSize);

            for (size_t i = 0; i < defaultSize; i++){
                if (t1[i] != testString[i])
                    return false;
            }
            
            if (t2[defaultSize] != 0)
                return false;
            //std::cout << (void*) t1 << " "<<  (void*) t2 << "XmlAllocateStringTest" << std::endl;

            if ((void*)t1 == (void*)t2)
                return false;

        }

        {
            auto t1 = pool.AllocateString(testString.c_str(), testString.size());

            for (size_t i = 0; i < testString.size(); i++){
                if (t1[i] != testString[i])
                    return false;
            }

            if (t1[testString.size()] != 0)
                return false;

            auto t2 = pool.AllocateString(testString.c_str(), testString.size() + 1);

            for (size_t i = 0; i < testString.size(); i++){
                if (t2[i] != testString[i])
                    return false;
            }

            if (t2[testString.size()] != 0)
                return false;

        }

        {
            //source null terminated, size undefined
            auto t1 = pool.AllocateString(testString.c_str(), 0);

            for (size_t i = 0; i < testString.size(); i++){
                if (t1[i] != testString[i])
                    return false;
            }

            if (t1[testString.size()] != 0)
                return false;

        }

        {
            const int index = 6;
            auto str = testString;
            str[index] = 0;

            auto t1 = pool.AllocateString(str.c_str(), 0);

            for (size_t i = 0; i < index; i++){
                if (t1[i] != str[i])
                    return false;
            }

            if (t1[index] != 0)
                return false;

        }

        {
            const int size = 6;
            char* temp = (char*) malloc(size);
            for (size_t i = 0; i < size; i++){
                temp[i] = char(10 + i);
            }
            
            auto t1 = pool.AllocateString(temp, size);

            for (size_t i = 0; i < size; i++){
                 if (t1[i] != temp[i])
                     return false;
            }

            if (t1[size] != 0)
                return false;

            free(temp);
        }

        return true;
    }

    bool XmlIteratorTest(const Options& options)
    {
        using namespace Cpl::Xml;

        std::string xml = "<root a1=\"1\" a2=\"2\" a3=\"3\"><c1/><c2/><c3/></root>";
        std::vector<char> buf(xml.begin(), xml.end());
        buf.push_back('\0');

        XmlDocument<char> doc;
        doc.Parse<0>(buf.data(), buf.size());
        XmlNode<char>* root = doc.FirstNode();
        if (!root)
            return false;

        // NodeIterator: postfix ++ returns old value, advances iterator
        {
            NodeIterator<char> it(root);
            std::string first = it->Name();
            NodeIterator<char> prev = it++;
            std::string second = it->Name();

            if (std::string(prev->Name()) != first)
                return false;
            if (first == second)
                return false;
            if (first != "c1" || second != "c2")
                return false;
        }

        // NodeIterator: postfix -- returns old value, goes backward
        {
            NodeIterator<char> it(root);
            ++it; // c2
            ++it; // c3
            std::string third = it->Name();
            NodeIterator<char> prev = it--;
            std::string second = it->Name();

            if (std::string(prev->Name()) != third)
                return false;
            if (third != "c3" || second != "c2")
                return false;
        }

        // AttributeIterator: postfix ++ returns old value, advances iterator
        {
            AttributeIterator<char> it(root);
            std::string first = it->Name();
            AttributeIterator<char> prev = it++;
            std::string second = it->Name();

            if (std::string(prev->Name()) != first)
                return false;
            if (first == second)
                return false;
            if (first != "a1" || second != "a2")
                return false;
        }

        // AttributeIterator: postfix -- returns old value, goes backward
        {
            AttributeIterator<char> it(root);
            ++it; // a2
            ++it; // a3
            std::string third = it->Name();
            AttributeIterator<char> prev = it--;
            std::string second = it->Name();

            if (std::string(prev->Name()) != third)
                return false;
            if (third != "a3" || second != "a2")
                return false;
        }

        return true;
    }

    bool XmlCDataPrefixTest(const Options& options)
    {
        using namespace Cpl::Xml;

        std::string xml = "<r><![CDATA[hi]]></r>";
        std::vector<char> buf(xml.begin(), xml.end());
        buf.push_back('\0');

        XmlDocument<char> doc;
        doc.Parse<0>(buf.data(), buf.size());
        XmlNode<char>* root = doc.FirstNode();
        if (!root)
        {
            CPL_LOG_SS(Error, "expected a root node");
            return false;
        }

        XmlNode<char>* cdata = root->FirstNode();
        if (!cdata)
        {
            CPL_LOG_SS(Error, "expected a CDATA child node");
            return false;
        }

        std::string value(cdata->Value(), cdata->ValueSize());
        if (value != "hi")
        {
            CPL_LOG_SS(Error, "expected CDATA value 'hi', got '" << value << "'");
            return false;
        }

        std::ostringstream oss;
        Print(static_cast<std::ostream&>(oss), doc);
        std::string printed = oss.str();
        if (printed.find("<![CDATA[hi]]>") == std::string::npos)
        {
            CPL_LOG_SS(Error, "expected re-printed document to contain '<![CDATA[hi]]>', got '" << printed << "'");
            return false;
        }

        return true;
    }

    bool XmlTrimWhitespaceUnderflowTest(const Options& options)
    {
        using namespace Cpl::Xml;

        return RunIsolated([]() -> bool
        {
            std::string xml = "<a> &#32;</a>";
            std::vector<char> buf(xml.begin(), xml.end());
            buf.push_back('\0');

            XmlDocument<char> doc;
            doc.Parse<ParseTrimWhitespace>(buf.data(), buf.size());
            XmlNode<char>* root = doc.FirstNode();
            if (!root)
            {
                CPL_LOG_SS(Error, "expected a root node");
                return false;
            }

            if (root->ValueSize() != 0)
            {
                CPL_LOG_SS(Error, "expected an empty value after trimming, got ValueSize() == " << root->ValueSize());
                return false;
            }
            return true;
        }, 1000);
    }

    bool XmlFileOpenDirectoryTest(const Options& options)
    {
        using namespace Cpl::Xml;

        return RunIsolated([&options]() -> bool
        {
            File<char> file;
            bool opened = file.Open(options.output.c_str());

            if (opened)
            {
                CPL_LOG_SS(Error, "Open() of directory '" << options.output << "' must fail, got success with " << file.Size() << " bytes");
                return false;
            }
            return true;
        }, 1000);
    }

    bool XmlUninitializedAttributeLinksTest(const Options& options)
    {
        using namespace Cpl::Xml;

        return RunIsolated([]() -> bool
        {
            // Pool memory is filled with 0xFF and reused, so fields the constructor skips hold garbage.
            MemPool pool;
            const size_t garbageSize = 256;
            char* garbage = pool.AllocateString(nullptr, garbageSize);
            std::memset(garbage, 0xFF, garbageSize + 1);
            pool.Clear();

            XmlAttribute<char>* attribute = pool.AllocateAttribute("a", "1", 1, 1);

            XmlAttribute<char>* found = attribute->NextAttribute("b");
            if (found != nullptr)
            {
                CPL_LOG_SS(Error, "NextAttribute(\"b\") on an unattached attribute must return NULL");
                return false;
            }
            found = attribute->PreviousAttribute("b");
            if (found != nullptr)
            {
                CPL_LOG_SS(Error, "PreviousAttribute(\"b\") on an unattached attribute must return NULL");
                return false;
            }
            return true;
        }, 1000);
    }

    bool XmlAllocatorNullCrashTest(const Options& options)
    {
        using namespace Cpl::Xml;

        return RunIsolated([]() -> bool
        {
            MemPool pool;
            pool.SetAllocator(+[](size_t) -> void* { return nullptr; }, +[](void*) {});

            try
            {
                if (pool.AllocateString(nullptr, DYNAMIC_POOL_SIZE + 16) == nullptr)
                    return true;
            }
            catch (const std::bad_alloc&)
            {
                return true;
            }
            CPL_LOG_SS(Error, "AllocateString must fail when the allocator returns NULL");
            return false;
        }, 1000);
    }

    bool XmlDecimalReferenceHexDigitTest(const Options& options)
    {
        using namespace Cpl::Xml;

        std::string xml = "<a>&#1f;</a>";
        std::vector<char> buf(xml.begin(), xml.end());
        buf.push_back('\0');

        XmlDocument<char> doc;
        bool threw = false;
        try
        {
            doc.Parse<0>(buf.data(), buf.size());
        }
        catch (const ParseError&)
        {
            threw = true;
        }

        if (!threw)
        {
            CPL_LOG_SS(Error, "expected &#1f; to be rejected as malformed, but it parsed successfully");
            return false;
        }
        return true;
    }

    bool XmlCharacterReferenceOverflowTest(const Options& options)
    {
        using namespace Cpl::Xml;

        // 2^64 + 5: wraps around to 5 in a 64-bit unsigned long accumulator.
        std::string xml = "<a>&#18446744073709551621;</a>";
        std::vector<char> buf(xml.begin(), xml.end());
        buf.push_back('\0');

        XmlDocument<char> doc;
        bool threw = false;
        try
        {
            doc.Parse<0>(buf.data(), buf.size());
        }
        catch (const ParseError&)
        {
            threw = true;
        }

        if (!threw)
        {
            CPL_LOG_SS(Error, "expected an absurdly large &#...; reference to be rejected, but it parsed successfully");
            return false;
        }
        return true;
    }

    bool XmlParseIgnoresLengthTest(const Options& options)
    {
        using namespace Cpl::Xml;

        std::string xml = "<a>1</a><b>2</b>";
        std::vector<char> buf(xml.begin(), xml.end());
        buf.push_back('\0');

        const size_t promisedLength = 5; // "<a>1<" - ends before <b> is even reachable

        XmlDocument<char> doc;
        try
        {
            doc.Parse<0>(buf.data(), promisedLength);
        }
        catch (const ParseError&)
        {
            return true; // rejecting data that runs past the given length is also correct
        }

        if (doc.FirstNode("b") != nullptr)
        {
            CPL_LOG_SS(Error, "Parse() read past the given length and parsed node <b>");
            return false;
        }
        return true;
    }

    bool XmlEndIteratorDecrementTest(const Options& options)
    {
        using namespace Cpl::Xml;

        return RunIsolated([]() -> bool
        {
            std::string xml = "<root a1=\"1\" a2=\"2\" a3=\"3\"><c1/><c2/><c3/></root>";
            std::vector<char> buf(xml.begin(), xml.end());
            buf.push_back('\0');

            XmlDocument<char> doc;
            doc.Parse<0>(buf.data(), buf.size());
            XmlNode<char>* root = doc.FirstNode();
            if (!root)
            {
                CPL_LOG_SS(Error, "expected a root node");
                return false;
            }

            {
                NodeIterator<char> it(root);
                ++it; ++it; ++it;
                --it;
                if (std::string(it->Name()) != "c3")
                {
                    CPL_LOG_SS(Error, "--end() of the node iterator must yield 'c3', got '" << it->Name() << "'");
                    return false;
                }
            }
            {
                AttributeIterator<char> it(root);
                ++it; ++it; ++it;
                --it;
                if (std::string(it->Name()) != "a3")
                {
                    CPL_LOG_SS(Error, "--end() of the attribute iterator must yield 'a3', got '" << it->Name() << "'");
                    return false;
                }
            }
            return true;
        }, 1000);
    }

    bool XmlWideCharClassificationTest(const Options& options)
    {
        using namespace Cpl::Xml;

        // U+0120: not whitespace, but its low byte (0x20) is the ASCII space.
        std::wstring xml = L"<a>" + std::wstring(1, static_cast<wchar_t>(0x120)) + L"</a>";
        std::vector<wchar_t> buf(xml.begin(), xml.end());
        buf.push_back(L'\0');

        XmlDocument<wchar_t> doc;
        doc.Parse<0>(buf.data(), buf.size());
        XmlNode<wchar_t>* root = doc.FirstNode();
        if (!root)
        {
            CPL_LOG_SS(Error, "expected a root node");
            return false;
        }

        if (root->ValueSize() != 1 || root->Value()[0] != static_cast<wchar_t>(0x120))
        {
            CPL_LOG_SS(Error, "expected element value {U+0120}, got size " << root->ValueSize());
            return false;
        }
        return true;
    }

    bool XmlParseErrorWhatTest(const Options& options)
    {
        using namespace Cpl::Xml;

        std::string xml = "<a>";
        std::vector<char> buf(xml.begin(), xml.end());
        buf.push_back('\0');

        XmlDocument<char> doc;
        try
        {
            doc.Parse<0>(buf.data(), buf.size());
        }
        catch (const std::exception& e)
        {
            std::string what = e.what();
            if (what.find("unexpected end of data") == std::string::npos)
            {
                CPL_LOG_SS(Error, "ParseError::what() lost the diagnostic, got '" << what << "'");
                return false;
            }
            return true;
        }

        CPL_LOG_SS(Error, "expected ParseError to be thrown for unterminated '<a>'");
        return false;
    }

    bool XmlDeepNestingStackOverflowTest(const Options& options)
    {
        using namespace Cpl::Xml;

        return RunIsolated([]() -> bool
        {
            const size_t depth = 1000000; // Far beyond any default stack.
            std::string xml;
            xml.reserve(depth * 3);
            for (size_t i = 0; i < depth; ++i)
                xml += "<a>";

            std::vector<char> buf(xml.begin(), xml.end());
            buf.push_back('\0');

            XmlDocument<char> doc;
            try
            {
                doc.Parse<0>(buf.data(), buf.size());
            }
            catch (const ParseError&)
            {
            }
            return true;
        }, 20000);
    }

    // A stream buffer whose every read fails, as a file buffer does on a read error.
    struct XmlFailingStreamBuffer : public std::streambuf
    {
        int_type underflow() override
        {
            throw std::ios_base::failure("read error");
        }
    };

    bool XmlFileStreamReadErrorTest(const Options& options)
    {
        using namespace Cpl::Xml;

        XmlFailingStreamBuffer buffer;
        std::istream stream(&buffer);
        try
        {
            File<char> file(stream);
        }
        catch (const std::runtime_error&)
        {
            return true;
        }
        catch (const std::exception& e)
        {
            CPL_LOG_SS(Error, "File(std::istream&) must report a read error as std::runtime_error, got: " << e.what());
            return false;
        }
        CPL_LOG_SS(Error, "File(std::istream&) must throw on a read error, but it returned normally");
        return false;
    }
}
