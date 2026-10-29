#pragma once
#include "RecordTypes.h"
#include <filesystem>
#include <vector>

using namespace std;

// 5번: 확정 결과의 보관과 집계. 게임 점수나 승패를 결정하지 않는다.
class RecordManager {
public:
    LoadStatus loadRecords(const filesystem::path& path);
    SaveStatus saveResult(const GameRecord& record);
    SaveStatus resetRecords();  // UI에서 사용자가 승인한 경우에만 호출
    Statistics getStatistics() const;
    vector<RankingEntry> rankings() const;
    vector<RankingEntry> rankings(Difficulty difficulty) const;
    static bool validName(const string& name);
    const vector<GameRecord>& records() const;
    bool canSave() const;
    LoadStatus loadStatus() const;
    const filesystem::path& path() const;
    const filesystem::path& lastBackupPath() const;
private:
    filesystem::path path_;
    filesystem::path lastBackup_;
    vector<GameRecord> records_;
    bool writable_ = false;
    LoadStatus loadStatus_ = LoadStatus::ReadError;
    SaveStatus writeAll(const vector<GameRecord>& records);
};

