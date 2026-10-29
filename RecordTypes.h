#pragma once
#include "Settings.h"
#include <cstddef>
#include <string>

using namespace std;

enum class GameResult { None, Win, Lose, Aborted };

struct GameRecord {
    GameResult result = GameResult::None;
    int score = 0;
    Settings settings;
    string name;
};

struct RankingEntry {
    size_t rank = 0;
    string name;
    int score = 0;
    Difficulty difficulty = Difficulty::Easy;
};

struct Statistics {
    size_t wins = 0;
    size_t losses = 0;
    size_t aborted = 0;
    int bestScore = 0;
};

enum class LoadStatus { Ok, NotFound, InvalidFormat, ReadError };
enum class SaveStatus { Ok, WriteError, Blocked, InvalidRecord };

