#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest/doctest.h"
#include "unicode_utils.hpp"

using namespace purecpp;

// 测试unicode_to_utf8函数
TEST_CASE("unicode_to_utf8 Tests") {
    SUBCASE("ASCII character") {
        std::string result = unicode_to_utf8(0x41); // 'A'
        CHECK(result == "A");
    }
    
    SUBCASE("2-byte UTF-8") {
        std::string result = unicode_to_utf8(0x00A0); // 不间断空格
        CHECK(result.size() == 2);
        CHECK(static_cast<uint8_t>(result[0]) == 0xC2);
        CHECK(static_cast<uint8_t>(result[1]) == 0xA0);
    }
    
    SUBCASE("3-byte UTF-8") {
        std::string result = unicode_to_utf8(0x4E2D); // 汉字 "中"
        CHECK(result.size() == 3);
        CHECK(static_cast<uint8_t>(result[0]) == 0xE4);
        CHECK(static_cast<uint8_t>(result[1]) == 0xB8);
        CHECK(static_cast<uint8_t>(result[2]) == 0xAD);
    }
}

// 测试escape_unicode_to_utf8函数
TEST_CASE("escape_unicode_to_utf8 Tests") {
    SUBCASE("Simple ASCII with no escapes") {
        std::string input = "Hello World!";
        std::string result = escape_unicode_to_utf8(input);
        CHECK(result == input);
    }
    
    SUBCASE("Single unicode escape") {
        std::string input = "Hello \\u4E2D\\u6587 World!";
        std::string result = escape_unicode_to_utf8(input);
        CHECK(result == "Hello 中文 World!");
    }
    
    SUBCASE("Multiple unicode escapes") {
        std::string input = "\\u4F60\\u597D\\uFF0C\\u4E16\\u754C\\uFF01";
        std::string result = escape_unicode_to_utf8(input);
        CHECK(result == "你好，世界！");
    }
    
    SUBCASE("Mixed ASCII and unicode") {
        std::string input = "C++ \\u662F\\u4E00\\u79CD\\u7F16\\u7A0B\\u8BED\\u8A00";
        std::string result = escape_unicode_to_utf8(input);
        CHECK(result == "C++ 是一种编程语言");
    }
    
    SUBCASE("Incomplete escape sequence") {
        std::string input = "Hello \\u4E";
        std::string result = escape_unicode_to_utf8(input);
        CHECK(result == "Hello \\u4E");
    }
    
    SUBCASE("Invalid escape sequence") {
        std::string input = "Hello \\uXXXX World!";
        std::string result = escape_unicode_to_utf8(input);
        // 应该保留原始格式
        CHECK(result == "Hello \\uXXXX World!");
    }
}

// 测试utf8_to_escape_unicode函数
TEST_CASE("utf8_to_escape_unicode Tests") {
    SUBCASE("Simple ASCII") {
        std::string input = "Hello World!";
        std::string result = utf8_to_escape_unicode(input);
        CHECK(result == input);
    }
    
    SUBCASE("Chinese characters") {
        std::string input = "中文";
        std::string result = utf8_to_escape_unicode(input);
        CHECK(result == "\\u4e2d\\u6587");
    }
    
    SUBCASE("Mixed ASCII and Chinese") {
        std::string input = "C++ 是一种编程语言";
        std::string result = utf8_to_escape_unicode(input);
        CHECK(result == "C++ \\u662f\\u4e00\\u79cd\\u7f16\\u7a0b\\u8bed\\u8a00");
    }
    
    SUBCASE("Punctuation") {
        std::string input = "你好，世界！";
        std::string result = utf8_to_escape_unicode(input);
        CHECK(result == "\\u4f60\\u597d\\uff0c\\u4e16\\u754c\\uff01");
    }
}

// 测试双向转换
TEST_CASE("Round trip conversion") {
    SUBCASE("Simple case") {
        std::string original = "Hello 中文 World!";
        std::string escaped = utf8_to_escape_unicode(original);
        std::string unescaped = escape_unicode_to_utf8(escaped);
        CHECK(unescaped == original);
    }
    
    SUBCASE("Complex case") {
        std::string original = "C++ 是一种强大的编程语言！\n你学会了吗？";
        std::string escaped = utf8_to_escape_unicode(original);
        std::string unescaped = escape_unicode_to_utf8(escaped);
        CHECK(unescaped == original);
    }
}
