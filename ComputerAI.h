#pragma once
#include "RuleChecker.h"
#include "Settings.h"
#include <map>
#include <optional>
#include <random>
#include <tuple>

using namespace std;

// 3번: 합법 후보와 선택 전략. 실제 사용 등록은 GameManager가 한다.
class ComputerAI {
public:
    explicit ComputerAI(unsigned seed = random_device{}());
    vector<string> getLegalCandidates(const string& lastWord,
        const Dictionary& dictionary, const RuleChecker& rules,
        const set<string>& usedWords, const Settings& settings = {}) const;
    size_t countReplies(const string& candidate, const Dictionary& dictionary,
        const RuleChecker& rules, const set<string>& usedWords, const Settings& settings = {}) const;
    optional<string> chooseWord(const string& lastWord,
        const Dictionary& dictionary, const RuleChecker& rules,
        const set<string>& usedWords, const Settings& settings);
    vector<string> getOpeningCandidates(const Dictionary& dictionary,
        const RuleChecker& rules, const Settings& settings = {}) const;
    optional<string> chooseOpening(const Dictionary& dictionary,
        const RuleChecker& rules, const Settings& settings = {},
        const set<string>& usedWords = {});
private:
    mt19937 random_;
    map<tuple<const Dictionary*,size_t,unsigned>,vector<string>> openingPoolCache_;
    string pick(const vector<string>& words);
};

