#include "RecordManager.h"
#include "Utf8Util.h"
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>

using namespace std;

namespace {
bool validRecord(const GameRecord& record) {
    const bool resultOk = record.result == GameResult::Win ||
        record.result == GameResult::Lose || record.result == GameResult::Aborted;
    const bool difficultyOk = record.settings.difficulty == Difficulty::Easy ||
        record.settings.difficulty == Difficulty::Normal ||
        record.settings.difficulty == Difficulty::Hard;
    return resultOk && difficultyOk && record.score >= 0 && RecordManager::validName(record.name);
}
const char* resultToken(GameResult result) {
    if (result == GameResult::Win) return "WIN";
    if (result == GameResult::Lose) return "LOSE";
    return "ABORTED";
}
bool parseRecord(const string& line, GameRecord& record) {
    istringstream input(line);
    string result, difficulty, extra;
    int oneShot = 0, korean = 0, three = 0;
    if (!(input >> quoted(record.name) >> record.score >> difficulty >> result >> oneShot >> korean >> three)) return false;
    if (input >> extra) return false;
    if (result == "WIN") record.result = GameResult::Win;
    else if (result == "LOSE") record.result = GameResult::Lose;
    else if (result == "ABORTED") record.result = GameResult::Aborted;
    else return false;
    if (difficulty == "EASY") record.settings.difficulty = Difficulty::Easy;
    else if (difficulty == "NORMAL") record.settings.difficulty = Difficulty::Normal;
    else if (difficulty == "HARD") record.settings.difficulty = Difficulty::Hard;
    else return false;
    if ((oneShot != 0 && oneShot != 1) || (korean != 0 && korean != 1) || (three != 0 && three != 1)) return false;
    record.settings.allowOneShot = oneShot == 1;
    record.settings.koreanOnly = korean == 1;
    record.settings.threeSyllables = three == 1;
    return validRecord(record);
}
filesystem::path unusedSibling(const filesystem::path& path, const string& suffix) {
    for (int i = 0; i < 10000; ++i) {
        auto candidate = path;
        candidate += suffix + to_string(i);
        error_code error;
        const bool exists = filesystem::exists(candidate, error);
        if (error) return {};
        if (!exists) return candidate;
    }
    return {};
}
}

LoadStatus RecordManager::loadRecords(const filesystem::path& path) {
    path_ = path;
    records_.clear();
    lastBackup_.clear();
    writable_ = false;
    loadStatus_ = LoadStatus::ReadError;
    error_code error;
    const bool exists = filesystem::exists(path_, error);
    if (error || path_.empty()) return loadStatus_;
    if (!exists) {
        writable_ = true;
        return loadStatus_ = LoadStatus::NotFound;
    }
    if (!filesystem::is_regular_file(path_, error) || error) return loadStatus_;
    ifstream file(path_, ios::binary);
    if (!file) return loadStatus_;
    string line;
    if (!getline(file, line)) {
        return loadStatus_ = file.bad() ? LoadStatus::ReadError : LoadStatus::InvalidFormat;
    }
    if (line.compare(0, 3, "\xef\xbb\xbf") == 0) line.erase(0, 3);
    if (Utf8Util::trimAscii(line) != "WORDCHAIN_RANKING_V2")
        return loadStatus_ = LoadStatus::InvalidFormat;
    vector<GameRecord> loaded;
    while (getline(file, line)) {
        line = Utf8Util::trimAscii(line);
        if (line.empty()) continue;
        GameRecord record;
        if (!parseRecord(line, record)) return loadStatus_ = LoadStatus::InvalidFormat;
        loaded.push_back(record);
    }
    if (file.bad() || (!file.eof() && file.fail())) return loadStatus_;
    records_.swap(loaded);
    writable_ = true;
    return loadStatus_ = LoadStatus::Ok;
}

