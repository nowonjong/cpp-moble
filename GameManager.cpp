#include "GameManager.h"
#include "Utf8Util.h"

using namespace std;

GameManager::GameManager(const Dictionary& dictionary, const RuleChecker& rules,
    ComputerAI& computer, RecordManager& records)
    : dictionary_(dictionary), rules_(rules), computer_(computer), records_(records) {}

bool GameManager::startNewGame(const Settings& settings, const string& name, const string& openingForTest) {
    if (state_.phase == GamePhase::HumanTurn || state_.phase == GamePhase::ComputerTurn) return false;
    if (!RecordManager::validName(name)) return false;
    if (settings.difficulty != Difficulty::Easy && settings.difficulty != Difficulty::Normal &&
        settings.difficulty != Difficulty::Hard) return false;
    const auto opening = openingForTest.empty()
        ? computer_.chooseOpening(dictionary_, rules_, settings)
        : optional<string>(openingForTest);
    if (!opening || !rules_.wordAllowed(*opening, dictionary_, settings)) return false;
    // chooseOpening은 실제 답변이 있는 시작 단어만 반환한다. 테스트에서 직접 넣은
    // 시작 단어만 별도로 검사해 전체 사전을 한 번 더 훑는 시작 지연을 피한다.
    if (!openingForTest.empty() &&
        computer_.getLegalCandidates(*opening, dictionary_, rules_, {*opening}, settings).empty()) return false;
    state_ = {};
    state_.settings = settings;
    state_.playerName = name;
    state_.lastWord = *opening;
    state_.usedWords.insert(*opening);
    state_.phase = GamePhase::HumanTurn;
    return true;
}

ValidationResult GameManager::submitWord(const string& word) {
    if (state_.phase != GamePhase::HumanTurn)
        return {ValidationCode::NotYourTurn, word};
    const auto result = rules_.validate(word, state_.lastWord, dictionary_, state_.usedWords, state_.settings);
    if (result.code == ValidationCode::Empty) return result;
    if (!result.valid()) {
        ++state_.errorsThisTurn;
        if (state_.errorsThisTurn == 3) finishGame(GameResult::Lose, EndReason::ThreeErrors);
        return result;
    }
    state_.lastWord = result.word;
    state_.usedWords.insert(result.word);
    char32_t first = 0, last = 0;
    size_t syllables = 0;
    Utf8Util::wordParts(result.word, first, last, syllables);
    state_.lastAward = ScorePolicy::calculate(syllables, state_.remainingAttempts());
    state_.score += state_.lastAward.total();
    ++state_.humanMoves;
    state_.phase = GamePhase::ComputerTurn;
    return result;
}

bool GameManager::playComputerTurn() {
    if (state_.phase != GamePhase::ComputerTurn) return false;
    const auto answer = computer_.chooseWord(state_.lastWord, dictionary_, rules_,
        state_.usedWords, state_.settings);
    if (!answer) {
        // 컴퓨터가 막히면 라운드 승리 보너스를 주고 새 제시어로 같은 게임을 잇는다.
        // 새 라운드에서도 같은 게임의 사용 이력을 유지하고, 새 제시어도 재사용하지 않는다.
        const auto opening = computer_.chooseOpening(dictionary_, rules_, state_.settings, state_.usedWords);
        if (!opening) {
            state_.score += ScorePolicy::ComputerDefeatBonus;
            state_.lastComputerDefeatBonus = ScorePolicy::ComputerDefeatBonus;
            ++state_.computerDefeats;
            finishGame(GameResult::Win, EndReason::ComputerStuck);
            return true;
        }
        state_.score += ScorePolicy::ComputerDefeatBonus;
        state_.lastComputerDefeatBonus = ScorePolicy::ComputerDefeatBonus;
        ++state_.computerDefeats;
        state_.lastWord = *opening;
        state_.usedWords.insert(*opening);
        state_.errorsThisTurn = 0;
        state_.phase = GamePhase::HumanTurn;
        state_.result = GameResult::None;
        state_.endReason = EndReason::None;
        return true;
    }
    state_.lastComputerDefeatBonus = 0;
    state_.lastWord = *answer;
    state_.usedWords.insert(*answer);
    state_.errorsThisTurn = 0;
    state_.phase = GamePhase::HumanTurn;
    if (computer_.getLegalCandidates(*answer, dictionary_, rules_, state_.usedWords, state_.settings).empty())
        finishGame(GameResult::Lose, EndReason::HumanStuck);
    return true;
}

void GameManager::finishGame(GameResult result, EndReason reason) {
    if (state_.phase != GamePhase::HumanTurn && state_.phase != GamePhase::ComputerTurn) return;
    state_.phase = GamePhase::Finished;
    state_.result = result;
    state_.endReason = reason;
    state_.saveStatus = records_.saveResult({result, state_.score, state_.settings, state_.playerName});
}
void GameManager::surrender() { finishGame(GameResult::Lose, EndReason::Surrender); }
void GameManager::abort() { finishGame(GameResult::Aborted, EndReason::InputEnded); }
const GameState& GameManager::state() const { return state_; }

