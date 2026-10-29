#pragma once
#include <string>
#include <vector>

using namespace std;

// 2번: 바이트와 한글 음절을 구분하는 공통 도구.
namespace Utf8Util {
    bool decode(const string& text, vector<char32_t>& output);
    bool isHangul(char32_t codePoint);
    string encode(char32_t codePoint);
    string trimAscii(const string& text);
    bool wordParts(const string& word, char32_t& first,
                   char32_t& last, size_t& syllableCount);
}

