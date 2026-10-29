#pragma once
#include "GameManager.h"
#include <iostream>
using namespace std;

class ConsoleUI {
public:
    ConsoleUI(istream& input, ostream& output) : input_(input), output_(output) {}
    int readChoice(const string& prompt, int low, int high);
    bool readLine(const string& prompt, string& text);
    void message(const string& text);
    void showRanking(const RecordManager& records);
    void showRules();
    void showGameState(const GameState& state, const RuleChecker& rules);
    static string errorText(ValidationCode code);
private:
    istream& input_;
    ostream& output_;
};
