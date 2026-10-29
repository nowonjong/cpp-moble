#include "SfmlApplication.h"
#include "Utf8Util.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <map>
#include <stdexcept>

using namespace std;

namespace {
const sf::Color background(244, 245, 252);
const sf::Color backgroundDeep(232, 234, 248);
const sf::Color ink(34, 35, 62);
const sf::Color muted(75, 78, 105);
const sf::Color purple(101, 77, 255);
const sf::Color purpleDark(75, 52, 221);
const sf::Color lavender(237, 233, 255);
const sf::Color border(223, 225, 237);
const sf::Color mint(38, 190, 151);
const sf::Color mintSoft(224, 248, 241);
const sf::Color coral(239, 84, 104);
const sf::Color coralSoft(255, 232, 236);
const sf::Color gold(244, 177, 56);
const sf::Color white(255, 255, 255);
sf::String unicode(const string& text) { return sf::String::fromUtf8(text.begin(), text.end()); }
string utf8(const sf::String& text) {
    const auto bytes = text.toUtf8();
    return string(bytes.begin(), bytes.end());
}
string difficultyName(Difficulty difficulty) {
    if (difficulty == Difficulty::Easy) return "쉬움";
    if (difficulty == Difficulty::Normal) return "보통";
    return "어려움";
}
}

SfmlApplication::SfmlApplication(const filesystem::path& dataDirectory, const filesystem::path& fontPath)
    : window_(sf::VideoMode({1320, 936}), unicode("끗마릳기 | 말끝을 잇는 한 판"), sf::Style::Default, sf::State::Windowed,
          [] { sf::ContextSettings settings; settings.antiAliasingLevel=8; return settings; }()),
      game_(dictionary_, rules_, computer_, records_), dataDirectory_(dataDirectory) {
    window_.setFramerateLimit(60);
    window_.setMinimumSize(sf::Vector2u{900, 640});
    if (!font_.openFromFile(fontPath)) throw runtime_error("Malgun Gothic font could not be loaded.");

    logoLoaded_ = logoTexture_.loadFromFile(dataDirectory_ / "branding" / "kkeut-maridegi-logo-horizontal.png");
    iconLoaded_ = iconTexture_.loadFromFile(dataDirectory_ / "branding" / "kkeut-maridegi-icon.png");
    referenceLoaded_ = referenceTexture_.loadFromFile(dataDirectory_ / "branding" / "game-reference.png");
    if (referenceLoaded_) referenceTexture_.setSmooth(true);
    if (logoLoaded_) logoTexture_.setSmooth(true);
    if (iconLoaded_) {
        iconTexture_.setSmooth(true);
        const auto windowIcon = iconTexture_.copyToImage();
        window_.setIcon(windowIcon.getSize(),windowIcon.getPixelsPtr());
    }
    if (!input_.create(window_.getNativeHandle(), 72, 510, 454, 54))
        throw runtime_error("The Korean input field could not be created.");
    input_.setVisible(false);
    const auto report = dictionary_.load(dataDirectory_ / "words.txt");
    dictionary_.loadKoreanWords(dataDirectory_ / "korean_words.txt");
    if (report.status != DictionaryStatus::Ok) {
        screen_ = Screen::Error;
        fatalError_ = "사전을 읽을 수 없거나 사용할 수 있는 단어가 없습니다.\n"
            "data 폴더의 words.txt를 확인한 뒤 다시 실행해 주세요.\n\n" +
            (dataDirectory_ / "words.txt").u8string();
    }
    const auto loaded = records_.loadRecords(dataDirectory_ / "rankings.txt");
    const auto profileLoaded = profiles_.load(dataDirectory_ / "profiles.txt");
    if (profileLoaded == ProfileManager::LoadStatus::NotFound) {
        vector<string> migratedNames;
        for (auto record = records_.records().rbegin(); record != records_.records().rend(); ++record) {
            if (RecordManager::validName(record->name) &&
                find(migratedNames.begin(), migratedNames.end(), record->name) == migratedNames.end())
                migratedNames.push_back(record->name);
        }
        if (!migratedNames.empty()) profiles_.initialize(migratedNames);
    }
    if (profiles_.hasActiveProfile()) playerName_ = profiles_.activeProfile();
    else if (profiles_.canSave()) profiles_.selectOrCreate(playerName_);
    input_.setTextUtf8(playerName_);
    if (loaded == LoadStatus::InvalidFormat || loaded == LoadStatus::ReadError) {
        notice_ = "기록을 읽을 수 없어 저장을 중지했어요. 복구 버튼에서 백업 후 초기화할 수 있어요.";
        noticeError_ = true;
    } else if (profileLoaded == ProfileManager::LoadStatus::InvalidFormat ||
               profileLoaded == ProfileManager::LoadStatus::ReadError) {
        notice_ = "프로필 파일을 읽지 못했어요. 게임과 랭킹은 그대로 이용할 수 있어요.";
        noticeError_ = true;
    } else if (report.invalid || report.duplicates) {
        notice_ = "사전에서 잘못된 행 " + to_string(report.invalid) + "개와 중복 " +
            to_string(report.duplicates) + "개를 제외했어요.";
    }
    resize();
}

int SfmlApplication::run(const filesystem::path& captureDirectory, bool runUiTest) {
    captureDirectory_ = captureDirectory;
    uiTest_ = runUiTest;
    if (!captureDirectory_.empty()) filesystem::create_directories(captureDirectory_);
    while (window_.isOpen()) {
        while (const auto event = window_.pollEvent()) {
            if (event->is<sf::Event::Closed>()) requestClose();
            else if (event->is<sf::Event::Resized>()) resize();
            else if (const auto* keyEvent = event->getIf<sf::Event::KeyPressed>()) {
                if (keyEvent->code == sf::Keyboard::Key::Escape && dialog_ == Dialog::None) {
                    if (paused_) resumeGame();
                    else if (activeGame()) pauseGame();
                }
            }
            else if (const auto* mouseEvent = event->getIf<sf::Event::MouseButtonPressed>()) {
                if (mouseEvent->button != sf::Mouse::Button::Left) continue;
                const auto point = window_.mapPixelToCoords(mouseEvent->position);
                const auto currentButtons = buttons_;
                for (auto it = currentButtons.rbegin(); it != currentButtons.rend(); ++it) {
                    if (it->enabled && it->bounds.contains(point)) { click(it->id); break; }
                }
            } else if (const auto* wheel = event->getIf<sf::Event::MouseWheelScrolled>()) {
                if (screen_ == Screen::Game && dialog_ == Dialog::None && !paused_) {
                    const int maximum = max(0, static_cast<int>(moves_.size()) - 6);
                    historyScroll_ = clamp(historyScroll_ + (wheel->delta > 0 ? 1 : -1), 0, maximum);
                }
            }
        }
        if (!window_.isOpen()) break;
        if (input_.consumePause() && dialog_ == Dialog::None) {
            if (paused_) resumeGame();
            else if (activeGame()) pauseGame();
        }
        if (input_.consumeSubmit() && dialog_ == Dialog::None && !paused_) {
            if (screen_ == Screen::Home) startGame();
            else if (screen_ == Screen::Game) submit();
        }
        updateComputer();
        syncInput();
        mouse_ = window_.mapPixelToCoords(sf::Mouse::getPosition(window_));
        draw();
        window_.display();
        ++frame_;
        if (uiTest_) advanceUiTest();
        else if (!captureDirectory_.empty() && frame_ == 3) {
            capture("home.png");
            window_.close();
        }
    }
    return 0;
}

bool SfmlApplication::activeGame() const {
    return game_.state().phase == GamePhase::HumanTurn || game_.state().phase == GamePhase::ComputerTurn;
}

void SfmlApplication::resize() {
    const sf::Vector2u size = window_.getSize();
    if (size.x == 0 || size.y == 0) return;
    const float scale = min(static_cast<float>(size.x) / 1100.f, static_cast<float>(size.y) / 780.f);
    const float width = 1100.f * scale / static_cast<float>(size.x);
    const float height = 780.f * scale / static_cast<float>(size.y);
    sf::View view(sf::FloatRect({0.f, 0.f}, {1100.f, 780.f}));
    view.setViewport(sf::FloatRect({(1.f-width)/2.f, (1.f-height)/2.f}, {width, height}));
    window_.setView(view);
    const bool home = screen_ == Screen::Home;
    const auto top = window_.mapCoordsToPixel({home ? 72.f : 114.f, home ? 429.f : 694.f});
    const auto bottom = window_.mapCoordsToPixel({home ? 526.f : 605.f, home ? 477.f : 729.f});
    input_.setBounds(top.x, top.y, bottom.x-top.x, bottom.y-top.y);
}

void SfmlApplication::syncInput() {
    const bool home = screen_ == Screen::Home;
    if (home != inputHome_) { inputHome_ = home; resize(); }
    const bool visible = dialog_ == Dialog::None && !paused_ && (home ||
        (screen_ == Screen::Game && game_.state().phase != GamePhase::Finished));
    const bool enabled = visible && (home || game_.state().phase == GamePhase::HumanTurn);
    const bool focusNeeded = enabled && !inputEnabled_;
    if (visible != inputVisible_) { input_.setVisible(visible); inputVisible_ = visible; }
    if (enabled != inputEnabled_) { input_.setEnabled(enabled); inputEnabled_ = enabled; }
    if (focusNeeded) input_.focus();
}

void SfmlApplication::startGame() {
    if (input_.isComposing()) { notice_ = "이름의 한글 조합을 확정한 뒤 시작해 주세요."; return; }
    if (screen_ == Screen::Home) playerName_ = Utf8Util::trimAscii(input_.textUtf8());
    if (!RecordManager::validName(playerName_)) {
        notice_ = "이름은 한글·영문·숫자·공백·_로 1~12글자 입력하세요."; noticeError_ = true; return;
    }
    if (settings_.koreanOnly && dictionary_.koreanSize() == 0) {
        notice_ = "data/korean_words.txt에 우리말 허용 단어를 준비해 주세요."; noticeError_ = true; return;
    }
    const auto profileStatus = profiles_.selectOrCreate(playerName_);
    if (!game_.startNewGame(settings_, playerName_, uiTest_ ? "사과" : "")) {
        notice_ = "이어갈 수 있는 시작 단어가 없어요. 사전의 단어 연결을 확인해 주세요.";
        noticeError_ = true;
        return;
    }
    screen_ = Screen::Game;
    const bool profileSaved = profileStatus == ProfileManager::SelectStatus::Selected ||
        profileStatus == ProfileManager::SelectStatus::Created;
    notice_ = profileSaved ? "컴퓨터가 먼저 시작했어요. 끝 음절을 이어 주세요!" :
        "컴퓨터가 먼저 시작했어요. 프로필 목록은 저장하지 못했지만 게임은 정상 진행됩니다.";
    noticeError_ = !profileSaved;
    moves_.clear();
    moves_.push_back({false, game_.state().lastWord});
    historyScroll_ = 0;
    resultAnimationStarted_ = false;
    animatedResult_ = GameResult::None;
    resultParticles_.clear();
    roundWinImpact_ = false;
    paused_ = false;
    input_.clear();
    syncInput();
    input_.focus();
}

