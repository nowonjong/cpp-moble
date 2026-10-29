#pragma once
#include "RecordTypes.h"
#include "ScorePolicy.h"
#include <set>
#include <string>

using namespace std;

enum class GamePhase { NotStarted, HumanTurn, ComputerTurn, Finished };
enum class EndReason { None, ThreeErrors, HumanStuck, ComputerStuck, Surrender, InputEnded };
struct GameState {
    GamePhase phase = GamePhase::NotStarted;
    GameResult result = GameResult::None;
    EndReason endReason = EndReason::None;
    Settings settings;
    string lastWord;
    string playerName;
    set<string> usedWords;
    int score = 0;
    int errorsThisTurn = 0;
    int humanMoves = 0;
    int computerDefeats = 0;
    int lastComputerDefeatBonus = 0;
    ScoreBreakdown lastAward;
    SaveStatus saveStatus = SaveStatus::Blocked;
    int remainingAttempts() const { return 3 - errorsThisTurn; }
};

