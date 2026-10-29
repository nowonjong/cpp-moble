#include "ComputerAI.h"
#include "Utf8Util.h"
#include <algorithm>
#include <limits>
#include <map>

using namespace std;

namespace {
struct OpeningChoice {
    const string* word;
    char32_t first;
    char32_t last;
    size_t length;
    size_t replies;
    bool familiar;
};

size_t syllableCount(const string& word) {
    char32_t first = 0, last = 0;
    size_t count = 0;
    return Utf8Util::wordParts(word, first, last, count) ? count : 64;
}
bool allowedKnownWord(const string& word, const Dictionary& dictionary, const Settings& settings,
                      char32_t& first, char32_t& last, size_t& length) {
    if (settings.koreanOnly && !dictionary.isKoreanWord(word)) return false;
    if (!Utf8Util::wordParts(word, first, last, length)) return false;
    return !settings.threeSyllables || length == 3;
}
map<char32_t,size_t> allowedCounts(const Dictionary& dictionary,
                                   const set<string>& usedWords, const Settings& settings) {
    map<char32_t,size_t> counts;
    for (const auto& word : dictionary.allWords()) {
        if (usedWords.count(word)) continue;
        char32_t first = 0, last = 0;
        size_t length = 0;
        if (allowedKnownWord(word, dictionary, settings, first, last, length)) ++counts[first];
    }
    return counts;
}
size_t countedReplies(char32_t first, char32_t last, const RuleChecker& rules,
                      const map<char32_t,size_t>& counts) {
    size_t total = 0;
    for (const auto start : rules.allowedStarts(last)) {
        const auto found = counts.find(start);
        if (found != counts.end()) total += found->second;
        if (start == first && found != counts.end() && found->second > 0) --total;
    }
    return total;
}
size_t countedReplies(const string& word, const RuleChecker& rules,
                      const map<char32_t,size_t>& counts) {
    char32_t first = 0, last = 0;
    size_t length = 0;
    if (!Utf8Util::wordParts(word, first, last, length)) return 0;
    return countedReplies(first, last, rules, counts);
}

vector<OpeningChoice> evaluatedOpenings(const Dictionary& dictionary,
    const RuleChecker& rules, const Settings& settings) {
    vector<OpeningChoice> words;
    words.reserve(dictionary.size());
    map<char32_t,size_t> counts;
    for (const auto& word : dictionary.allWords()) {
        char32_t first = 0, last = 0;
        size_t length = 0;
        if (!allowedKnownWord(word, dictionary, settings, first, last, length)) continue;
        words.push_back({&word,first,last,length,0,dictionary.isKoreanWord(word)});
        ++counts[first];
    }
    for (auto& word : words) word.replies = countedReplies(word.first,word.last,rules,counts);

    map<char32_t,vector<const OpeningChoice*>> legalReplies;
    for (const auto& word : words)
        if (settings.allowOneShot || word.replies > 0) legalReplies[word.first].push_back(&word);

    vector<OpeningChoice> choices;
    choices.reserve(words.size());
    for (const auto& word : words) {
        for (const auto start : rules.allowedStarts(word.last)) {
            const auto indexed=legalReplies.find(start);
            if (indexed==legalReplies.end()) continue;
            bool found = false;
            for (const auto* reply : indexed->second) {
                if (*reply->word == *word.word) continue;
                size_t replyCount = reply->replies;
                const auto nextStarts = rules.allowedStarts(reply->last);
                if (find(nextStarts.begin(),nextStarts.end(),word.first)!=nextStarts.end() && replyCount>0)
                    --replyCount;
                if (settings.allowOneShot || replyCount > 0) { found = true; break; }
            }
            if (found) { choices.push_back(word); break; }
        }
    }
    return choices;
}

// 새 라운드도 같은 게임이므로, 사용한 단어를 제외한 실제 답이 있어야 시작할 수 있다.
bool hasUnusedReply(const string& opening, const Dictionary& dictionary,
                    const RuleChecker& rules, const Settings& settings,
                    const set<string>& usedWords) {
    if (usedWords.count(opening)) return false;
    char32_t first = 0, last = 0;
    size_t length = 0;
    if (!Utf8Util::wordParts(opening, first, last, length)) return false;
    auto usedWithOpening = usedWords;
    usedWithOpening.insert(opening);
    for (const auto start : rules.allowedStarts(last)) {
        for (const auto& reply : dictionary.candidates(start)) {
            if (usedWithOpening.count(reply) || !rules.wordAllowed(reply, dictionary, settings)) continue;
            if (settings.allowOneShot || rules.hasReply(reply, dictionary, usedWithOpening, settings))
                return true;
        }
    }
    return false;
}

long long openingPreference(const OpeningChoice& choice, Difficulty difficulty) {
    if (difficulty == Difficulty::Normal)
        return choice.familiar && choice.length <= 4 ? 1 : 0;
    if (difficulty == Difficulty::Hard)
        return -static_cast<long long>(choice.replies);
    return (choice.familiar ? 1000000LL : 0LL) +
        (choice.length <= 2 ? 4000LL : choice.length == 3 ? 3000LL : choice.length == 4 ? 1500LL : 0LL) +
        static_cast<long long>(min<size_t>(choice.replies, 2000));
}
}