void SfmlApplication::submit() {
    if (game_.state().phase != GamePhase::HumanTurn) return;
    if (input_.isComposing()) {
        notice_ = "한글 조합을 마친 뒤 Enter 또는 제출을 눌러 주세요.";
        noticeError_ = false;
        return;
    }
    const auto result = game_.submitWord(input_.textUtf8());
    if (!result.valid()) {
        notice_ = errorText(result.code);
        noticeError_ = result.code != ValidationCode::Empty;
        input_.focus();
        return;
    }
    moves_.push_back({true, result.word});
    historyScroll_ = 0;
    notice_ = "정답! +" + to_string(game_.state().lastAward.total()) + "점 · 컴퓨터 차례예요.";
    noticeError_ = false;
    input_.clear();
    computerClock_.restart();
}
void SfmlApplication::updateComputer() {
    if (dialog_ != Dialog::None || paused_ || game_.state().phase != GamePhase::ComputerTurn) return;
    if (computerClock_.getElapsedTime().asSeconds() < 0.1f) return;
    const int previousDefeats = game_.state().computerDefeats;
    game_.playComputerTurn();
    if (game_.state().phase == GamePhase::HumanTurn)
        moves_.push_back({false, game_.state().lastWord});
    historyScroll_ = 0;
    notice_ = game_.state().computerDefeats > previousDefeats ?
        "컴퓨터 패배! +1,000점 · 새 제시어로 계속해요." :
        "내 차례예요. 새로운 단어로 이어 주세요.";
    if (game_.state().computerDefeats > previousDefeats) {
        roundWinImpact_ = true;
        roundWinClock_.restart();
    }
    noticeError_ = false;
    syncInput();
    if (game_.state().phase == GamePhase::HumanTurn) input_.focus();
}

void SfmlApplication::pauseGame() {
    if (!activeGame() || dialog_ != Dialog::None || paused_) return;
    paused_ = true;
    syncInput();
}

void SfmlApplication::resumeGame() {
    if (!paused_ || dialog_ != Dialog::None) return;
    paused_ = false;
    if (game_.state().phase == GamePhase::ComputerTurn) computerClock_.restart();
    syncInput();
    if (inputEnabled_) input_.focus();
}

void SfmlApplication::click(const string& id) {
    if (id == "dialog_cancel") { dialog_ = Dialog::None; syncInput(); if (inputEnabled_) input_.focus(); return; }
    if (id == "dialog_confirm") { confirmDialog(); return; }
    if (dialog_ != Dialog::None) return;
    if (id == "pause_resume") { resumeGame(); return; }
    if (id == "pause_surrender") { dialog_ = Dialog::Leave; syncInput(); return; }
    if (paused_) return;
    if (screen_ == Screen::Error && id != "exit") return;
    if (id == "start" || id == "restart") startGame();
    else if (id == "submit") submit();
    else if (id == "pause") pauseGame();
    else if (id == "surrender") dialog_ = Dialog::Leave;
    else if (id == "home") {
        if (activeGame()) dialog_ = Dialog::Leave;
        else { screen_ = Screen::Home; input_.setTextUtf8(playerName_); notice_.clear(); }
    } else if (id == "records" && !activeGame()) {
        rankingDifficulty_ = screen_ == Screen::Game ? game_.state().settings.difficulty : settings_.difficulty;
        screen_ = Screen::Records;
        recordPage_ = 0;
    }
    else if (id == "profiles" && !activeGame()) {
        screen_ = Screen::Profiles;
        profilePage_ = 0;
        notice_.clear();
    }
    else if (id == "rules" && !activeGame()) screen_ = Screen::Rules;
    else if (id == "easy") settings_.difficulty = Difficulty::Easy;
    else if (id == "normal") settings_.difficulty = Difficulty::Normal;
    else if (id == "hard") settings_.difficulty = Difficulty::Hard;
    else if (id == "toggle_one_shot") settings_.allowOneShot = !settings_.allowOneShot;
    else if (id == "toggle_korean") settings_.koreanOnly = !settings_.koreanOnly;
    else if (id == "toggle_three") settings_.threeSyllables = !settings_.threeSyllables;
    else if (id == "rank_easy") { rankingDifficulty_ = Difficulty::Easy; recordPage_ = 0; }
    else if (id == "rank_normal") { rankingDifficulty_ = Difficulty::Normal; recordPage_ = 0; }
    else if (id == "rank_hard") { rankingDifficulty_ = Difficulty::Hard; recordPage_ = 0; }
    else if (id == "records_prev") recordPage_ = max(0, recordPage_ - 1);
    else if (id == "records_next") recordPage_ = min(recordPage_ + 1,
        max(0, static_cast<int>((records_.rankings(rankingDifficulty_).size() + 6) / 7) - 1));
    else if (id == "profiles_prev") profilePage_ = max(0, profilePage_ - 1);
    else if (id == "profiles_next") profilePage_ = min(profilePage_ + 1,
        max(0, static_cast<int>((profileSummaries().size() + 3) / 4) - 1));
    else if (id == "new_profile") {
        playerName_.clear();
        input_.clear();
        notice_ = "새 이름을 입력하고 게임을 시작하면 프로필이 만들어져요.";
        noticeError_ = false;
        screen_ = Screen::Home;
    }
    else if (id.rfind("select_profile_", 0) == 0) {
        try {
            const size_t index = static_cast<size_t>(stoul(id.substr(15)));
            const auto summaries = profileSummaries();
            if (index < summaries.size()) {
                const auto result = profiles_.selectOrCreate(summaries[index].name);
                if (result == ProfileManager::SelectStatus::Selected || result == ProfileManager::SelectStatus::Created) {
                    playerName_ = summaries[index].name;
                    input_.setTextUtf8(playerName_);
                    profilePage_ = 0;
                    notice_ = playerName_ + " 프로필을 선택했어요.";
                    noticeError_ = false;
                } else {
                    notice_ = "프로필 선택을 저장하지 못했어요.";
                    noticeError_ = true;
                }
            }
        } catch (...) {}
    }
    else if (id == "reset_records") dialog_ = Dialog::ResetRecords;
    else if (id == "exit") requestClose();
    syncInput();
}

void SfmlApplication::requestClose() {
    if (activeGame()) dialog_ = Dialog::Close;
    else window_.close();
}

void SfmlApplication::confirmDialog() {
    const auto action = dialog_;
    dialog_ = Dialog::None;
    if (action == Dialog::Leave) {
        paused_ = false;
        game_.surrender();
        notice_.clear();
        screen_ = Screen::Game;
    } else if (action == Dialog::Close) {
        paused_ = false;
        game_.abort();
        if (game_.state().saveStatus != SaveStatus::Ok) {
            notice_ = "중단 기록을 저장하지 못했어요. 기존 기록 파일은 보존됩니다.";
            noticeError_ = true;
            screen_ = Screen::Game;
        } else window_.close();
    } else if (action == Dialog::ResetRecords) {
        const auto result = records_.resetRecords();
        notice_ = result == SaveStatus::Ok ? "기존 파일을 백업하고 기록을 초기화했어요." : "기록을 초기화하지 못했어요. 파일 경로를 확인해 주세요.";
        noticeError_ = result != SaveStatus::Ok;
    }
    syncInput();
}

string SfmlApplication::resultText(GameResult result) const {
    if (result == GameResult::Win) return "승리";
    if (result == GameResult::Lose) return "패배";
    return "중단";
}

string SfmlApplication::errorText(ValidationCode code) const {
    switch (code) {
    case ValidationCode::Empty: return "입력칸에 단어를 적어 주세요. 빈 입력은 기회를 사용하지 않아요.";
    case ValidationCode::InvalidEncoding: return "문자를 읽지 못했어요. 한글로 다시 입력해 주세요.";
    case ValidationCode::InvalidCharacters: return "공백·숫자·자음만 있는 글자 대신 완성된 한글 단어를 입력해 주세요.";
    case ValidationCode::TooShort: return "두 음절 이상의 단어를 입력해 주세요.";
    case ValidationCode::TooLong: return "단어는 64음절까지 입력할 수 있어요.";
    case ValidationCode::NotInDictionary: return "현재 사전에 없는 단어예요. 다른 단어로 다시 시도해 주세요.";
    case ValidationCode::AlreadyUsed: return "이미 사용한 단어예요. 새로운 단어를 입력해 주세요.";
    case ValidationCode::WrongStart: return "직전 단어의 마지막 음절과 같은 음절로 시작해야 해요.";
    case ValidationCode::NotKoreanWord: return "우리말 허용 사전에 등록되지 않은 단어예요.";
    case ValidationCode::MustBeThreeSyllables: return "쿵쿵따에서는 정확히 세 음절 단어를 입력하세요.";
    case ValidationCode::OneShotNotAllowed: return "이어갈 단어가 없는 한방 단어는 금지되어 있어요.";
    default: return "지금은 단어를 제출할 수 없어요.";
    }
}

vector<SfmlApplication::ProfileSummary> SfmlApplication::profileSummaries() const {
    vector<ProfileSummary> summaries;
    const auto append = [&](const string& name) {
        if (!RecordManager::validName(name)) return;
        const auto found = find_if(summaries.begin(), summaries.end(), [&](const ProfileSummary& item) {
            return item.name == name;
        });
        if (found == summaries.end()) {
            ProfileSummary summary;
            summary.name = name;
            summary.active = profiles_.hasActiveProfile() ? profiles_.activeProfile() == name : playerName_ == name;
            summaries.push_back(std::move(summary));
        }
    };
    for (const auto& name : profiles_.profiles()) append(name);
    for (const auto& record : records_.records()) append(record.name);
    for (const auto& record : records_.records()) {
        const auto found = find_if(summaries.begin(), summaries.end(), [&](const ProfileSummary& item) {
            return item.name == record.name;
        });
        if (found == summaries.end() || (record.result != GameResult::Win && record.result != GameResult::Lose)) continue;
        ++found->games;
        if (record.result == GameResult::Win) ++found->wins;
        else ++found->losses;
        found->bestScore = max(found->bestScore, record.score);
        found->totalScore += record.score;
    }
    return summaries;
}

