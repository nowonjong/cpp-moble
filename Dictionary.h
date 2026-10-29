#pragma once
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

using namespace std;

enum class DictionaryStatus { Ok, NotFound, Empty, ReadError };
struct DictionaryReport {
    DictionaryStatus status = DictionaryStatus::NotFound;
    size_t lines = 0, accepted = 0, duplicates = 0, invalid = 0, blank = 0;
};

// 1번: 파일 데이터와 검색 인덱스를 관리한다.
class Dictionary {
public:
    DictionaryReport load(const filesystem::path& path);
    DictionaryReport loadKoreanWords(const filesystem::path& path);
    bool isKoreanWord(const string& word) const;
    size_t koreanSize() const;
    bool contains(const string& word) const;
    const vector<string>& candidates(char32_t first) const;
    const set<string>& allWords() const;
    size_t size() const;
    size_t revision() const;
private:
    set<string> words_;
    set<string> koreanWords_;
    map<char32_t, vector<string>> index_;
    size_t revision_ = 0;
};