ComputerAI::ComputerAI(unsigned seed) : random_(seed) {}
string ComputerAI::pick(const vector<string>& words) {
    uniform_int_distribution<size_t> select(0, words.size() - 1);
    return words[select(random_)];
}

static vector<string> legalCandidatesWithCounts(const string& lastWord,
    const Dictionary& dictionary, const RuleChecker& rules,
    const set<string>& usedWords, const Settings& settings, const map<char32_t,size_t>& counts) {
    vector<string> found;
    char32_t first = 0, last = 0;
    size_t count = 0;
    if (!Utf8Util::wordParts(lastWord, first, last, count)) return found;

    set<string> unique;
    for (const auto start : rules.allowedStarts(last)) {
        for (const auto& word : dictionary.candidates(start)) {
            if (usedWords.count(word) || !unique.insert(word).second) continue;
            char32_t candidateFirst = 0, candidateLast = 0;
            size_t candidateLength = 0;
            if (allowedKnownWord(word,dictionary,settings,candidateFirst,candidateLast,candidateLength) &&
                (settings.allowOneShot || countedReplies(candidateFirst,candidateLast,rules,counts) > 0))
                    found.push_back(word);
        }
    }
    return found;
}

vector<string> ComputerAI::getLegalCandidates(const string& lastWord,
    const Dictionary& dictionary, const RuleChecker& rules,
    const set<string>& usedWords, const Settings& settings) const {
    const auto counts=allowedCounts(dictionary,usedWords,settings);
    return legalCandidatesWithCounts(lastWord,dictionary,rules,usedWords,settings,counts);
}
size_t ComputerAI::countReplies(const string& candidate, const Dictionary& dictionary,
    const RuleChecker& rules, const set<string>& usedWords, const Settings& settings) const {
    auto temporaryUsed = usedWords;
    temporaryUsed.insert(candidate);  // 평가 중인 단어도 다음 턴에는 재사용 불가
    return getLegalCandidates(candidate, dictionary, rules, temporaryUsed, settings).size();
}

optional<string> ComputerAI::chooseWord(const string& lastWord,
    const Dictionary& dictionary, const RuleChecker& rules,
    const set<string>& usedWords, const Settings& settings) {
    const auto counts = allowedCounts(dictionary, usedWords, settings);
    const auto choices = legalCandidatesWithCounts(lastWord, dictionary, rules, usedWords, settings, counts);
    if (choices.empty()) return nullopt;
    if (settings.difficulty == Difficulty::Normal) {
        // 보통도 체감 난도를 낮추기 위해 익숙하고 짧은 단어를 먼저 사용한다.
        vector<string> friendly;
        for (const auto& word : choices)
            if (dictionary.isKoreanWord(word) && syllableCount(word) <= 4) friendly.push_back(word);
        return pick(friendly.empty() ? choices : friendly);
    }
    size_t best = dictionary.size();
    vector<string> tied;
    if (settings.difficulty == Difficulty::Easy) {
        // 쉬움은 사용자가 답할 선택지가 많고 짧으며 기본 목록에 있는 단어를 고른다.

        size_t bestEase = 0;
        for (const auto& word : choices) {
            const size_t replies = countedReplies(word, rules, counts);
            const size_t length = syllableCount(word);
            const size_t familiarity = dictionary.isKoreanWord(word) ? 1000000 : 0;
            const size_t shortness = length <= 2 ? 4000 : length == 3 ? 3000 : length == 4 ? 1500 : 0;
            const size_t ease = familiarity + shortness + min<size_t>(replies, 2000);
            if (ease > bestEase) { bestEase = ease; tied.clear(); }
            if (ease == bestEase) tied.push_back(word);
        }
        return pick(tied);
    }
    // 어려움은 기존 보통 난이도의 전략을 이어 받아 다음 답변 수가 가장 적은 단어를 고른다.

    for (const auto& word : choices) {
        const auto count = countedReplies(word, rules, counts);
        if (count < best) { best = count; tied.clear(); }
        if (count == best) tied.push_back(word);
    }
    return pick(tied);
}