void SfmlApplication::panel(float x,float y,float w,float h,sf::Color color,float radius) {
    const float r = min(radius, min(w,h)/2.f);
    sf::RectangleShape center({w-2.f*r,h});
    center.setPosition({x+r,y}); center.setFillColor(color); window_.draw(center);
    sf::RectangleShape middle({w,h-2.f*r});
    middle.setPosition({x,y+r}); middle.setFillColor(color); window_.draw(middle);
    sf::CircleShape corner(r,64);
    corner.setFillColor(color);
    for (const auto position : vector<sf::Vector2f>{{x,y},{x+w-2.f*r,y},{x,y+h-2.f*r},{x+w-2.f*r,y+h-2.f*r}}) {
        corner.setPosition(position); window_.draw(corner);
    }
}

void SfmlApplication::shadowPanel(float x,float y,float w,float h,sf::Color color,float radius) {
    panel(x + 3.f, y + 7.f, w, h, sf::Color(42, 39, 82, 22), radius);
    panel(x, y, w, h, color, radius);
}

void SfmlApplication::drawText(const sf::Text& original) {
    const sf::View logicalView=window_.getView();
    const auto viewport=window_.getViewport(logicalView);
    const float pixelsPerUnit=static_cast<float>(viewport.size.y)/logicalView.getSize().y;
    sf::Text text=original;
    const unsigned pixelSize=max(1u,static_cast<unsigned>(round(original.getCharacterSize()*pixelsPerUnit)));
    text.setCharacterSize(pixelSize);

    const auto oldBounds=original.getLocalBounds();
    const auto newBounds=text.getLocalBounds();
    const auto oldOrigin=original.getOrigin();
    const float anchorX=oldBounds.size.x>0 ? (oldOrigin.x-oldBounds.position.x)/oldBounds.size.x : 0.f;
    const float anchorY=oldBounds.size.y>0 ? (oldOrigin.y-oldBounds.position.y)/oldBounds.size.y : 0.f;
    const sf::Vector2f origin={newBounds.position.x+newBounds.size.x*anchorX,newBounds.position.y+newBounds.size.y*anchorY};
    text.setOrigin({round(origin.x),round(origin.y)});
    text.setScale(original.getScale());
    text.setOutlineThickness(round(original.getOutlineThickness()*pixelsPerUnit));
    const auto position=window_.mapCoordsToPixel(original.getPosition(),logicalView);
    text.setPosition({static_cast<float>(position.x),static_cast<float>(position.y)});
    // 글꼴을 실제 화면 픽셀 크기로 생성하고 1:1로 표시해 재축소 필터를 없앤다.
    window_.setView(sf::View(sf::FloatRect({0.f,0.f},sf::Vector2f(window_.getSize()))));
    window_.draw(text);
    window_.setView(logicalView);
}
void SfmlApplication::label(float x,float y,unsigned size,const string& value,sf::Color color) {
    sf::Text text(font_, unicode(value), size);
    text.setFillColor(color);
    text.setOrigin(text.getLocalBounds().position);
    text.setPosition({x,y});
    drawText(text);
}

void SfmlApplication::centeredLabel(float centerX,float y,unsigned size,const string& value,sf::Color color) {
    sf::Text text(font_, unicode(value), size);
    text.setFillColor(color);
    const auto bounds = text.getLocalBounds();
    text.setOrigin({bounds.position.x + bounds.size.x / 2.f, bounds.position.y});
    text.setPosition({centerX, y});
    drawText(text);
}

void SfmlApplication::centeredAt(float x,float y,unsigned size,const string& value,sf::Color color) {
    sf::Text text(font_,unicode(value),size);
    text.setFillColor(color);
    const auto bounds=text.getLocalBounds();
    text.setOrigin(bounds.position+bounds.size/2.f);
    text.setPosition({x,y});
    drawText(text);
}
void SfmlApplication::wrapped(float x,float y,float width,unsigned size,const string& value,sf::Color color) {
    sf::String line;
    const sf::String content = unicode(value);
    float top = y;
    for (const char32_t character : content) {
        if (character == U'\n') {
            label(x,top,size,utf8(line),color);
            line.clear(); top += static_cast<float>(size)*1.6f;
            continue;
        }
        sf::String proposed = line;
        proposed += character;
        const sf::Text measured(font_, proposed, size);
        if (!line.isEmpty() && measured.getLocalBounds().size.x > width) {
            label(x,top,size,utf8(line),color);
            line.clear(); top += static_cast<float>(size)*1.6f;
        }
        line += character;
    }
    if (!line.isEmpty()) label(x,top,size,utf8(line),color);
}

void SfmlApplication::button(const string& id,float x,float y,float w,float h,const string& caption,bool enabled,bool primary) {
    const sf::FloatRect bounds({x,y},{w,h});
    const bool overlayButton = id.rfind("dialog_",0) == 0 || id.rfind("pause_",0) == 0;
    const bool hover = enabled && bounds.contains(mouse_) &&
        ((dialog_ == Dialog::None && !paused_) || overlayButton);
    const sf::Color fill = !enabled ? sf::Color(233,234,242) : primary ?
        (hover ? purpleDark : purple) : (hover ? lavender : sf::Color(248,248,252));
    if (primary && enabled) panel(x + 2.f, y + 5.f, w, h, sf::Color(55, 39, 145, 28), 12.f);
    panel(x,y,w,h,fill,12.f);
    if (!primary && enabled) {
        sf::RectangleShape underline({w - 24.f, 2.f});
        underline.setPosition({x + 12.f, y + h - 3.f});
        underline.setFillColor(hover ? purple : sf::Color(0,0,0,0));
        window_.draw(underline);
    }
    unsigned captionSize=h >= 64.f ? 26 : 18;
    while (captionSize>12 && sf::Text(font_,unicode(caption),captionSize).getLocalBounds().size.x>w-22.f)
        --captionSize;
    sf::Text text(font_, unicode(caption),captionSize);
    text.setFillColor(!enabled ? sf::Color(160,162,180) : primary ? white : ink);
    const auto textBounds = text.getLocalBounds();
    text.setOrigin(textBounds.position + textBounds.size/2.f);
    const bool enterIcon=id == "submit" && caption == "제출";
    text.setPosition({x+w/2.f-(enterIcon ? 16.f : 0.f),y+h/2.f});
    drawText(text);
    if (enterIcon) {
        // 글꼴에 엔터 기호가 없어도 빈 사각형 대신 항상 같은 화살표를 표시한다.
        const float cx=x+w/2.f+24.f, cy=y+h/2.f;
        const sf::Vector2f points[]={{cx+10.f,cy-10.f},{cx+10.f,cy+5.f},
            {cx+10.f,cy+5.f},{cx-10.f,cy+5.f},{cx-10.f,cy+5.f},{cx-3.f,cy-2.f},
            {cx-10.f,cy+5.f},{cx-3.f,cy+12.f}};
        for (size_t i=0;i<8;i+=2) {
            const auto delta=points[i+1]-points[i];
            sf::RectangleShape line({sqrt(delta.x*delta.x+delta.y*delta.y),2.f});
            line.setOrigin({0.f,1.f}); line.setPosition(points[i]);
            line.setRotation(sf::radians(atan2(delta.y,delta.x)));
            line.setFillColor(text.getFillColor()); window_.draw(line);
        }
    }
    buttons_.push_back({id,bounds,enabled});
}

void SfmlApplication::badge(float x,float y,const string& value,sf::Color color) {
    const float width = max(98.f, sf::Text(font_, unicode(value), 14).getLocalBounds().size.x + 32.f);
    panel(x,y,width,32,color,16);
    centeredAt(x+width/2.f,y+16.f,14,value,ink);
}

void SfmlApplication::drawLogo(float x,float y,float width,bool iconOnly) {
    (void)iconOnly;
    if (!logoLoaded_) return;
    const float scale=width/192.f;
    // 원본의 세 꼭짓점 왕관과 보라색 배지를 화면 도형으로 표시한다.
    // 작은 PNG를 확대하지 않으므로 창 크기가 달라져도 윤곽이 선명하다.
    panel(x+16.f*scale,y+42.f*scale,43.f*scale,33.f*scale,sf::Color(55,35,112,25),10.f*scale);
    sf::ConvexShape crown(7);
    const sf::Vector2f points[]={{10.5f,18.f},{24.f,30.f},{37.5f,5.f},{51.f,30.f},{64.5f,18.f},{58.f,57.f},{17.f,57.f}};
    for (size_t i=0;i<7;++i) crown.setPoint(i,{x+points[i].x*scale,y+points[i].y*scale});
    crown.setFillColor(sf::Color(255,211,74));
    crown.setOutlineColor(sf::Color(237,164,40));
    crown.setOutlineThickness(1.4f*scale);
    window_.draw(crown);
    panel(x+16.f*scale,y+39.f*scale,43.f*scale,33.f*scale,purple,10.f*scale);
    centeredLabel(x+37.5f*scale,y+42.f*scale,static_cast<unsigned>(round(29.f*scale)),"끗",white);
    const sf::Vector2f tips[]={{10.5f,18.f},{37.5f,5.f},{64.5f,18.f}};
    const sf::Color colors[]={sf::Color(255,108,132),purple,sf::Color(48,204,169)};
    for (size_t i=0;i<3;++i) {
        sf::CircleShape tip(3.4f*scale,48);
        tip.setOrigin({3.4f*scale,3.4f*scale});
        tip.setPosition({x+tips[i].x*scale,y+tips[i].y*scale});
        tip.setFillColor(colors[i]); window_.draw(tip);
    }
    auto logoText=[&](float left,float top,unsigned size,const string& value,sf::Color color) {
        sf::Text text(font_,unicode(value),static_cast<unsigned>(ceil(size*scale)));
        text.setFillColor(color);
        text.setOrigin(text.getLocalBounds().position);
        text.setPosition({x+left*scale,y+top*scale});
        drawText(text);
    };
    logoText(79.f,18.f,25,"끗마릳기",sf::Color(17,18,49));
    logoText(79.f,49.f,9,"말끝의 왕을 가린다",purple);
}

