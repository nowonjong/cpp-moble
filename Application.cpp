#include "Application.h"
#include "ConsoleUI.h"
#include "Utf8Util.h"
using namespace std;

int runApplication(const filesystem::path& dataDirectory, istream& input, ostream& output, unsigned seed) {
    ConsoleUI ui(input, output);
    Dictionary dictionary;
    if (dictionary.load(dataDirectory / "words.txt").status != DictionaryStatus::Ok) {
        ui.message("사전을 읽을 수 없거나 유효한 단어가 없습니다."); return 1;
    }
    dictionary.loadKoreanWords(dataDirectory / "korean_words.txt");
    ui.message("전체 사전 " + to_string(dictionary.size()) + "개 / 우리말 사전 " + to_string(dictionary.koreanSize()) + "개");
    RecordManager records;
    records.loadRecords(dataDirectory / "rankings.txt");
    if (!records.canSave()) ui.message("랭킹 파일을 읽을 수 없어 저장을 중지했습니다. 5번에서 백업·초기화할 수 있습니다.");
    RuleChecker rules;
    ComputerAI computer(seed);
    GameManager game(dictionary, rules, computer, records);
    Settings settings;
    string name;
    bool running = true;
    while (running) {
        ui.message("\n1. 새 게임  2. 설정  3. 랭킹  4. 규칙  5. 랭킹 복구  0. 종료");
        const int menu = ui.readChoice("선택 > ", 0, 5);
        if (menu <= 0) break;
        if (menu == 2) {
            Settings next = settings;
            const int difficulty = ui.readChoice("난이도: 1. 쉬움  2. 보통  3. 어려움 > ", 1, 3);
            if (difficulty < 0) break;
            const int oneShot = ui.readChoice("한방단어: 0. 금지  1. 허용 > ", 0, 1);
            if (oneShot < 0) break;
            const int korean = ui.readChoice("우리말만: 0. 끔  1. 켬 > ", 0, 1);
            if (korean < 0) break;
            const int three = ui.readChoice("쿵쿵따(세 글자): 0. 끔  1. 켬 > ", 0, 1);
            if (three < 0) break;
            next.difficulty = difficulty == 1 ? Difficulty::Easy :
                difficulty == 2 ? Difficulty::Normal : Difficulty::Hard;
            next.allowOneShot = oneShot == 1;
            next.koreanOnly = korean == 1;
            next.threeSyllables = three == 1;
            if (next.koreanOnly && dictionary.koreanSize() == 0) {
                ui.message("우리말 사전이 비어 있습니다. data/korean_words.txt를 확인하세요."); continue;
            }
            settings = next;
            ui.message("설정을 적용했습니다. 두음법칙은 항상 켜져 있습니다.");
        } else if (menu == 3) ui.showRanking(records);
        else if (menu == 4) ui.showRules();
        else if (menu == 5) {
            if (ui.readChoice("기존 랭킹을 백업하고 초기화할까요? 1. 확인  0. 취소 > ", 0, 1) == 1)
                ui.message(records.resetRecords() == SaveStatus::Ok ? "백업 후 초기화했습니다." : "초기화 실패. 원본을 확인하세요.");
        } else if (menu == 1) {
            if (!ui.readLine("랭킹에 사용할 이름(한글·영문·숫자·공백·_, 1~12글자) > ", name)) break;
            name = Utf8Util::trimAscii(name);
            if (!RecordManager::validName(name)) { ui.message("이름 형식이 올바르지 않습니다."); continue; }
            if (!game.startNewGame(settings, name)) {
                ui.message("현재 설정으로 이어갈 시작 단어가 없습니다. 사전 또는 설정을 확인하세요."); continue;
            }
            ui.message("컴퓨터: " + game.state().lastWord);
            while (game.state().phase != GamePhase::Finished) {
                if (game.state().phase == GamePhase::ComputerTurn) {
                    const int defeats = game.state().computerDefeats;
                    game.playComputerTurn();
                    if (game.state().computerDefeats > defeats && game.state().phase == GamePhase::HumanTurn)
                        ui.message("컴퓨터 패배! +1,000점. 새 라운드를 계속합니다.");
                    if (game.state().phase == GamePhase::HumanTurn)
                        ui.message("컴퓨터: " + game.state().lastWord);
                    continue;
                }
                ui.showGameState(game.state(), rules);
                string word;
                if (!ui.readLine("단어 (quit: 포기) > ", word)) { game.abort(); running = false; }
                else if (Utf8Util::trimAscii(word) == "quit") game.surrender();
                else {
                    const auto result = game.submitWord(word);
                    if (result.valid()) {
                        const auto& award = game.state().lastAward;
                        ui.message("정답! +" + to_string(award.total()) + "점 (기본 " + to_string(award.base) +
                                   " + 기회 " + to_string(award.attemptBonus) + " + 길이 " + to_string(award.lengthBonus) + ")");
                    } else ui.message(ConsoleUI::errorText(result.code));
                }
            }
            const auto& state = game.state();
            ui.message(string(state.result == GameResult::Win ? "승리" : state.result == GameResult::Lose ? "패배" : "중단") +
                       " | " + name + " | " + to_string(state.score) + "점");
            if (state.saveStatus != SaveStatus::Ok) ui.message("저장 실패: 랭킹 파일과 폴더를 확인하세요.");
        }
    }
    return 0;
}
