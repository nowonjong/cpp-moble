#include "Utf8Util.h"

using namespace std;

bool Utf8Util::decode(const string& text, vector<char32_t>& output) {
    output.clear();
    vector<char32_t> decoded;
    for (size_t i = 0; i < text.size();) {
        const auto lead = static_cast<unsigned char>(text[i]);
        char32_t cp = 0, minimum = 0;
        size_t length = 0;
        if (lead <= 0x7f) { cp = lead; length = 1; }
        else if (lead >= 0xc2 && lead <= 0xdf) {
            cp = lead & 0x1f; length = 2; minimum = 0x80;
        } else if (lead >= 0xe0 && lead <= 0xef) {
            cp = lead & 0x0f; length = 3; minimum = 0x800;
        } else if (lead >= 0xf0 && lead <= 0xf4) {
            cp = lead & 0x07; length = 4; minimum = 0x10000;
        } else return false;
        if (length > text.size() - i) return false;
        for (size_t j = 1; j < length; ++j) {
            const auto byte = static_cast<unsigned char>(text[i + j]);
            if ((byte & 0xc0) != 0x80) return false;
            cp = (cp << 6) | (byte & 0x3f);
        }
        if (cp < minimum || cp > 0x10ffff || (cp >= 0xd800 && cp <= 0xdfff))
            return false;
        decoded.push_back(cp);
        i += length;
    }
    output.swap(decoded);
    return true;
}

bool Utf8Util::isHangul(char32_t cp) {
    return cp >= 0xac00 && cp <= 0xd7a3;
}

string Utf8Util::encode(char32_t cp) {
    if (!isHangul(cp)) return {};
    string result;
    result += static_cast<char>(0xe0 | (cp >> 12));
    result += static_cast<char>(0x80 | ((cp >> 6) & 0x3f));
    result += static_cast<char>(0x80 | (cp & 0x3f));
    return result;
}

string Utf8Util::trimAscii(const string& text) {
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == string::npos) return {};
    const auto end = text.find_last_not_of(" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

bool Utf8Util::wordParts(const string& word, char32_t& first,
                        char32_t& last, size_t& count) {
    vector<char32_t> decoded;
    if (!decode(word, decoded) || decoded.size() < 2 || decoded.size() > 64) return false;
    for (const auto cp : decoded) if (!isHangul(cp)) return false;
    first = decoded.front();
    last = decoded.back();
    count = decoded.size();
    return true;
}