vector<string> ComputerAI::getOpeningCandidates(const Dictionary& dictionary,
    const RuleChecker& rules, const Settings& settings) const {
    vector<string> choices;
    const auto evaluated = evaluatedOpenings(dictionary,rules,settings);
    choices.reserve(evaluated.size());
    for (const auto& choice : evaluated) choices.push_back(*choice.word);
    return choices;
}

optional<string> ComputerAI::chooseOpening(const Dictionary& dictionary,
    const RuleChecker& rules, const Settings& settings, const set<string>& usedWords) {
    const unsigned optionKey = (settings.allowOneShot ? 1u : 0u) |
        (settings.koreanOnly ? 2u : 0u) | (settings.threeSyllables ? 4u : 0u) |
        (settings.difficulty == Difficulty::Normal ? 8u : settings.difficulty == Difficulty::Hard ? 16u : 0u);
    const auto cacheKey = make_tuple(&dictionary,dictionary.revision(),optionKey);
    const auto cached = openingPoolCache_.find(cacheKey);
    if (cached == openingPoolCache_.end()) {
        const auto choices = evaluatedOpenings(dictionary, rules, settings);
        if (choices.empty()) return nullopt;
        vector<string> pool;
        if (settings.difficulty == Difficulty::Normal) {
            vector<string> all;
            vector<string> friendly;
            all.reserve(choices.size());
            for (const auto& choice : choices) {
                all.push_back(*choice.word);
                if (choice.familiar && choice.length <= 4) friendly.push_back(*choice.word);
            }
            pool = friendly.empty() ? move(all) : move(friendly);
        } else {
            size_t best = settings.difficulty == Difficulty::Easy ? 0 : (numeric_limits<size_t>::max)();
            for (const auto& choice : choices) {
                if (settings.difficulty == Difficulty::Easy) {
                    const size_t ease = (choice.familiar ? 1000000 : 0) +
                        (choice.length <= 2 ? 4000 : choice.length == 3 ? 3000 : choice.length == 4 ? 1500 : 0) +
                        min<size_t>(choice.replies, 2000);
                    if (ease > best) { best = ease; pool.clear(); }
                    if (ease == best) pool.push_back(*choice.word);
                } else {
                    if (choice.replies < best) { best = choice.replies; pool.clear(); }
                    if (choice.replies == best) pool.push_back(*choice.word);
                }
            }
        }
        openingPoolCache_.emplace(cacheKey,move(pool));
    }
    const auto& preferred = openingPoolCache_.find(cacheKey)->second;
    if (usedWords.empty())
        return preferred.empty() ? nullopt : optional<string>(pick(preferred));

    vector<string> availablePreferred;
    for (const auto& word : preferred)
        if (hasUnusedReply(word, dictionary, rules, settings, usedWords)) availablePreferred.push_back(word);
    if (!availablePreferred.empty()) return pick(availablePreferred);

    // 선호 후보가 소진되면 같은 난이도 기준으로 다음 후보를 찾는다.
    auto remaining = evaluatedOpenings(dictionary, rules, settings);
    shuffle(remaining.begin(), remaining.end(), random_);
    stable_sort(remaining.begin(), remaining.end(), [&](const OpeningChoice& a, const OpeningChoice& b) {
        return openingPreference(a, settings.difficulty) > openingPreference(b, settings.difficulty);
    });
    for (const auto& choice : remaining)
        if (hasUnusedReply(*choice.word, dictionary, rules, settings, usedWords)) return *choice.word;
    return nullopt;
}

