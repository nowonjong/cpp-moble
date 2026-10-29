#pragma once

enum class Difficulty { Easy, Normal, Hard };

// 5번: 한 판을 시작할 때 복사해서 사용하는 설정.
struct Settings {
    Difficulty difficulty = Difficulty::Easy;
    bool allowOneShot = false;
    bool koreanOnly = false;  // 별도 우리말 허용 사전 사용 (외래어 제외, 한자어 허용)
    bool threeSyllables = false;
};