SaveStatus RecordManager::writeAll(const vector<GameRecord>& records) {
    if (path_.empty()) return SaveStatus::WriteError;
    const auto temporary = unusedSibling(path_, ".tmp.");
    if (temporary.empty()) return SaveStatus::WriteError;
    ofstream file(temporary, ios::binary | ios::trunc);
    if (!file) return SaveStatus::WriteError;
    file << "WORDCHAIN_RANKING_V2\n";
    for (const auto& record : records) {
        file << quoted(record.name) << ' ' << record.score << ' '
             << (record.settings.difficulty == Difficulty::Easy ? "EASY" :
                 record.settings.difficulty == Difficulty::Normal ? "NORMAL" : "HARD")
             << ' ' << resultToken(record.result)
             << ' ' << (record.settings.allowOneShot ? 1 : 0)
             << ' ' << (record.settings.koreanOnly ? 1 : 0)
             << ' ' << (record.settings.threeSyllables ? 1 : 0) << '\n';
    }
    file.close();
    error_code error;
    if (file.fail()) {
        filesystem::remove(temporary, error);
        return SaveStatus::WriteError;
    }
    // 기존 기록을 먼저 지우지 않는다. 교체 실패 시 원본을 보존한다.
    filesystem::rename(temporary, path_, error);
    if (error) {
        error_code cleanupError;
        filesystem::remove(temporary, cleanupError);
        return SaveStatus::WriteError;
    }
    return SaveStatus::Ok;
}

SaveStatus RecordManager::saveResult(const GameRecord& record) {
    if (!writable_) return SaveStatus::Blocked;
    if (!validRecord(record)) return SaveStatus::InvalidRecord;
    auto next = records_;
    next.push_back(record);
    const auto status = writeAll(next);
    if (status == SaveStatus::Ok) {
        records_.swap(next);
        loadStatus_ = LoadStatus::Ok;
    }
    return status;
}

SaveStatus RecordManager::resetRecords() {
    if (path_.empty()) return SaveStatus::Blocked;
    error_code error;
    const bool exists = filesystem::exists(path_, error);
    if (error) return SaveStatus::WriteError;
    if (exists) {
        if (!filesystem::is_regular_file(path_, error) || error) return SaveStatus::WriteError;
        const auto backup = unusedSibling(path_, ".bak.");
        if (backup.empty()) return SaveStatus::WriteError;
        filesystem::copy_file(path_, backup, filesystem::copy_options::none, error);
        if (error) return SaveStatus::WriteError;
        lastBackup_ = backup;
    }
    const auto result = writeAll({});
    if (result == SaveStatus::Ok) {
        records_.clear();
        writable_ = true;
        loadStatus_ = LoadStatus::Ok;
    }
    return result;
}

Statistics RecordManager::getStatistics() const {
    Statistics stats;
    for (const auto& record : records_) {
        if (record.result == GameResult::Aborted) { ++stats.aborted; continue; }
        if (record.result == GameResult::Win) ++stats.wins;
        if (record.result == GameResult::Lose) ++stats.losses;
        stats.bestScore = max(stats.bestScore, record.score);
    }
    return stats;
}
const vector<GameRecord>& RecordManager::records() const { return records_; }
bool RecordManager::canSave() const { return writable_; }
LoadStatus RecordManager::loadStatus() const { return loadStatus_; }
const filesystem::path& RecordManager::path() const { return path_; }
const filesystem::path& RecordManager::lastBackupPath() const { return lastBackup_; }

bool RecordManager::validName(const string& name) {
    if (Utf8Util::trimAscii(name) != name || name.empty()) return false;
    vector<char32_t> letters;
    if (!Utf8Util::decode(name, letters) || letters.size() > 12) return false;
    for (const auto letter : letters) {
        if (!Utf8Util::isHangul(letter) && !(letter >= U'A' && letter <= U'Z') &&
            !(letter >= U'a' && letter <= U'z') && !(letter >= U'0' && letter <= U'9') &&
            letter != U' ' && letter != U'_') return false;
    }
    return true;
}

vector<RankingEntry> RecordManager::rankings() const {
    vector<RankingEntry> result;
    for (const auto& record : records_) {
        if (record.result != GameResult::Aborted)
            result.push_back({0, record.name, record.score, record.settings.difficulty});
    }
    stable_sort(result.begin(), result.end(), [](const RankingEntry& a, const RankingEntry& b) {
        return a.score > b.score;
    });
    for (size_t i = 0; i < result.size(); ++i)
        result[i].rank = i > 0 && result[i].score == result[i - 1].score ? result[i - 1].rank : i + 1;
    return result;
}

vector<RankingEntry> RecordManager::rankings(Difficulty difficulty) const {
    auto result = rankings();
    result.erase(remove_if(result.begin(), result.end(), [difficulty](const RankingEntry& entry) {
        return entry.difficulty != difficulty;
    }), result.end());
    for (size_t i = 0; i < result.size(); ++i)
        result[i].rank = i > 0 && result[i].score == result[i - 1].score ? result[i - 1].rank : i + 1;
    return result;
}

