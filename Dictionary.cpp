#include "Dictionary.h"
#include "Utf8Util.h"
#include <fstream>

using namespace std;

DictionaryReport Dictionary::load(const filesystem::path& path) {
    ++revision_;
    words_.clear();
    koreanWords_.clear();
    index_.clear();
    DictionaryReport report;
    error_code error;
    const bool exists = filesystem::exists(path, error);
    if (error) { report.status = DictionaryStatus::ReadError; return report; }
    if (!exists) return report;
    if (!filesystem::is_regular_file(path, error) || error) {
        report.status = DictionaryStatus::ReadError; return report;
    }
    ifstream file(path, ios::binary);
    if (!file) { report.status = DictionaryStatus::ReadError; return report; }
    string line;
    while (getline(file, line)) {
        ++report.lines;
        if (report.lines == 1 && line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
        line = Utf8Util::trimAscii(line);
        if (line.empty()) { ++report.blank; continue; }
        char32_t first = 0, last = 0;
        size_t count = 0;
        if (!Utf8Util::wordParts(line, first, last, count)) {
            ++report.invalid; continue;
        }
        if (!words_.insert(line).second) { ++report.duplicates; continue; }
        index_[first].push_back(line);
        ++report.accepted;
    }
    if (file.bad() || (!file.eof() && file.fail())) {
        words_.clear(); index_.clear(); report.accepted = 0;
        report.status = DictionaryStatus::ReadError;
    } else {
        report.status = words_.empty() ? DictionaryStatus::Empty : DictionaryStatus::Ok;
    }
    return report;
}

bool Dictionary::contains(const string& word) const { return words_.count(word) != 0; }
DictionaryReport Dictionary::loadKoreanWords(const filesystem::path& path) {
    ++revision_;
    koreanWords_.clear();
    Dictionary selected;
    auto report = selected.load(path);
    if (report.status != DictionaryStatus::Ok) return report;
    for (const auto& word : selected.allWords()) {
        if (contains(word)) koreanWords_.insert(word);
        else ++report.invalid;
    }
    report.accepted = koreanWords_.size();
    report.status = koreanWords_.empty() ? DictionaryStatus::Empty : DictionaryStatus::Ok;
    return report;
}
bool Dictionary::isKoreanWord(const string& word) const { return koreanWords_.count(word) != 0; }
size_t Dictionary::koreanSize() const { return koreanWords_.size(); }
const vector<string>& Dictionary::candidates(char32_t first) const {
    static const vector<string> empty;
    const auto found = index_.find(first);
    return found == index_.end() ? empty : found->second;
}
const set<string>& Dictionary::allWords() const { return words_; }
size_t Dictionary::size() const { return words_.size(); }
size_t Dictionary::revision() const { return revision_; }