void SfmlApplication::drawMascot(float centerX,float centerY,float scale,bool computer,bool active) {
    sf::CircleShape halo(64.f*scale,48);
    halo.setOrigin({64.f*scale,64.f*scale});
    halo.setPosition({centerX,centerY});
    halo.setFillColor(computer ? sf::Color(255,255,255,178) : sf::Color(255,255,255,198));
    halo.setOutlineThickness(active ? 4.f*scale : 2.f*scale);
    halo.setOutlineColor(active ? (computer ? coral : mint) : border);
    window_.draw(halo);
    if (computer) {
        panel(centerX-43.f*scale,centerY-35.f*scale,86.f*scale,72.f*scale,
            sf::Color(75,68,116),18.f*scale);
        sf::RectangleShape antenna({4.f*scale,18.f*scale});
        antenna.setOrigin({2.f*scale,18.f*scale}); antenna.setPosition({centerX,centerY-34.f*scale});
        antenna.setFillColor(sf::Color(75,68,116)); window_.draw(antenna);
        sf::CircleShape tip(7.f*scale,24); tip.setOrigin({7.f*scale,7.f*scale});
        tip.setPosition({centerX,centerY-57.f*scale}); tip.setFillColor(coral); window_.draw(tip);
        for (float offset : {-20.f,20.f}) {
            sf::CircleShape eye(6.f*scale,24); eye.setOrigin({6.f*scale,6.f*scale});
            eye.setPosition({centerX+offset*scale,centerY-6.f*scale}); eye.setFillColor(mint); window_.draw(eye);
        }
        sf::RectangleShape mouth({30.f*scale,4.f*scale}); mouth.setOrigin({15.f*scale,2.f*scale});
        mouth.setPosition({centerX,centerY+17.f*scale}); mouth.setFillColor(sf::Color(218,214,239)); window_.draw(mouth);
    } else {
        sf::CircleShape head(42.f*scale,48); head.setOrigin({42.f*scale,42.f*scale});
        head.setPosition({centerX,centerY}); head.setFillColor(sf::Color(255,218,177)); window_.draw(head);
        sf::CircleShape hair(43.f*scale,48); hair.setOrigin({43.f*scale,43.f*scale});
        hair.setPosition({centerX,centerY-9.f*scale}); hair.setScale({1.f,.55f});
        hair.setFillColor(purpleDark); window_.draw(hair);
        for (float offset : {-14.f,14.f}) {
            sf::CircleShape eye(4.f*scale,20); eye.setOrigin({4.f*scale,4.f*scale});
            eye.setPosition({centerX+offset*scale,centerY+3.f*scale}); eye.setFillColor(ink); window_.draw(eye);
        }
        sf::CircleShape smile(13.f*scale,28); smile.setOrigin({13.f*scale,13.f*scale});
        smile.setPosition({centerX,centerY+13.f*scale}); smile.setFillColor(sf::Color::Transparent);
        smile.setOutlineColor(coral); smile.setOutlineThickness(2.f*scale); window_.draw(smile);
        sf::RectangleShape cover({32.f*scale,13.f*scale}); cover.setOrigin({16.f*scale,6.f*scale});
        cover.setPosition({centerX,centerY+6.f*scale}); cover.setFillColor(sf::Color(255,218,177)); window_.draw(cover);
    }
}

void SfmlApplication::drawBackground() {
    if (screen_ == Screen::Game && referenceLoaded_) {
        // 시안의 캐릭터·배경·장식은 원본 그대로. 모든 게임 정보는 drawGame에서 덮어 그린다.
        sf::Sprite reference(referenceTexture_);
        reference.setScale({1100.f / referenceTexture_.getSize().x,780.f / referenceTexture_.getSize().y});
        window_.draw(reference);
        return;
    }
    for (int row = 0; row < 156; ++row) {
        const float ratio = static_cast<float>(row) / 155.f;
        sf::RectangleShape stripe({1100.f,5.f});
        stripe.setPosition({0.f,row*5.f});
        stripe.setFillColor(sf::Color(static_cast<uint8_t>(213-21*ratio),
            static_cast<uint8_t>(209-24*ratio),255));
        window_.draw(stripe);
    }
    for (int i=0;i<9;++i) {
        sf::CircleShape cloud(100.f+i*9.f,64);
        cloud.setPosition({i%2 ? 890.f+(i%3)*32.f : -100.f+(i%3)*34.f,150.f+i*79.f});
        cloud.setFillColor(sf::Color(249,248,255,45)); window_.draw(cloud);
    }
    for (int i=0;i<12;++i) {
        sf::Text tile(font_,unicode(i%3==0 ? "끗" : i%3==1 ? "ㄱ" : "ㅁ"),44);
        tile.setFillColor(sf::Color(126,101,244,35));
        tile.setPosition({float((i*247+39)%1100),float(140+(i*113)%580)});
        tile.setRotation(sf::degrees(float(i%2 ? 21 : -20))); drawText(tile);
    }
}

void SfmlApplication::drawHeader() {
    panel(0,0,1100,85,white,0);
    const vector<pair<string,string>> navigation{{"home","홈"},{"records","랭킹"},{"rules","규칙"},{"profiles","프로필"}};
    float x=735.f;
    for (const auto& item:navigation) {
        const bool selected = (item.first=="home" && (screen_==Screen::Home || screen_==Screen::Game)) ||
            (item.first=="records" && screen_==Screen::Records) ||
            (item.first=="rules" && screen_==Screen::Rules) ||
            (item.first=="profiles" && screen_==Screen::Profiles);
        const bool enabled = screen_!=Screen::Error && (!activeGame() || item.first=="home");
        const sf::FloatRect bounds({x,20},{80,48});
        centeredLabel(x+40,34,21,item.second,selected ? purple : enabled ? ink : muted);
        if (selected) panel(x+17,61,46,4,purple,2);
        else if (enabled && bounds.contains(mouse_)) panel(x+24,64,32,3,lavender,1);
        buttons_.push_back({item.first,bounds,enabled});
        x+=90;
    }
}

void SfmlApplication::drawHome() {
    shadowPanel(32,122,674,594,white,24);
    panel(32,122,674,8,purple,4);
    panel(66,151,612,145,lavender,28);
    drawLogo(202,165,334,false);
    if (referenceLoaded_) {
        // 게임 화면과 같은 캐릭터를 홈의 브랜드 카드에도 표시한다.
        sf::CircleShape computer(44.f,64);
        computer.setTexture(&referenceTexture_);
        computer.setTextureRect(sf::IntRect({42,136},{330,290}));
        computer.setOutlineColor(white); computer.setOutlineThickness(3.f);
        computer.setPosition({85.f,187.f}); window_.draw(computer);
        sf::CircleShape player(44.f,64);
        player.setTexture(&referenceTexture_);
        player.setTextureRect(sf::IntRect({42,580},{330,285}));
        player.setOutlineColor(white); player.setOutlineThickness(3.f);
        player.setPosition({573.f,187.f}); window_.draw(player);
    }
    centeredLabel(369,320,25,"말끝의 왕좌에 도전하세요",ink);
    centeredLabel(369,355,14,"세 번의 기회, 한 글자 차이로 갈리는 컴퓨터와의 단어 대결",muted);
    label(68,399,15,"현재 플레이어",ink);
    label(199,401,13,"한글·영문·숫자 1~12글자",muted);
    panel(64,421,470,64,border,13);
    panel(67,424,464,58,white,11);
    if (!inputVisible_) label(72,429,20,input_.textUtf8().empty() ? "플레이어 이름을 입력하세요" : input_.textUtf8(),ink);
    button("profiles",548,421,130,64,"프로필 보기");
    button("start",66,511,612,62,"왕좌에 도전하기  →",true,true);
    panel(66,592,612,86,mintSoft,20);
    sf::CircleShape scoreIcon(23.f,32); scoreIcon.setFillColor(sf::Color(255,255,255,220)); scoreIcon.setPosition({84.f,611.f}); window_.draw(scoreIcon);
    centeredAt(107,634,18,"+",purple);
    label(142,602,15,"점수 공식",ink);
    label(142,626,18,"기본 100 + 기회 보너스 + 글자 수 × 20",sf::Color(0,106,90));
    label(142,652,15,"두음법칙은 모든 게임에 자동으로 적용됩니다",ink);

    shadowPanel(726,122,342,594,white,24);
    panel(726,122,342,8,purple,4);
    label(754,154,23,"게임 설정",ink);
    label(754,193,14,"내 플레이에 맞게 골라 보세요",muted);
    label(754,237,14,"난이도",muted);
    button("easy",754,263,88,44,settings_.difficulty == Difficulty::Easy ? "● 쉬움" : "쉬움",true,settings_.difficulty == Difficulty::Easy);
    button("normal",850,263,88,44,settings_.difficulty == Difficulty::Normal ? "● 보통" : "보통",true,settings_.difficulty == Difficulty::Normal);
    button("hard",946,263,94,44,settings_.difficulty == Difficulty::Hard ? "● 어려움" : "어려움",true,settings_.difficulty == Difficulty::Hard);
    wrapped(756,319,278,14,settings_.difficulty == Difficulty::Easy ?
        "짧고 익숙한 단어를 주로 골라요." :
        settings_.difficulty == Difficulty::Normal ? "익숙한 단어를 고르게 섞어 골라요." :
        "이어갈 답이 적은 단어도 활용해요.",muted);
    label(754,371,14,"게임 방식",muted);
    button("toggle_one_shot",754,397,286,48,settings_.allowOneShot ? "● 한방단어 허용" : "○ 한방단어 금지",true,settings_.allowOneShot);
    button("toggle_korean",754,457,286,48,settings_.koreanOnly ? "● 우리말만 사용" : "○ 모든 등록 단어",true,settings_.koreanOnly);
    button("toggle_three",754,517,286,48,settings_.threeSyllables ? "● 쿵쿵따 · 세 글자" : "○ 일반 끝말잇기",true,settings_.threeSyllables);
    panel(754,592,286,86,lavender,14);
    label(772,603,16,"기본 규칙",purpleDark);
    wrapped(772,630,250,14,"매 턴 기회 3회\n컴퓨터 KO +1,000점 · 새 라운드",ink);
    if (!records_.canSave()) {
        label(34,746,14,"랭킹 저장이 중지되어 있어요. 파일을 확인하거나 복구해 주세요.",coral);
        button("reset_records",878,734,190,38,"기록 복구");
    } else {
        label(34,746,13,"등록 단어 " + to_string(dictionary_.size()) + "개 · 우리말 " + to_string(dictionary_.koreanSize()) + "개",muted);
        if (!notice_.empty()) wrapped(420,741,645,13,notice_,noticeError_ ? coral : muted);
    }
}

