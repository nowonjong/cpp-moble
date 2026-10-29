#include "RuleChecker.h"
#include "Utf8Util.h"
#include <algorithm>

using namespace std;

vector<char32_t> RuleChecker::allowedStarts(char32_t last) const {
    vector<char32_t> starts {last};
    if (!Utf8Util::isHangul(last)) return starts;
    const int syllable = static_cast<int>(last - U'가');
    const int initial = syllable / (21 * 28);
    const int vowel = (syllable / 28) % 21;
    int changed = initial;
    // 종성은 그대로 두고 초성만 바꾼다. 력→역, 년→연, 록→녹.
    if (initial == 2 && (vowel == 6 || vowel == 12 || vowel == 17 || vowel == 20)) changed = 11;
    if (initial == 5) {
        if (vowel == 2 || vowel == 6 || vowel == 7 || vowel == 12 || vowel == 17 || vowel == 20)
            changed = 11;
        else if (vowel == 0 || vowel == 1 || vowel == 8 || vowel == 11 || vowel == 13 || vowel == 18)
            changed = 2;
    }
    if (changed != initial) starts.push_back(static_cast<char32_t>(last + (changed - initial) * 21 * 28));
    return starts;
}

bool RuleChecker::wordAllowed(const string& word, const Dictionary& dictionary, const Settings& settings) const {
    if (!dictionary.contains(word)) return false;
    if (settings.koreanOnly && !dictionary.isKoreanWord(word)) return false;
    char32_t first = 0, last = 0;
    size_t count = 0;
    return Utf8Util::wordParts(word, first, last, count) && (!settings.threeSyllables || count == 3);
}

bool RuleChecker::hasReply(const string& word, const Dictionary& dictionary,
                           const set<string>& usedWords, const Settings& settings) const {
    char32_t first = 0, last = 0;
    size_t count = 0;
    if (!Utf8Util::wordParts(word, first, last, count)) return false;
    // 한방 판정에서 한방 금지를 다시 적용하면 재귀 판정이 된다.
    // 여기서는 언어/길이/두음/중복 조건으로 다음 단어의 존재만 확인한다.
    for (const auto start : allowedStarts(last)) {
        for (const auto& reply : dictionary.candidates(start)) {
            if (reply != word && !usedWords.count(reply) && wordAllowed(reply, dictionary, settings)) return true;
        }
    }
    return false;
}

ValidationResult RuleChecker::validate(const string& input, const string& lastWord,
                                       const Dictionary& dictionary,
                                       const set<string>& usedWords, const Settings& settings) const {
    ValidationResult result;
    result.word = Utf8Util::trimAscii(input);
    if (result.word.empty()) { result.code = ValidationCode::Empty; return result; }
    vector<char32_t> cp;
    if (!Utf8Util::decode(result.word, cp)) {
        result.code = ValidationCode::InvalidEncoding; return result;
    }
    for (const auto ch : cp) {
        if (!Utf8Util::isHangul(ch)) {
            result.code = ValidationCode::InvalidCharacters; return result;
        }
    }
    if (cp.size() < 2) { result.code = ValidationCode::TooShort; return result; }
    if (cp.size() > 64) { result.code = ValidationCode::TooLong; return result; }
    if (!dictionary.contains(result.word)) {
        result.code = ValidationCode::NotInDictionary; return result;
    }
    if (settings.threeSyllables && cp.size() != 3) {
        result.code = ValidationCode::MustBeThreeSyllables; return result;
    }
    if (settings.koreanOnly && !dictionary.isKoreanWord(result.word)) {
        result.code = ValidationCode::NotKoreanWord; return result;
    }
    if (usedWords.count(result.word)) {
        result.code = ValidationCode::AlreadyUsed; return result;
    }
    char32_t first = 0, last = 0;
    size_t count = 0;
    if (!Utf8Util::wordParts(lastWord, first, last, count)) {
        result.code = ValidationCode::WrongStart; return result;
    }
    const auto starts = allowedStarts(last);
    if (find(starts.begin(), starts.end(), cp.front()) == starts.end()) {
        result.code = ValidationCode::WrongStart;
        return result;
    }
    if (!settings.allowOneShot && !hasReply(result.word, dictionary, usedWords, settings))
        result.code = ValidationCode::OneShotNotAllowed;
    return result;
}

