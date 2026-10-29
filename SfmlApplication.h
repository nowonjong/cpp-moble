#pragma once
#include <SFML/Graphics.hpp>
#include "GameManager.h"
#include "NativeTextInput.h"
#include "ProfileManager.h"
#include <filesystem>
#include <string>
#include <vector>

using namespace std;

// 6번: 창, 버튼, 화면 전환만 관리한다. 판정과 점수는 기존 GameManager에 맡긴다.
class SfmlApplication {
public:
    SfmlApplication(const filesystem::path& dataDirectory, const filesystem::path& fontPath);
    int run(const filesystem::path& captureDirectory = {}, bool runUiTest = false);
private:
    enum class Screen { Home, Game, Profiles, Records, Rules, Error };
    enum class Dialog { None, Leave, Close, ResetRecords };
    struct Button { string id; sf::FloatRect bounds; bool enabled; };
    struct Move { bool human; string word; };
    struct ResultParticle {
        sf::Vector2f origin;
        sf::Vector2f velocity;
        sf::Color color;
        float size;
        float delay;
    };
    struct ProfileSummary {
        string name;
        size_t games = 0;
        size_t wins = 0;
        size_t losses = 0;
        int bestScore = 0;
        long long totalScore = 0;
        bool active = false;
    };

    sf::RenderWindow window_;
    sf::Font font_;

    NativeTextInput input_;
    Dictionary dictionary_;
    RuleChecker rules_;
    ComputerAI computer_;
    RecordManager records_;
    ProfileManager profiles_;
    GameManager game_;
    Settings settings_;
    filesystem::path dataDirectory_;
    Screen screen_ = Screen::Home;
    Dialog dialog_ = Dialog::None;
    bool paused_ = false;
    vector<Button> buttons_;
    vector<Move> moves_;
    string notice_;
    bool noticeError_ = false;
    string fatalError_;
    sf::Clock computerClock_;
    sf::Clock resultClock_;
    sf::Clock roundWinClock_;
    sf::Texture logoTexture_;
    sf::Texture iconTexture_;
    sf::Texture referenceTexture_;
    bool referenceLoaded_ = false;
    bool logoLoaded_ = false;
    bool iconLoaded_ = false;
    vector<ResultParticle> resultParticles_;
    GameResult animatedResult_ = GameResult::None;
    bool resultAnimationStarted_ = false;
    bool roundWinImpact_ = false;
    sf::Vector2f mouse_;
    int recordPage_ = 0;
    int profilePage_ = 0;
    Difficulty rankingDifficulty_ = Difficulty::Easy;
    int historyScroll_ = 0;
    bool inputVisible_ = false;
    bool inputEnabled_ = false;
    bool inputHome_ = true;
    string playerName_ = "플레이어";
    bool uiTest_ = false;
    int testStep_ = 0;
    int frame_ = 0;
    filesystem::path captureDirectory_;

    void draw();
    void drawHeader();
    void drawHome();
    void drawGame();
    void drawProfiles();
    void drawRecords();
    void drawRules();
    void drawDialog();
    void drawPause();
    void drawBackground();
    void drawResultCelebration();
    void drawRoundWinImpact();
    void drawLogo(float x,float y,float width,bool iconOnly=false);
    void drawMascot(float centerX,float centerY,float scale,bool computer,bool active);
    void startResultAnimation(GameResult result);
    void panel(float x,float y,float w,float h,sf::Color color,float radius=16.f);
    void shadowPanel(float x,float y,float w,float h,sf::Color color,float radius=16.f);
    void centeredLabel(float centerX,float y,unsigned size,const string& value,sf::Color color);
    void label(float x,float y,unsigned size,const string& value,sf::Color color);
    void drawText(const sf::Text& text);
    void centeredAt(float x,float y,unsigned size,const string& value,sf::Color color);
    void wrapped(float x,float y,float width,unsigned size,const string& value,sf::Color color);
    void button(const string& id,float x,float y,float w,float h,const string& caption,bool enabled=true,bool primary=false);
    void badge(float x,float y,const string& value,sf::Color color);
    void resize();
    void syncInput();
    void click(const string& id);
    void startGame();
    void submit();
    void updateComputer();
    void pauseGame();
    void resumeGame();
    void requestClose();
    void confirmDialog();
    void capture(const string& filename);
    void advanceUiTest();
    bool activeGame() const;
    vector<ProfileSummary> profileSummaries() const;
    string errorText(ValidationCode code) const;
    string resultText(GameResult result) const;
};
