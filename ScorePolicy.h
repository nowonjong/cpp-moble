#pragma once
#include <cstddef>
using namespace std;

struct ScoreBreakdown {
    int base = 0;
    int attemptBonus = 0;
    int lengthBonus = 0;
    int total() const { return base + attemptBonus + lengthBonus; }
};

class ScorePolicy {
public:
    static constexpr int ComputerDefeatBonus = 1000;

    static ScoreBreakdown calculate(size_t syllables, int attemptsRemaining) {
        if (syllables < 2 || syllables > 64 || attemptsRemaining < 1 || attemptsRemaining > 3) return {};
        return {100, (attemptsRemaining - 1) * 50, static_cast<int>(syllables) * 20};
    }
};