void SfmlApplication::drawGame() {
    const auto& state = game_.state();
    const bool finished = state.phase==GamePhase::Finished;
    const bool human = state.phase==GamePhase::HumanTurn;
    // 원본 시안의 좌표를 논리 해상도 1100×780에 맞춘다.
    const float s=1100.f/1490.f;
    auto card=[&](float x,float y,float w,float h,sf::Color fill,float r=28.f) {
        panel(x*s,y*s,w*s,h*s,fill,r*s);
    };
    if (!referenceLoaded_) {
        drawMascot(144,217,1.1f,true,!human);
        drawMascot(144,538,1.1f,false,human);
    }
    // 마지막 세 단어를 실제 사용 이력에서 표시한다. 휠로 이전 연결을 볼 수 있다.
    const int end=max(0,static_cast<int>(moves_.size())-historyScroll_);
    const int begin=max(0,end-3);
    const vector<sf::FloatRect> chain{
        {{438,171},{308,114}},{{495,350},{289,113}},{{487,529},{314,113}}
    };
    for (int slot=0;slot<3;++slot) {
        const auto& rect=chain[slot];
        card(rect.position.x,rect.position.y,rect.size.x,rect.size.y,white,44);
        const int index=begin+slot;
        const bool exists=index<end;
        sf::String word=exists ? unicode(moves_[static_cast<size_t>(index)].word) : unicode("···");
        if (word.getSize()>10) word=word.substring(0,8)+unicode("…")+word.substring(word.getSize()-1);
        const sf::Color accent=slot==2 ? sf::Color(171,248,221) : sf::Color(218,202,255);
        const float cx=(rect.position.x+rect.size.x-66)*s;
        const float cy=(rect.position.y+57)*s;
        sf::CircleShape syllable(50*s,64);
        syllable.setOrigin({50*s,50*s}); syllable.setPosition({cx,cy});
        syllable.setFillColor(exists ? accent : sf::Color(241,237,255)); window_.draw(syllable);
        unsigned size=static_cast<unsigned>(78*s);
        if (exists) {
            const sf::String prefix=word.substring(0,word.getSize()-1);
            const sf::String finalSyllable=word.substring(word.getSize()-1);
            while (size>16) {
                const float width=sf::Text(font_,prefix,size).getLocalBounds().size.x+
                    sf::Text(font_,finalSyllable,size).getLocalBounds().size.x/2.f+12*s;
                if (width<cx-rect.position.x*s-14*s) break;
                size-=2;
            }
            sf::Text wordText(font_,word,size);
            const auto bounds=wordText.getLocalBounds();
            const auto lastBounds=sf::Text(font_,finalSyllable,size).getLocalBounds();
            const float lastCenter=wordText.findCharacterPos(word.getSize()-1).x+lastBounds.position.x+lastBounds.size.x/2.f;
            wordText.setOrigin({lastCenter,bounds.position.y+bounds.size.y/2.f});
            wordText.setPosition({cx,cy});
            wordText.setFillColor(ink);
            drawText(wordText);
        } else {
            centeredLabel((rect.position.x+rect.size.x/2)*s,cy-5*s,28,"···",muted);
        }
    }
    // 시안의 플레이어 이름도 실제 프로필 이름으로 표시한다.
    card(111,809,174,56,sf::Color(8,159,143),28);
    centeredLabel(198*s,825*s,18,state.playerName,white);
    // 점수·난이도·KO를 실제 게임 값으로 표시한다.
    // 같은 위치·크기의 상태 카드를 재구성하고 원본 아이콘을 그대로 표시한다.
    const sf::Color cardWhite(250,250,255);
    card(939,145,266,138,cardWhite,24);
    card(1217,145,235,138,cardWhite,24);
    card(939,295,266,125,cardWhite,24);
    card(1217,295,235,125,cardWhite,24);
    auto artwork=[&](int x,int y,int w,int h) {
        if (!referenceLoaded_) return;
        sf::Sprite sprite(referenceTexture_,sf::IntRect({x,y},{w,h}));
        sprite.setPosition({x*s,y*s}); sprite.setScale({s,s}); window_.draw(sprite);
    };
    artwork(963,168,61,58);
    artwork(1241,170,53,49);
    artwork(966,321,43,48);
    artwork(1246,320,48,47);
    label(1034*s,180*s,16,"현재 점수",muted);
    string score=to_string(state.score);
    for (int i=static_cast<int>(score.size())-3;i>0;i-=3) score.insert(static_cast<size_t>(i),",");
    unsigned scoreSize=35;
    while (scoreSize>18 && sf::Text(font_,unicode(score+"점"),scoreSize).getLocalBounds().size.x>166*s) --scoreSize;
    label(1030*s,216*s,scoreSize,score+"점",purple);

    label(1310*s,181*s,16,"남은 기회",muted);
    for (int i=0;i<3;++i) {
        sf::CircleShape dot(13*s,40); dot.setPosition({(1315+i*38)*s,221*s});
        dot.setFillColor(i<state.remainingAttempts() && !finished ? coral : border);
        window_.draw(dot);
        sf::CircleShape shine(4*s,24); shine.setPosition({(1319+i*38)*s,224*s});
        shine.setFillColor(sf::Color(255,255,255,95)); window_.draw(shine);
    }

    label(1034*s,330*s,16,"난이도",muted);
    label(1034*s,367*s,26,difficultyName(state.settings.difficulty),sf::Color(0,147,120));

    label(1308*s,330*s,15,"컴퓨터 KO",muted);
    label(1315*s,367*s,28,to_string(state.computerDefeats),ink);
    // 오른쪽 안내는 입력 오류·AI 대기·일반 안내에 따라 바뀐다.
    card(946,498,501,147,sf::Color(223,255,247),32);
    card(956,508,481,127,cardWhite,27);
    artwork(948,453,120,88);
    const string message=noticeError_ ? notice_ : finished ? "한 판을 마쳤어요. 다시 도전해 볼까요?" :
        human ? "새로운 단어를 입력해 주세요." : "컴퓨터가 다음 단어를 생각하고 있어요.";
    wrapped(1020*s,(noticeError_ ? 545.f : 563.f)*s,400*s,noticeError_ ? 15 : 18,message,noticeError_ ? coral : ink);
    // 차례 카드도 실제 턴 상태에 맞춘다.
    card(395,706,591,146,human ? sf::Color(216,255,241) : lavender,42);
    centeredLabel(690*s,737*s,46,finished ? "게임 종료" : human ? "내 차례" : "컴퓨터 차례",
        human ? sf::Color(0,69,59) : purpleDark);
    char32_t first=0,last=0;size_t count=0;
    string starts;
    if (Utf8Util::wordParts(state.lastWord,first,last,count)) {
        for (const auto ch:rules_.allowedStarts(last)) {
            if (!starts.empty()) starts+=" / ";
            starts+=Utf8Util::encode(ch);
        }
    }
    centeredLabel(690*s,815*s,20,starts+"(으)로 시작하는 단어",sf::Color(0,146,128));
    // 하단 입력·제출·포기를 실제 컨트롤로 연결한다.
    card(29,892,1432,136,white,30);
    card(64,915,782,95,sf::Color(202,199,238),20);
    card(68,919,774,87,white,17);
        sf::RectangleShape pencil({8.f,26.f});
    pencil.setOrigin({4.f,13.f}); pencil.setPosition({96*s,967*s});
    pencil.setRotation(sf::degrees(40)); pencil.setFillColor(sf::Color(173,165,218));
    window_.draw(pencil);
    const string typed=input_.textUtf8();
    if (!inputVisible_) label(114,694,23,typed.empty() ? "단어를 입력하세요" : typed,typed.empty() ? sf::Color(170,170,194) : ink);
    button("submit",866*s,916*s,299*s,96*s,human ? "제출" : "생각 중",human,true);
    button("surrender",1198*s,916*s,236*s,96*s,"게임 포기",!finished,false);
    if (!finished) {
        // 시안의 빈 공간에 작은 일시정지 버튼. Esc로도 사용할 수 있다.
        button("pause",994,333,74,29,"Ⅱ  정지",true,false);
    }
    if (!finished && roundWinImpact_) drawRoundWinImpact();
    if (finished) drawResultCelebration();
}

void SfmlApplication::drawRoundWinImpact() {
    const float elapsed = roundWinClock_.getElapsedTime().asSeconds();
    if (elapsed >= 1.45f) {
        roundWinImpact_ = false;
        return;
    }
    const float fade = elapsed < 1.f ? 1.f : max(0.f, 1.f - (elapsed - 1.f) / .45f);
    const auto alpha = static_cast<uint8_t>(255.f * fade);
    if (elapsed < .32f) {
        sf::RectangleShape flash({1100.f,695.f});
        flash.setPosition({0.f,85.f});
        flash.setFillColor(sf::Color(mint.r,mint.g,mint.b,
            static_cast<uint8_t>((1.f - elapsed / .32f) * 44.f)));
        window_.draw(flash);
    }
    const float centerX = 377.f;
    const float centerY = 333.f;
    for (int i = 0; i < 18; ++i) {
        const float angle = static_cast<float>(i) * 20.f * 3.14159265f / 180.f;
        const float distance = 82.f + elapsed * (55.f + static_cast<float>(i % 4) * 9.f);
        sf::CircleShape spark(3.f + static_cast<float>(i % 3), i % 2 == 0 ? 4 : 20);
        spark.setPosition({centerX + cos(angle) * distance, centerY + sin(angle) * distance});
        const sf::Color color = i % 3 == 0 ? gold : i % 3 == 1 ? mint : purple;
        spark.setFillColor(sf::Color(color.r,color.g,color.b,static_cast<uint8_t>(210.f * fade)));
        window_.draw(spark);
    }
    const float lift = min(elapsed * 12.f, 10.f);
    panel(190.f,273.f-lift,374.f,126.f,sf::Color(37,34,70),20.f);
    panel(190.f,273.f-lift,374.f,7.f,mint,4.f);
    centeredLabel(377.f,290.f-lift,13,"컴퓨터 KO!",sf::Color(176,247,226,alpha));
    centeredLabel(377.f,315.f-lift,32,"+1,000점",sf::Color(255,255,255,alpha));
    centeredLabel(377.f,361.f-lift,14,"새 제시어로 바로 이어갑니다",sf::Color(210,211,230,alpha));
}

