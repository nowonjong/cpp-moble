#pragma once
#include "Dictionary.h"
#include "Settings.h"
#include <set>

using namespace std;

enum class ValidationCode {
    Valid, Empty, InvalidEncoding, InvalidCharacters, TooShort, TooLong,
    NotInDictionary, AlreadyUsed, WrongStart, NotYourTurn,
    NotKoreanWord, MustBeThreeSyllables, OneShotNotAllowed
};
struct ValidationResult {
    ValidationCode code = ValidationCode::Valid;
    string word;  // 앞뒤 공백을 제거한 입력
    bool valid() const { return code == ValidationCode::Valid; }
};

// 2번: 검사만 한다. 게임 상태를 변경하거나 출력하지 않는다.
class RuleChecker {
public:
    ValidationResult validate(const string& input, const string& lastWord,
                              const Dictionary& dictionary,
                              const set<string>& usedWords, const Settings& settings = {}) const;
    vector<char32_t> allowedStarts(char32_t last) const;
    bool wordAllowed(const string& word, const Dictionary& dictionary, const Settings& settings) const;
    bool hasReply(const string& word, const Dictionary& dictionary,
                  const set<string>& usedWords, const Settings& settings) const;
};

