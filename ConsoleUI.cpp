#include "ConsoleUI.h"
#include "Utf8Util.h"
#include <sstream>
using namespace std;

bool ConsoleUI::readLine(const string& prompt, string& text) {
    output_ << prompt;
    return static_cast<bool>(getline(input_, text));
}
int ConsoleUI::readChoice(const string& prompt, int low, int high) {
    string line;
    while (readLine(prompt, line)) {
        istringstream parser(line);
        int choice;
        string extra;
        if ((parser >> choice) && !(parser >> extra) && choice >= low && choice <= high) return choice;
        message("올바른 메뉴 번호를 입력하세요.");
    }
    return -1;
}
void ConsoleUI::message(const string& text) { output_ << text << '\n'; }
void ConsoleUI::showRanking(const RecordManager& records) {
    message("\n[랭킹] 등수 | 이름 | 점수 | 난이도");
    const auto ranking = records.rankings();
    if (ranking.empty()) message("등록된 점수가 없습니다.");
    for (const auto& item : ranking) {
        output_ << item.rank << "위 | " << item.name << " | " << item.score << " | "
                << (item.difficulty == Difficulty::Easy ? "쉬움" :
                    item.difficulty == Difficulty::Normal ? "보통" : "어려움") << '\n';
    }
}
void ConsoleUI::showRules() {
    message("두음법칙은 항상 적용: 력→역, 년→연, 록→녹. 원래 음절도 허용합니다.");
    message("정답 점수 = 100 + 기회 보너스(3/2/1회: 100/50/0) + 한글 음절 수×20.");
    message("매 턴 기회 3회. 빈 입력은 무료, 나머지 무효 제출은 1회 차감합니다.");
    message("한방 금지: 현재 설정과 사용 단어를 고려해 이어갈 단어가 없는 단어는 무효.");
    message("우리말만: korean_words.txt 허용 목록 사용. 외래어 제외, 한자어 허용.");
    message("쿵쿵따: 사람·AI·시작 단어 모두 정확히 3음절. 일반 모드는 2~64음절.");
    message("완료된 게임은 이름·점수·난이도로 랭킹에 표시. 중단은 랭킹 제외.");
    message("컴퓨터가 단어를 잇지 못하면 1,000점을 받고 새 제시어로 같은 게임을 계속합니다.");
}
void ConsoleUI::showGameState(const GameState& state, const RuleChecker& rules) {
    char32_t first = 0, last = 0;
    size_t length = 0;
    Utf8Util::wordParts(state.lastWord, first, last, length);
    string starts;
    for (const auto ch : rules.allowedStarts(last)) {
        if (!starts.empty()) starts += "/";
        starts += Utf8Util::encode(ch);
    }
    message("현재 단어: " + state.lastWord + " | 시작 음절: " + starts +
            " | 점수: " + to_string(state.score) + " | 남은 기회: " + to_string(state.remainingAttempts()));
}
string ConsoleUI::errorText(ValidationCode code) {
    switch (code) {
    case ValidationCode::Empty: return "단어를 입력하세요. 빈 입력은 기회를 사용하지 않습니다.";
    case ValidationCode::InvalidEncoding: return "UTF-8 문자를 읽을 수 없습니다.";
    case ValidationCode::InvalidCharacters: return "완성된 한글 단어를 입력하세요.";
    case ValidationCode::TooShort: return "두 음절 이상 입력하세요.";
    case ValidationCode::TooLong: return "64음절까지 입력할 수 있습니다.";
    case ValidationCode::NotInDictionary: return "사전에 없는 단어입니다.";
    case ValidationCode::AlreadyUsed: return "이미 사용한 단어입니다.";
    case ValidationCode::WrongStart: return "이전 단어의 끝 음절 또는 두음 변환 음절로 시작하세요.";
    case ValidationCode::NotKoreanWord: return "우리말 허용 사전에 등록되지 않은 단어입니다.";
    case ValidationCode::MustBeThreeSyllables: return "쿵쿵따에서는 정확히 세 음절만 허용합니다.";
    case ValidationCode::OneShotNotAllowed: return "이어갈 단어가 없는 한방 단어는 현재 설정에서 금지됩니다.";
    default: return "지금은 제출할 수 없습니다.";
    }
}