void SfmlApplication::startResultAnimation(GameResult result) {
    resultAnimationStarted_ = true;
    animatedResult_ = result;
    resultClock_.restart();
    resultParticles_.clear();
    const vector<sf::Color> winColors {purple, mint, gold, sf::Color(255,105,149), sf::Color(105,188,255)};
    const vector<sf::Color> loseColors {coral, sf::Color(255,153,120), sf::Color(174,158,199), sf::Color(118,121,154)};
    const auto& colors = result == GameResult::Win ? winColors : loseColors;
    for (int i = 0; i < 54; ++i) {
        const float speed = 125.f + static_cast<float>((i * 41) % 150);
        ResultParticle particle;
        const float lane = static_cast<float>(i / 4);
        if (i % 4 == 0) {
            particle.origin = {174.f, 150.f + fmod(lane * 41.f, 540.f)};
            particle.velocity = {-speed, -85.f + fmod(lane * 29.f, 150.f)};
        } else if (i % 4 == 1) {
            particle.origin = {926.f, 150.f + fmod(lane * 47.f, 540.f)};
            particle.velocity = {speed, -70.f + fmod(lane * 31.f, 145.f)};
        } else if (i % 4 == 2) {
            particle.origin = {205.f + fmod(lane * 59.f, 690.f), 126.f};
            particle.velocity = {-75.f + fmod(lane * 23.f, 150.f), -speed};
        } else {
            particle.origin = {205.f + fmod(lane * 61.f, 690.f), 726.f};
            particle.velocity = {-65.f + fmod(lane * 19.f, 130.f), speed * .45f};
        }
        particle.color = colors[static_cast<size_t>(i) % colors.size()];
        particle.size = 3.f + static_cast<float>(i % 5);
        particle.delay = static_cast<float>(i % 7) * 0.025f;
        resultParticles_.push_back(particle);
    }
}

void SfmlApplication::drawResultCelebration() {
    const auto& state = game_.state();
    if (!resultAnimationStarted_ || animatedResult_ != state.result) startResultAnimation(state.result);
    const float elapsed = resultClock_.getElapsedTime().asSeconds();
    const bool won = state.result == GameResult::Win;
    const sf::Color accent = won ? mint : coral;
    sf::RectangleShape veil({1100.f,695.f});
    veil.setPosition({0.f,85.f});
    veil.setFillColor(sf::Color(28,25,58,188));
    window_.draw(veil);
    for (const auto& particle : resultParticles_) {
        const float t = elapsed - particle.delay;
        if (t < 0.f) continue;
        const float cycle = fmod(t, 2.25f);
        const float x = particle.origin.x + particle.velocity.x * cycle;
        const float y = particle.origin.y + particle.velocity.y * cycle + 115.f * cycle * cycle;
        if (x < -20.f || x > 1120.f || y < 95.f || y > 800.f) continue;
        sf::CircleShape spark(particle.size, won ? 4 : 20);
        spark.setPosition({x,y});
        spark.setFillColor(sf::Color(particle.color.r,particle.color.g,particle.color.b,210));
        window_.draw(spark);
    }
    const float shake = !won && elapsed < .45f ? sin(elapsed * 55.f) * (1.f - elapsed / .45f) * 7.f : 0.f;
    const float cardX = 170.f + shake;
    shadowPanel(cardX,120,760,610,white,28);
    panel(cardX,120,760,10,accent,5);
    sf::CircleShape emblem(30.f,48);
    emblem.setFillColor(won ? mintSoft : coralSoft);
    emblem.setPosition({cardX + 38.f,151.f}); window_.draw(emblem);
    centeredAt(cardX + 68.f,181.f,28,won ? "★" : "!",accent);
    label(cardX + 116.f,148,12,won ? "VICTORY" : "GAME RESULT",accent);
    label(cardX + 116.f,173,25,won ? "승리했습니다" : "이번 게임이 종료됐어요",ink);
    string reason;
    switch (state.endReason) {
    case EndReason::ComputerStuck: reason = "컴퓨터가 더 이상 단어를 잇지 못했어요."; break;
    case EndReason::HumanStuck: reason = "사전에 이어갈 수 있는 단어가 남지 않았어요."; break;
    case EndReason::ThreeErrors: reason = "이번 턴의 세 번의 기회를 모두 사용했어요."; break;
    case EndReason::Surrender: reason = "이번 도전은 여기까지예요. 바로 다시 시작할 수 있어요."; break;
    default: reason = "진행하던 게임을 마쳤어요."; break;
    }
    label(cardX + 116.f,207,13,reason,muted);
    panel(cardX + 560.f,148,162,78,won ? mintSoft : coralSoft,16);
    centeredAt(cardX + 641.f,166,13,"FINAL SCORE",muted);
    centeredAt(cardX + 641.f,196,27,to_string(state.score) + "점",accent);

    sf::RectangleShape divider({700.f,1.f});
    divider.setPosition({cardX + 30.f,244.f}); divider.setFillColor(border); window_.draw(divider);
    label(cardX + 38.f,263,20,difficultyName(state.settings.difficulty) + " 랭킹",ink);
    label(cardX + 158.f,269,13,"선택한 난이도의 TOP 3와 내 순위",muted);

    const auto ranking = records_.rankings(state.settings.difficulty);
    const RankingEntry* mine = nullptr;
    for (const auto& item : ranking) {
        if (item.name == state.playerName && item.score == state.score &&
            item.difficulty == state.settings.difficulty) {
            mine = &item;
            break;
        }
    }
    const vector<sf::Color> medalColors {gold, sf::Color(143,151,177), sf::Color(200,126,81)};
    for (size_t row = 0; row < 3; ++row) {
        const float y = 302.f + static_cast<float>(row) * 57.f;
        panel(cardX + 30.f,y,700.f,47.f,row == 0 ? sf::Color(255,248,229) : sf::Color(247,248,252),12);
        sf::CircleShape medal(15.f);
        medal.setFillColor(sf::Color(medalColors[row].r,medalColors[row].g,medalColors[row].b,35));
        medal.setPosition({cardX + 49.f,y + 8.f}); window_.draw(medal);
        centeredAt(cardX + 64.f,y + 23.f,14,to_string(row + 1),medalColors[row]);
        if (row < ranking.size()) {
            const auto& item = ranking[row];
            label(cardX + 94.f,y + 11.f,16,item.name,ink);
            label(cardX + 440.f,y + 12.f,15,to_string(item.score) + "점",ink);
            centeredAt(cardX + 645.f,y + 23.5f,14,difficultyName(item.difficulty),muted);
        } else {
            label(cardX + 94.f,y + 13.f,14,"아직 기록이 없어요",muted);
        }
    }

    label(cardX + 38.f,480,13,"MY RANK",purpleDark);
    panel(cardX + 30.f,503,700.f,64.f,lavender,14);
    sf::CircleShape myMedal(19.f); myMedal.setFillColor(purple); myMedal.setPosition({cardX + 48.f,516.f}); window_.draw(myMedal);
    centeredAt(cardX + 67.f,535.f,13,mine ? to_string(mine->rank) : "-",white);
    centeredAt(cardX + 235.f,523,17,state.playerName,ink);
    centeredAt(cardX + 235.f,548,13,difficultyName(state.settings.difficulty) + " · 정답 " + to_string(state.humanMoves) + "개",muted);
    centeredAt(cardX + 478.f,535,13,mine ? "내 순위" : "저장 상태",muted);
    centeredAt(cardX + 570.f,535,20,mine ? to_string(mine->rank) + "위" : "확인 필요",purpleDark);
    centeredAt(cardX + 665.f,535,15,to_string(state.score) + "점",ink);

    buttons_.clear();
    button("restart",cardX + 30.f,592,337.f,56,"다시 도전  →",true,true);
    button("home",cardX + 383.f,592,347.f,56,"홈으로");
    centeredLabel(cardX + 380.f,679,13,state.saveStatus == SaveStatus::Ok ?
        "결과를 랭킹에 저장했어요." : "결과를 저장하지 못했어요. 파일을 확인해 주세요.",
        state.saveStatus == SaveStatus::Ok ? muted : coral);
    if (elapsed < .55f) {
        sf::RectangleShape flash({1100.f,695.f}); flash.setPosition({0.f,85.f});
        const auto alpha = static_cast<uint8_t>(max(0.f, 72.f - elapsed * 130.f));
        flash.setFillColor(sf::Color(accent.r,accent.g,accent.b,alpha)); window_.draw(flash);
    }
}

