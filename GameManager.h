#pragma once
#include "GameTypes.h"
#include "ComputerAI.h"
#include "RecordManager.h"

using namespace std;

// 4번: 현재 게임 상태를 변경하는 유일한 클래스.
class GameManager {
public:
    GameManager(const Dictionary& dictionary, const RuleChecker& rules,
                ComputerAI& computer, RecordManager& records);
    bool startNewGame(const Settings& settings, const string& name, const string& openingForTest = "");
    ValidationResult submitWord(const string& word);
    bool playComputerTurn();
    void surrender();
    void abort();
    const GameState& state() const;
private:
    const Dictionary& dictionary_;
    const RuleChecker& rules_;
    ComputerAI& computer_;
    RecordManager& records_;
    GameState state_;
    void finishGame(GameResult result, EndReason reason);
};