void SfmlApplication::drawProfiles() {
    const auto summaries = profileSummaries();
    const ProfileSummary* active = nullptr;
    for (const auto& item : summaries) {
        if (item.active) { active = &item; break; }
    }
    if (!active && !summaries.empty()) active = &summaries.front();

    shadowPanel(32,122,1036,150,purpleDark,24);
    panel(32,122,1036,8,gold,4);


    label(58,145,13,"ACTIVE CHAMPION",sf::Color(216,207,255));
    label(58,174,30,active ? active->name : "새 플레이어",white);
    label(58,217,14,active ? "이 프로필로 게임과 랭킹 기록을 이어갑니다" : "홈에서 이름을 입력해 첫 프로필을 만들어 보세요",sf::Color(224,220,244));
    panel(810,143,226,103,sf::Color(88,66,224),18);
    label(834,158,12,"PERSONAL BEST",sf::Color(218,211,245));
    label(834,182,31,active ? to_string(active->bestScore) + "점" : "-",gold);
    label(834,219,12,active ? "완료 경기 " + to_string(active->games) + "회" : "아직 기록이 없어요",sf::Color(230,226,246));

    shadowPanel(32,296,620,420,white,24);
    label(62,323,21,"플레이어 선택",ink);
    label(62,353,13,"프로필을 누르면 다음 게임의 이름과 기록이 바뀝니다",muted);
    button("new_profile",482,316,142,42,"+ 새 프로필");
    const size_t pageStart = static_cast<size_t>(profilePage_) * 4;
    for (size_t row=0;row<4;++row) {
        const size_t index = pageStart + row;
        const float y = 385.f + static_cast<float>(row) * 68.f;
        if (index >= summaries.size()) {
            if (row == 0) {
                panel(62,y,560,56,sf::Color(248,248,252),13);
                label(82,y+17,14,"저장된 프로필이 아직 없어요",muted);
            }
            continue;
        }
        const auto& item = summaries[index];
        const sf::FloatRect bounds({62.f,y},{560.f,56.f});
        const bool hover = bounds.contains(mouse_) && dialog_ == Dialog::None;
        const sf::Color fill = item.active ? lavender : hover ? sf::Color(245,243,255) : sf::Color(248,248,252);
        panel(62,y,560,56,fill,13);
        sf::CircleShape avatar(18.f,32); avatar.setFillColor(item.active ? purple : sf::Color(225,222,240)); avatar.setPosition({78.f,y+10.f}); window_.draw(avatar);
        centeredAt(96,y+28.f,14,item.active ? "★" : to_string(index+1),item.active ? white : muted);
        label(126,y+8,17,item.name,ink);
        label(126,y+33,12,item.active ? "선택됨" : "선택하기",item.active ? purple : muted);
        label(390,y+11,13,"최고 " + to_string(item.bestScore) + "점",ink);
        label(390,y+33,12,to_string(item.games) + "전 · " + to_string(item.wins) + "승",muted);
        buttons_.push_back({"select_profile_" + to_string(index),bounds,true});
    }
    const int profilePages = max(1,static_cast<int>((summaries.size()+3)/4));
    button("profiles_prev",62,669,86,34,"이전",profilePage_>0);
    centeredLabel(335,675,13,to_string(profilePage_+1) + " / " + to_string(profilePages),muted);
    button("profiles_next",536,669,86,34,"다음",profilePage_+1<profilePages);

    shadowPanel(676,296,392,420,white,24);
    label(706,323,13,"PROFILE RECORD",purple);
    label(706,350,23,active ? active->name + "의 기록" : "프로필 기록",ink);
    const vector<pair<string,string>> metrics {
        {"완료 경기",active ? to_string(active->games) + "회" : "0회"},
        {"승리",active ? to_string(active->wins) + "회" : "0회"},
        {"패배",active ? to_string(active->losses) + "회" : "0회"},
        {"최고 점수",active ? to_string(active->bestScore) + "점" : "0점"}
    };
    for (size_t i=0;i<metrics.size();++i) {
        const float x = 706.f + static_cast<float>(i%2)*168.f;
        const float y = 398.f + static_cast<float>(i/2)*112.f;
        panel(x,y,154,94,i==1 ? mintSoft : i==2 ? coralSoft : lavender,16);
        label(x+18,y+15,12,metrics[i].first,muted);
        label(x+18,y+43,22,metrics[i].second,i==1 ? mint : i==2 ? coral : purpleDark);
    }
    panel(706,628,322,54,sf::Color(248,248,252),13);
    label(724,644,13,"누적 점수",muted);
    label(890,640,20,active ? to_string(active->totalScore) + "점" : "0점",ink);
    label(34,746,13,!notice_.empty() ? notice_ : "프로필별 게임 기록은 기존 랭킹 결과에서 자동으로 집계됩니다",noticeError_ ? coral : muted);
    button("home",894,734,174,38,"홈으로");
}

void SfmlApplication::drawRecords() {
    shadowPanel(32,122,1036,128,purple,24);
    label(64,150,14,"LEADERBOARD",sf::Color(220,213,255));
    label(64,178,31,"끗마릳기 명예의 전당",white);
    label(618,188,15,"난이도별 높은 점수부터 · 같은 점수는 공동 등수",sf::Color(224,218,255));
    shadowPanel(32,274,1036,442,white,24);
    label(64,302,18,difficultyName(rankingDifficulty_) + " 랭킹",ink);
    button("rank_easy",668,289,108,40,"쉬움",true,rankingDifficulty_ == Difficulty::Easy);
    button("rank_normal",788,289,108,40,"보통",true,rankingDifficulty_ == Difficulty::Normal);
    button("rank_hard",908,289,108,40,"어려움",true,rankingDifficulty_ == Difficulty::Hard);
    panel(56,342,988,42,lavender,10);
    centeredAt(93,363,13,"RANK",muted);
    centeredAt(414,363,13,"PLAYER",muted);
    centeredAt(693,363,13,"SCORE",muted);
    centeredAt(899,363,13,"LEVEL",muted);
    const auto history=records_.rankings(rankingDifficulty_);
    if (history.empty()) {
        centeredLabel(550,464,21,difficultyName(rankingDifficulty_) + " 랭킹이 아직 비어 있어요",ink);
        centeredLabel(550,505,15,"이 난이도로 게임을 끝내고 이름을 올려 보세요.",muted);
    }
    for (int row=0;row<7;++row) {
        const size_t offset=static_cast<size_t>(recordPage_*7+row);
        if (offset>=history.size()) break;
        const auto& item=history[offset];
        const float y=397.f+static_cast<float>(row)*43.f;
        if (row % 2 == 1) panel(56,y-6,988,38,sf::Color(248,248,252),8);
        const sf::Color rankColor = item.rank == 1 ? gold : item.rank == 2 ? sf::Color(151,157,180) : item.rank == 3 ? sf::Color(201,126,81) : purple;
        sf::CircleShape medal(14.f); medal.setFillColor(sf::Color(rankColor.r,rankColor.g,rankColor.b,28)); medal.setPosition({79.f,y-1.f}); window_.draw(medal);
        centeredAt(93,y+13.f,14,to_string(item.rank),rankColor);
        centeredAt(414,y+13.f,17,item.name,ink);
        centeredAt(693,y+13.f,17,to_string(item.score)+"점",ink);
        badge(850,y-3,difficultyName(item.difficulty),lavender);
    }
    const int pages=max(1,static_cast<int>((history.size()+6)/7));
    label(34,746,13,"선택한 난이도에서 완료된 게임만 순위를 계산합니다.",muted);
    button("records_prev",792,734,86,38,"이전",recordPage_>0);
    label(897,745,14,to_string(recordPage_+1)+" / "+to_string(pages),muted);
    button("records_next",974,734,94,38,"다음",recordPage_+1<pages);
}

void SfmlApplication::drawRules() {
    shadowPanel(32,122,1036,594,white,24);
    label(62,151,13,"HOW TO PLAY",purple);
    label(62,179,29,"끗마릳기, 이렇게 플레이해요",ink);
    const vector<pair<string,string>> rules {
        {"01","컴퓨터가 낸 단어의 끝 음절로\n새 단어를 시작하세요.\n두음법칙은 항상 자동으로 적용됩니다."},
        {"02","사전에 있는 두 음절 이상의 한글 단어를\n사용합니다. 한 번 나온 단어는\n다시 쓸 수 없어요."},
        {"03","기본 100점 + 기회 보너스 + 음절 수×20점.\n매 턴 세 번의 제출 기회가 주어집니다."},
        {"04","한방단어·우리말만·쿵쿵따를\n홈에서 선택할 수 있어요.\n쿵쿵따는 정확히 세 글자를 씁니다."},
        {"05","컴퓨터가 막히면 +1,000점을 받고 새 라운드가 시작됩니다.\n점수는 그대로 이어집니다."}
    };
    const vector<string> titles {"끝 음절 잇기","사용 가능한 단어","기회와 점수","선택 규칙","새 라운드"};
    for (size_t i=0;i<rules.size();++i) {
        const bool last = i == 4;
        const float x = last ? 62.f : (i % 2 == 0 ? 62.f : 562.f);
        const float y = last ? 548.f : (233.f + static_cast<float>(i / 2) * 151.f);
        const float w = last ? 976.f : 476.f;
        panel(x,y,w,last ? 128.f : 128.f,(i == 3 || (i < 3 && i % 2 == 0)) ? lavender : sf::Color(246,247,252),18);
        sf::CircleShape number(24.f); number.setFillColor(i == 4 ? mint : purple); number.setPosition({x+20.f,y+20.f}); window_.draw(number);
        centeredAt(x+44.f,y+44.f,14,rules[i].first,white);
        label(x+82.f,y+20.f,20,titles[i],ink);
        wrapped(x+82.f,y+53.f,w-104.f,16,rules[i].second,ink);
    }
    label(34,746,13,"난이도와 세 가지 게임 설정은 홈에서 바꿀 수 있어요.",muted);
    button("home",894,734,174,38,"홈으로");
}

void SfmlApplication::drawPause() {
    const auto& state = game_.state();
    sf::RectangleShape veil({1100.f,695.f});
    veil.setPosition({0.f,85.f});
    veil.setFillColor(sf::Color(28,25,58,202));
    window_.draw(veil);
    buttons_.clear();
    shadowPanel(300,145,500,492,white,28);
    panel(300,145,500,9,gold,4);
    sf::CircleShape icon(42.f,48); icon.setFillColor(lavender); icon.setPosition({508.f,181.f}); window_.draw(icon);
    sf::RectangleShape leftBar({9.f,31.f}); leftBar.setPosition({533.5f,207.5f}); leftBar.setFillColor(purple); window_.draw(leftBar);
    sf::RectangleShape rightBar({9.f,31.f}); rightBar.setPosition({557.5f,207.5f}); rightBar.setFillColor(purple); window_.draw(rightBar);
    centeredLabel(550,274,29,"게임 일시정지",ink);
    centeredLabel(550,317,14,"진행 중인 단어와 점수는 그대로 보관됩니다",muted);
    panel(340,350,420,78,sf::Color(248,247,253),16);
    label(363,364,12,"현재 단어",muted);
    label(363,389,18,state.lastWord,purpleDark);
    label(592,364,12,"현재 점수",muted);
    label(592,389,18,to_string(state.score) + "점",ink);
    button("pause_resume",340,462,420,60,"계속 플레이  →",true,true);
    button("pause_surrender",340,536,420,54,"게임 포기");
    centeredLabel(550,606,12,"ESC 키로도 다시 시작할 수 있어요",muted);
}

void SfmlApplication::drawDialog() {
    sf::RectangleShape veil({1100,780});
    veil.setFillColor(sf::Color(28,25,58,180));
    window_.draw(veil);
    shadowPanel(278,230,544,320,white,26);
    const bool close=dialog_==Dialog::Close;
    const bool reset=dialog_==Dialog::ResetRecords;
    sf::CircleShape icon(25.f); icon.setFillColor(coralSoft); icon.setPosition({315.f,267.f}); window_.draw(icon);
    centeredAt(340,292,24,"!",coral);
    label(380,269,24,reset?"기록을 초기화할까요?":close?"게임을 중단하고 닫을까요?":"게임을 포기할까요?",ink);
    wrapped(318,340,466,16,reset?
        "기존 파일을 백업한 뒤 기록을 비웁니다. 백업에 실패하면 원본을 유지합니다.":
        close?"진행 중인 판은 중단으로 저장합니다. 랭킹에는 포함하지 않습니다.":
        "이번 판은 패배로 기록합니다. 취소하면 계속 플레이할 수 있습니다.",muted);
    buttons_.clear();
    button("dialog_cancel",318,463,220,56,"계속하기");
    button("dialog_confirm",552,463,232,56,reset?"백업 후 초기화":close?"중단하고 닫기":"포기하기",true,true);
}

void SfmlApplication::draw() {
    window_.clear(background);
    drawBackground();
    buttons_.clear();
    drawHeader();
    if (screen_==Screen::Home) drawHome();
    else if (screen_==Screen::Game) drawGame();
    else if (screen_==Screen::Profiles) drawProfiles();
    else if (screen_==Screen::Records) drawRecords();
    else if (screen_==Screen::Rules) drawRules();
    else {
        shadowPanel(32,122,1036,594,white,24);
        label(64,164,13,"STARTUP ERROR",coral);
        label(64,198,30,"끗마릳기를 준비하지 못했어요",ink);
        wrapped(64,270,960,18,fatalError_,muted);
        button("exit",64,610,230,58,"프로그램 닫기",true,true);
    }
    if (paused_) drawPause();
    if (dialog_!=Dialog::None) drawDialog();
}

void SfmlApplication::capture(const string& filename) {
    // display 이후에는 이전 버퍼가 남으므로 현재 화면을 다시 그린 뒤 저장한다.
    draw();
    sf::Texture texture;
    if (!texture.resize(window_.getSize())) throw runtime_error("Cannot create screenshot.");
    texture.update(window_);
    if (!texture.copyToImage().saveToFile(captureDirectory_/filename))
        throw runtime_error("Cannot save screenshot.");
    window_.display();
}

void SfmlApplication::advanceUiTest() {
    // 별도 7단어 사전/빈 기록/새 프로필 파일로만 실행한다. 실제 사용자 자료에서 테스트하지 않는다.
    auto check=[](bool condition,const char* message) {
        if (!condition) throw runtime_error(message);
    };
    if (frame_>1800) throw runtime_error("GUI test timed out.");
    switch (testStep_) {
    case 0:
        if (frame_<3) return;
        check(dictionary_.size()==7 && dictionary_.contains("사과") && dictionary_.contains("과자") &&
            dictionary_.contains("자동차") && dictionary_.contains("차표") &&
            dictionary_.contains("바다") && dictionary_.contains("다리") && dictionary_.contains("리본") &&
            records_.records().empty() && records_.canSave(),"GUI tests require a fresh seven-word fixture.");
        check(profiles_.canSave() && profiles_.profiles().size()==1 &&
            profiles_.hasActiveProfile() && profiles_.activeProfile()=="플레이어",
            "GUI tests require a fresh default profile fixture.");
        settings_.allowOneShot = true;
        capture("01_home.png");
        click("profiles");
        check(screen_==Screen::Profiles && !inputVisible_ &&
            profileSummaries().size()==1 && profileSummaries().front().active,
            "Profile screen did not open with the default profile selected.");
        capture("01_profile_default.png");
        click("new_profile");
        check(screen_==Screen::Home && playerName_.empty() && inputVisible_ && inputEnabled_,
            "New-profile action did not return to an editable home screen.");
        input_.setTextUtf8("테스터");
        capture("01_new_profile.png");
        click("start");
        check(screen_==Screen::Game && activeGame() && game_.state().playerName=="테스터" &&
            profiles_.profiles().size()==2 && profiles_.activeProfile()=="테스터",
            "Starting with a new name did not create and select its profile.");
        ++testStep_; break;
    case 1:
        capture("02_game.png");
        input_.setTextUtf8("abc"); click("submit");
        check(game_.state().remainingAttempts()==2,"Invalid input did not consume one chance.");
        ++testStep_; break;
    case 2:
        capture("03_validation.png");
        input_.setTextUtf8("과자"); click("submit");
        check(game_.state().phase==GamePhase::ComputerTurn && game_.state().score==190 &&
            game_.state().lastWord=="과자" && game_.state().usedWords.size()==2 && moves_.size()==2,
            "Korean submission failed.");
        click("pause");
        check(paused_ && dialog_==Dialog::None && !inputVisible_ && !inputEnabled_,
            "Computer-turn pause did not hide and disable input.");
        capture("03_pause.png");
        ++testStep_; break;
    case 3:
        // The AI normally moves after 0.4 seconds. Waiting longer proves that pause freezes it.
        if (computerClock_.getElapsedTime().asSeconds()<0.65f) return;
        check(paused_ && dialog_==Dialog::None && game_.state().phase==GamePhase::ComputerTurn &&
            game_.state().result==GameResult::None && game_.state().lastWord=="과자" &&
            game_.state().score==190 && game_.state().remainingAttempts()==2 &&
            game_.state().humanMoves==1 && game_.state().computerDefeats==0 &&
            game_.state().usedWords.size()==2 && moves_.size()==2 &&
            records_.records().empty() && !inputVisible_ && !inputEnabled_,
            "Pause allowed AI, input, game state or records to change.");
        input_.setTextUtf8("차표");
        click("submit");
        check(game_.state().phase==GamePhase::ComputerTurn && game_.state().lastWord=="과자" &&
            game_.state().score==190 && game_.state().usedWords.size()==2 && moves_.size()==2,
            "A hidden input was submitted while paused.");
        input_.clear();
        requestClose();
        check(dialog_==Dialog::Close && paused_,"Close confirmation did not preserve pause state.");
        capture("03_pause_close_confirmation.png");
        click("dialog_cancel");
        check(dialog_==Dialog::None && paused_ && activeGame() && !inputVisible_ && !inputEnabled_ &&
            records_.records().empty(),
            "Cancelling close resumed or changed the paused game.");
        capture("03_pause_restored.png");
        click("pause_resume");
        check(!paused_ && dialog_==Dialog::None && game_.state().phase==GamePhase::ComputerTurn &&
            inputVisible_ && !inputEnabled_ && computerClock_.getElapsedTime().asSeconds()<0.15f,
            "Resume did not restore the pending computer turn cleanly.");
        ++testStep_; break;
    case 4:
        if (game_.state().phase!=GamePhase::HumanTurn) return;
        check(game_.state().lastWord=="자동차" && game_.state().remainingAttempts()==3,"AI/reset failed.");
        capture("02_word_chain.png");
        input_.setTextUtf8("차표"); click("submit"); ++testStep_; break;
    case 5:
        if (game_.state().phase!=GamePhase::HumanTurn) return;
        check(game_.state().result==GameResult::None && game_.state().score==1430 &&
            game_.state().computerDefeats==1 && game_.state().usedWords.size()==5,
            "Computer defeat did not award 1000 points and continue with a new round.");
        input_.setTextUtf8("차표"); click("submit");
        check(game_.state().remainingAttempts()==2 && game_.state().score==1430,
            "Previous-round word was accepted again.");
        capture("04_round_continues.png"); click("surrender"); click("dialog_confirm");
        check(game_.state().phase==GamePhase::Finished && game_.state().saveStatus==SaveStatus::Ok,
            "Continued game was not saved when it ended.");
        click("profiles");
        {
            const auto beforeSelection = profileSummaries();
            const auto defaultProfile = find_if(beforeSelection.begin(),beforeSelection.end(),
                [](const ProfileSummary& item) { return item.name=="플레이어"; });
            check(screen_==Screen::Profiles && beforeSelection.size()==2 &&
                defaultProfile!=beforeSelection.end(),"Created profile was not shown in the profile screen.");
            click("select_profile_" + to_string(distance(beforeSelection.begin(),defaultProfile)));
            check(playerName_=="플레이어" && profiles_.activeProfile()=="플레이어",
                "Existing profile selection failed.");

            const auto afterSelection = profileSummaries();
            const auto testerProfile = find_if(afterSelection.begin(),afterSelection.end(),
                [](const ProfileSummary& item) { return item.name=="테스터"; });
            check(testerProfile!=afterSelection.end(),"New profile disappeared after selecting another profile.");
            click("select_profile_" + to_string(distance(afterSelection.begin(),testerProfile)));
            const auto selected = profileSummaries();
            const auto activeTester = find_if(selected.begin(),selected.end(),
                [](const ProfileSummary& item) { return item.name=="테스터"; });
            check(playerName_=="테스터" && profiles_.activeProfile()=="테스터" &&
                activeTester!=selected.end() && activeTester->active && activeTester->games==1 &&
                activeTester->wins==0 && activeTester->losses==1 &&
                activeTester->bestScore==1430 && activeTester->totalScore==1430,
                "Profile reselection or per-profile record summary failed.");
        }
        capture("04_profile_selected.png");
        click("records"); ++testStep_; break;
    case 6:
        capture("05_records.png");
        click("rank_hard");
        check(rankingDifficulty_ == Difficulty::Hard && records_.rankings(rankingDifficulty_).empty(),
            "Difficulty ranking tab did not filter records.");
        click("rank_easy");
        check(rankingDifficulty_ == Difficulty::Easy && records_.rankings(rankingDifficulty_).size() == 1,
            "Difficulty ranking tab did not restore easy records.");
        click("rules"); ++testStep_; break;
    case 7:
        capture("06_rules.png"); click("home"); click("start");
        for (int i=0;i<3;++i) { input_.setTextUtf8("abc"); click("submit"); }
        check(game_.state().result==GameResult::Lose && records_.records().size()==2,"Three-error loss failed.");
        ++testStep_; break;
    case 8:
        if (resultClock_.getElapsedTime().asSeconds() < 0.45f) return;
        capture("07_loss.png"); click("restart"); requestClose(); ++testStep_; break;
    case 9:
        capture("08_confirmation.png"); click("dialog_cancel");
        check(activeGame() && records_.records().size()==2,"Cancel changed game/records.");
        requestClose(); confirmDialog();
        check(records_.rankings().size()==2 && records_.rankings().front().score==1430 &&
            records_.rankings().front().name=="테스터" && profiles_.profiles().size()==2 &&
            profiles_.activeProfile()=="테스터","Ranking, profile or abort policy failed.");
        {
            ofstream result(captureDirectory_/"ui_test_results.txt");
            result << "PASS SFML screens, profile entry/create/select, computer-turn pause freeze, hidden-input blocking, close-cancel pause restoration, resume, native UTF-8 input, validation, AI turn, chance reset, +1000 computer-defeat continuation, cross-round replay rejection, loss, restart, records, close confirmation and abort.\n";
            result << "Profiles: tester active; tester and player stored; tester 1 completed game at profile capture.\n";
            result << "Ranking: tester 1430, tester 0. Aborted entry excluded.\n";
        }
        ++testStep_; break;
    }
}
