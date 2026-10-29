#include "Application.h"
#include "GameManager.h"
#include "ProfileManager.h"
#include "Utf8Util.h"
#include <algorithm>
#include <chrono>
#include <fstream>
#include <functional>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace std;
namespace fs = filesystem;

namespace {
fs::path root;
int fixtures = 0;
void require(bool value, const string& message) { if (!value) throw runtime_error(message); }
void writeFile(const fs::path& path, const string& text) {
    ofstream file(path, ios::binary); file << text; file.close();
    require(!file.fail(), "fixture write failed");
}
string readFile(const fs::path& path) {
    ifstream file(path, ios::binary); return {istreambuf_iterator<char>(file), {}};
}
const string chain = "사과\n과자\n자동차\n차표\n바다\n다리\n리본\n";
struct Fixture {
    fs::path dir;
    Dictionary dictionary;
    RuleChecker rules;
    ComputerAI computer {123};
    RecordManager records;
    Fixture(const string& words = chain, const string& korean = "") {
        dir = root / to_string(++fixtures); fs::create_directories(dir);
        writeFile(dir / "words.txt", words);
        require(dictionary.load(dir / "words.txt").status == DictionaryStatus::Ok, "dictionary load");
        if (!korean.empty()) {
            writeFile(dir / "korean_words.txt", korean);
            dictionary.loadKoreanWords(dir / "korean_words.txt");
        }
        require(records.loadRecords(dir / "rankings.txt") == LoadStatus::NotFound, "fresh ranking");
    }
};
Settings permissive() { Settings settings; settings.allowOneShot = true; return settings; }
}

int main(int argc, char* argv[]) {
    root = fs::temp_directory_path() / ("wordchain_features_" + to_string(chrono::steady_clock::now().time_since_epoch().count()));
    fs::create_directories(root);
    int passed = 0, failed = 0;
    auto test = [&](const char* name, const function<void()>& run) {
        try { run(); ++passed; cout << "PASS " << name << '\n'; }
        catch (const exception& error) { ++failed; cout << "FAIL " << name << ": " << error.what() << '\n'; }
    };
    test("UTF-8 valid, malformed, overlong, surrogate and syllable limits", [] {
        vector<char32_t> cp;
        require(Utf8Util::decode("기러기", cp) && cp.size() == 3, "Hangul decode");
        for (const string bad : {string("\xc0\xaf"),string("\xed\xa0\x80"),string("\xf4\x90\x80\x80"),string("\xea\xb0")})
            require(!Utf8Util::decode(bad, cp), "invalid UTF-8 accepted");
        char32_t first, last; size_t count;
        require(!Utf8Util::wordParts("가", first, last, count), "one syllable");
        require(!Utf8Util::wordParts("ㄱㄴ", first, last, count), "jamo accepted");
    });
    test("dictionary BOM, duplicates, invalid lines, korean allowlist and reload", [] {
        Fixture f("\xef\xbb\xbf사과\r\n사과\n과자\nabc\n가\n", "사과\n과자\n자동차\n");
        require(f.dictionary.size() == 2 && f.dictionary.koreanSize() == 2, "counts");
        require(f.dictionary.isKoreanWord("사과") && !f.dictionary.isKoreanWord("자동차"), "intersection");
        f.dictionary.loadKoreanWords(f.dir / "missing.txt");
        require(f.dictionary.koreanSize() == 0, "stale korean data");
        f.dictionary.load(f.dir / "missing.txt");
        require(f.dictionary.size() == 0, "stale dictionary data");
    });
    test("dueum always on; final consonants retained; no reverse conversion", [] {
        RuleChecker rules;
        const vector<pair<char32_t,char32_t>> pairs {{U'녀',U'여'},{U'뇨',U'요'},{U'뉴',U'유'},{U'니',U'이'},
            {U'라',U'나'},{U'래',U'내'},{U'로',U'노'},{U'뢰',U'뇌'},{U'루',U'누'},{U'르',U'느'},
            {U'랴',U'야'},{U'려',U'여'},{U'례',U'예'},{U'료',U'요'},{U'류',U'유'},{U'리',U'이'}};
        for (const auto& pair : pairs) for (int tail = 0; tail < 28; ++tail) {
            const auto result = rules.allowedStarts(pair.first + tail);
            require(result == vector<char32_t>{pair.first + tail, pair.second + tail}, "dueum/tail mapping");
        }
        require(rules.allowedStarts(U'역') == vector<char32_t>{U'역'}, "reverse conversion");
        require(rules.allowedStarts(U'나') == vector<char32_t>{U'나'}, "unrelated n vowel");
    });
    test("shared rule validation errors and dueum connection", [] {
        Fixture f("매력\n역사\n사과\n과자\n자동차\n차표\n");
        auto validate = [&](const string& word) { return f.rules.validate(word,"매력",f.dictionary,{"매력"},permissive()).code; };
        require(validate("역사") == ValidationCode::Valid, "dueum rejected");
        require(validate(" ") == ValidationCode::Empty, "empty");
        require(validate("abc") == ValidationCode::InvalidCharacters, "characters");
        require(validate("가") == ValidationCode::TooShort, "short");
        require(validate("사과") == ValidationCode::WrongStart, "wrong start");
        require(validate("매력") == ValidationCode::AlreadyUsed, "used");
        require(validate("역할") == ValidationCode::NotInDictionary, "unknown");
    });
    test("one-shot counts dueum replies, current used set and self exclusion", [] {
        Fixture f("력매\n매력\n역사\n사과\n과자\n자동차\n기러기\n도자기\n");
        Settings settings;
        require(f.rules.validate("매력","력매",f.dictionary,{"력매"},settings).valid(), "dueum reply missed");
        require(f.rules.validate("매력","력매",f.dictionary,{"력매","역사"},settings).code == ValidationCode::OneShotNotAllowed, "used reply counted");
        require(f.rules.validate("기러기","도자기",f.dictionary,{"도자기"},settings).code == ValidationCode::OneShotNotAllowed, "self counted");
        require(f.rules.validate("기러기","도자기",f.dictionary,{"도자기"},permissive()).valid(), "one-shot option");
    });
    test("korean and three-syllable rules change both validation and one-shot", [] {
        Fixture f("가도\n도서관\n관광객\n객실\n카페\n페인트\n도카\n", "가도\n도서관\n객실\n도카\n");
        Settings settings = permissive(); settings.koreanOnly = true;
        require(f.rules.validate("카페","도카",f.dictionary,{"도카"},settings).code == ValidationCode::NotKoreanWord, "foreign permitted");
        settings.allowOneShot = false;
        require(f.rules.validate("도서관","가도",f.dictionary,{"가도"},settings).code == ValidationCode::OneShotNotAllowed, "non-korean reply counted");
        settings.koreanOnly = false;
        require(f.rules.validate("도서관","가도",f.dictionary,{"가도"},settings).valid(), "allowed reply missing");
        settings.threeSyllables = true;
        require(f.rules.validate("관광객","도서관",f.dictionary,{"도서관"},settings).code == ValidationCode::OneShotNotAllowed, "two-syllable reply counted");
        require(f.rules.validate("객실","관광객",f.dictionary,{},settings).code == ValidationCode::MustBeThreeSyllables, "three mode");
    });
    test("score formula for every chance; syllables rather than UTF-8 bytes", [] {
        require(ScorePolicy::calculate(3,3).total() == 260, "first try");
        require(ScorePolicy::calculate(3,2).total() == 210, "second try");
        require(ScorePolicy::calculate(3,1).total() == 160, "third try");
        require(ScorePolicy::calculate(64,3).total() == 1480, "long word");
        require(ScorePolicy::calculate(3,0).total() == 0, "invalid chances");
    });
    test("actual game score on first, second and third attempts", [] {
        for (int errors=0; errors<3; ++errors) {
            Fixture f;
            GameManager game(f.dictionary,f.rules,f.computer,f.records);
            require(game.startNewGame(permissive(),"홍 길동","사과"), "start");
            require(game.submitWord(" ").code == ValidationCode::Empty && game.state().remainingAttempts() == 3, "blank cost");
            for (int i=0;i<errors;++i) game.submitWord("abc");
            require(game.submitWord("과자").valid(), "submit");
            require(game.state().score == 240-errors*50, "attempt score");
            game.playComputerTurn();
            require(game.state().lastWord == "자동차" && game.state().remainingAttempts() == 3, "chance reset");
            require(game.submitWord("차표").valid(), "finishing word");
            game.playComputerTurn();
            require(game.state().phase == GamePhase::HumanTurn && game.state().result == GameResult::None &&
                game.state().score == 1480-errors*50 && game.state().computerDefeats == 1,
                "computer defeat bonus/continuation");
            require(game.state().usedWords.size() == 5 && game.state().remainingAttempts() == 3,
                "new round keeps used words and resets chances");
            for (const auto& used : {"사과", "과자", "자동차", "차표"})
                require(game.state().usedWords.count(used) != 0, "previous round word forgotten");
            game.surrender(); game.abort(); game.playComputerTurn();
            require(f.records.rankings().size() == 1, "saved twice");
        }
    });
    test("computer KO continues without replaying iridium across rounds", [] {
        Fixture f("종이\n이리듐\n아이\n이불\n불꽃\n");
        GameManager game(f.dictionary, f.rules, f.computer, f.records);
        require(game.startNewGame(permissive(), "사용자", "종이"), "start");
        require(game.submitWord("이리듐").valid(), "first iridium");
        require(game.playComputerTurn(), "computer KO");
        require(game.state().phase == GamePhase::HumanTurn && game.state().score == 1260 &&
            game.state().computerDefeats == 1, "KO should continue with bonus");
        require(game.state().lastWord == "아이" && game.state().usedWords.size() == 3,
            "new opening must avoid past words");
        require(game.submitWord("이리듐").code == ValidationCode::AlreadyUsed,
            "iridium replay accepted");
        require(game.state().score == 1260 && game.state().remainingAttempts() == 2,
            "replay changed score or chances incorrectly");
        require(game.submitWord("이불").valid(), "fresh word rejected");
        game.surrender();
        require(game.startNewGame(permissive(), "사용자", "종이") &&
            game.submitWord("이리듐").valid(), "new game should reset word history");
    });
    test("game ends in victory when no unused opening remains", [] {
        Fixture f("종이\n이리듐\n");
        GameManager game(f.dictionary, f.rules, f.computer, f.records);
        require(game.startNewGame(permissive(), "사용자", "종이"), "start");
        require(game.submitWord("이리듐").valid(), "one-shot word");
        require(game.playComputerTurn() && game.state().phase == GamePhase::Finished &&
            game.state().result == GameResult::Win && game.state().score == 1260,
            "exhausted dictionary should not replay a word");
    });
    test("three mistakes lose without word or score changes; restart and abort", [] {
        Fixture f;
        GameManager game(f.dictionary,f.rules,f.computer,f.records);
        require(!game.startNewGame(permissive(),"","사과"), "empty name");
        require(game.startNewGame(permissive(),"사용자","사과"), "start");
        require(!game.startNewGame(permissive(),"다른사람","사과"), "active restart");
        for (int i=0;i<3;++i) game.submitWord("abc");
        require(game.state().result == GameResult::Lose && game.state().score == 0 && game.state().usedWords.size()==1, "three errors");
        require(game.startNewGame(permissive(),"새이름","사과") && game.state().remainingAttempts()==3, "restart");
        game.abort();
        require(f.records.records().size()==2 && f.records.rankings().size()==1, "abort ranking");
    });
    test("opening and AI obey all eight setting combinations", [] {
        const string words="도서관\n관광객\n객관적\n적대감\n감나무\n무용수\n수영장\n장독대\n대장간\n간장독\n독서실\n실내화\n화장실\n실습생\n생명력\n역무원\n원두막\n막걸리\n리어카\n카메라\n라디오\n사과\n과자\n자동차\n차표\n";
        Fixture f(words,"도서관\n관광객\n객관적\n적대감\n감나무\n무용수\n수영장\n장독대\n대장간\n간장독\n독서실\n실내화\n화장실\n실습생\n생명력\n역무원\n원두막\n막걸리\n");
        for (int mask=0;mask<8;++mask) {
            Settings settings; settings.allowOneShot=(mask&1)!=0; settings.koreanOnly=(mask&2)!=0; settings.threeSyllables=(mask&4)!=0;
            const auto openings=f.computer.getOpeningCandidates(f.dictionary,f.rules,settings);
            require(!openings.empty(), "no opening for option");
            for (const auto& opening : openings) {
                require(f.rules.wordAllowed(opening,f.dictionary,settings), "bad opening");
                const set<string> used{opening};
                const auto candidates=f.computer.getLegalCandidates(opening,f.dictionary,f.rules,used,settings);
                require(!candidates.empty(), "unplayable opening");
                set<string> scanned;
                for (const auto& word:f.dictionary.allWords()) if (f.rules.validate(word,opening,f.dictionary,used,settings).valid()) scanned.insert(word);
                require(set<string>(candidates.begin(),candidates.end())==scanned,"AI/rule disagreement");
                settings.difficulty=Difficulty::Hard;
                const auto chosen=f.computer.chooseWord(opening,f.dictionary,f.rules,used,settings);
                require(chosen.has_value(), "hard empty");
                require(f.rules.validate(*chosen,opening,f.dictionary,used,settings).valid(),"hard illegal");
            }
        }
    });
    test("easy and normal prefer familiar words; hard keeps strategy", [] {
        Fixture f("사과\n과자\n자동차\n자두\n두부\n과학\n학문\n", "사과\n과자\n자동차\n자두\n두부\n");
        Settings settings=permissive();
        const set<string> used{"사과"};
        settings.difficulty=Difficulty::Easy;
        require(f.computer.chooseWord("사과",f.dictionary,f.rules,used,settings)==optional<string>("과자"),
            "easy did not prefer familiar answer");
        settings.difficulty=Difficulty::Normal;
        require(f.computer.chooseWord("사과",f.dictionary,f.rules,used,settings)==optional<string>("과자"),
            "normal did not prefer familiar answer");
        settings.difficulty=Difficulty::Hard;
        require(f.computer.chooseWord("사과",f.dictionary,f.rules,used,settings)==optional<string>("과학"),
            "hard did not minimize replies");
    });
    test("ranking order, joint rank, stable ties, abort exclusion and UTF-8 roundtrip", [] {
        Fixture f;
        require(f.records.saveResult({GameResult::Lose,120,{},"민수"})==SaveStatus::Ok,"save1");
        Settings hard; hard.difficulty=Difficulty::Hard; hard.threeSyllables=true;
        require(f.records.saveResult({GameResult::Win,260,hard,"지수"})==SaveStatus::Ok,"save2");
        require(f.records.saveResult({GameResult::Win,260,{},"Team 1"})==SaveStatus::Ok,"save3");
        require(f.records.saveResult({GameResult::Aborted,9999,{},"중단"})==SaveStatus::Ok,"save4");
        RecordManager loaded; require(loaded.loadRecords(f.dir/"rankings.txt")==LoadStatus::Ok,"reload");
        const auto ranking=loaded.rankings();
        require(ranking.size()==3 && ranking[0].name=="지수" && ranking[1].name=="Team 1","ordering");
        require(ranking[0].rank==1 && ranking[1].rank==1 && ranking[2].rank==3,"joint ranks");
        require(ranking[0].difficulty==Difficulty::Hard && loaded.records()[1].settings.threeSyllables,"settings persisted");
        const auto easyRanking=loaded.rankings(Difficulty::Easy);
        const auto hardRanking=loaded.rankings(Difficulty::Hard);
        require(easyRanking.size()==2 && easyRanking[0].name=="Team 1" && easyRanking[0].rank==1 &&
            easyRanking[1].name=="민수" && easyRanking[1].rank==2,"easy ranking filter");
        require(hardRanking.size()==1 && hardRanking[0].name=="지수" && hardRanking[0].rank==1,
            "hard ranking filter");
    });
    test("corrupt/legacy records blocked; backup reset and malformed name rejection", [] {
        Fixture f;
        for (const string text:{string("WORDCHAIN_RECORD_V1\nWIN 10 EASY 0 0\n"),string("broken"),
             string("WORDCHAIN_RANKING_V2\n\"name\" -1 EASY WIN 0 0 0\n"),
             string("WORDCHAIN_RANKING_V2\n\"name\" 10 EASY WIN 2 0 0\n")}) {
            writeFile(f.dir/"rankings.txt",text);
            require(f.records.loadRecords(f.dir/"rankings.txt")==LoadStatus::InvalidFormat,"corrupt accepted");
            require(f.records.saveResult({GameResult::Win,260,{},"정상"})==SaveStatus::Blocked,"corrupt overwrite");
            require(readFile(f.dir/"rankings.txt")==text,"bytes changed");
            require(f.records.resetRecords()==SaveStatus::Ok,"reset");
            require(readFile(f.records.lastBackupPath())==text,"backup mismatch");
        }
        require(!RecordManager::validName("\n") && !RecordManager::validName("\"bad\"") && !RecordManager::validName("1234567890123"),"bad name");
        require(f.records.saveResult({GameResult::Win,1,{},""})==SaveStatus::InvalidRecord,"empty record name");
    });
    test("profiles create, select MRU, reload, validate names and preserve corruption", [] {
        const fs::path dir = root / to_string(++fixtures);
        fs::create_directories(dir);
        const fs::path path = dir / "profiles.txt";

        ProfileManager profiles;
        require(profiles.load(path) == ProfileManager::LoadStatus::NotFound,
            "fresh profiles");
        require(profiles.canSave() && !profiles.hasActiveProfile() &&
            profiles.profiles().empty(), "fresh profile state");
        require(profiles.selectOrCreate("bad!") == ProfileManager::SelectStatus::InvalidName &&
            !fs::exists(path), "invalid profile name");

        require(profiles.selectOrCreate("민수") == ProfileManager::SelectStatus::Created,
            "create first profile");
        require(profiles.selectOrCreate("Team 1") == ProfileManager::SelectStatus::Created,
            "create second profile");
        require(profiles.activeProfile() == "Team 1" &&
            profiles.profiles() == vector<string>({"Team 1", "민수"}),
            "new profile MRU order");
        require(profiles.selectOrCreate("민수") == ProfileManager::SelectStatus::Selected,
            "reselect profile");
        require(profiles.activeProfile() == "민수" &&
            profiles.profiles() == vector<string>({"민수", "Team 1"}),
            "reselected profile MRU order");

        ProfileManager reloaded;
        require(reloaded.load(path) == ProfileManager::LoadStatus::Ok,
            "reload profiles");
        require(reloaded.activeProfile() == "민수" &&
            reloaded.profiles() == vector<string>({"민수", "Team 1"}),
            "active profile persistence");

        const fs::path importedPath = dir / "imported_profiles.txt";
        ProfileManager imported;
        require(imported.load(importedPath) == ProfileManager::LoadStatus::NotFound &&
            imported.initialize({"최근 사용자", "이전 사용자"}) &&
            imported.profiles() == vector<string>({"최근 사용자", "이전 사용자"}),
            "bulk profile migration");
        ProfileManager importedAgain;
        require(importedAgain.load(importedPath) == ProfileManager::LoadStatus::Ok &&
            importedAgain.activeProfile() == "최근 사용자" &&
            importedAgain.profiles() == vector<string>({"최근 사용자", "이전 사용자"}) &&
            !importedAgain.initialize({"덮어쓰기"}),
            "bulk profile migration persistence");

        const string corrupt = "WORDCHAIN_PROFILES_V1\nACTIVE \"민수\"\nPROFILE bad!\n";
        writeFile(path, corrupt);
        ProfileManager blocked;
        require(blocked.load(path) == ProfileManager::LoadStatus::InvalidFormat &&
            !blocked.canSave(), "corrupt profiles accepted");
        require(blocked.selectOrCreate("지수") == ProfileManager::SelectStatus::Blocked,
            "corrupt profiles overwrite allowed");
        require(readFile(path) == corrupt, "corrupt profile bytes changed");
    });
    test("write failure preserves final game result", [] {
        Fixture f; f.records.loadRecords(f.dir/"absent"/"rankings.txt");
        GameManager game(f.dictionary,f.rules,f.computer,f.records);
        require(game.startNewGame(permissive(),"사용자","사과"),"start");
        game.surrender();
        require(game.state().result==GameResult::Lose && game.state().saveStatus==SaveStatus::WriteError,"save failure");
        require(f.records.records().empty(),"memory changed on failed write");
    });
    test("console options/name/points/ranking end-to-end without hints", [] {
        Fixture f("사과\n과자\n과일\n바다\n다리\n리본\n",
                  "사과\n과자\n과일\n바다\n다리\n리본\n");
        istringstream input("2\n1\n1\n0\n0\n1\n테스터\n과자\nquit\n3\n0\n");
        ostringstream output;
        require(runApplication(f.dir,input,output,123)==0,"console exit");
        require(output.str().find("+240점")!=string::npos && output.str().find("컴퓨터 패배! +1,000점")!=string::npos &&
            output.str().find("1위 | 테스터 | 1240")!=string::npos,"console score/continue/ranking");
        require(output.str().find("힌트") == string::npos,"hint remains");
    });
    test("shipping dictionary all 8 rule combinations and three AI levels", [&] {
        if (argc<2) { cout << "SKIP shipping data: pass data directory\n"; return; }
        Dictionary dictionary; const fs::path data=fs::u8path(argv[1]);
        require(dictionary.load(data/"words.txt").status==DictionaryStatus::Ok,"shipping words");
        require(dictionary.loadKoreanWords(data/"korean_words.txt").status==DictionaryStatus::Ok,"shipping korean words");
        int games=0, moves=0, longRallies=0;
        long long maximumStartMilliseconds=0;
        long long maximumCachedStartMilliseconds=0;
        for (int mask=0;mask<8;++mask) {
            const int difficulty=mask%3;
            RuleChecker rules; ComputerAI computer(100+mask+difficulty);
            RecordManager records; records.loadRecords(root/("shipping"+to_string(games)+".txt"));
            Settings settings; settings.allowOneShot=(mask&1)!=0; settings.koreanOnly=(mask&2)!=0;
            settings.threeSyllables=(mask&4)!=0; settings.difficulty=difficulty==0?Difficulty::Easy:
                difficulty==1?Difficulty::Normal:Difficulty::Hard;
            GameManager game(dictionary,rules,computer,records);
            const auto startTime=chrono::steady_clock::now();
            require(game.startNewGame(settings,"검증"),"shipping opening");
            maximumStartMilliseconds=max(maximumStartMilliseconds,
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now()-startTime).count());
            const auto cachedStartTime=chrono::steady_clock::now();
            require(computer.chooseOpening(dictionary,rules,settings).has_value(),"cached shipping opening");
            maximumCachedStartMilliseconds=max(maximumCachedStartMilliseconds,
                chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now()-cachedStartTime).count());
            for (int turn=0;turn<10 && game.state().phase!=GamePhase::Finished;++turn) {
                if (game.state().phase==GamePhase::ComputerTurn) { game.playComputerTurn(); continue; }
                const auto candidates=computer.getLegalCandidates(game.state().lastWord,dictionary,rules,game.state().usedWords,settings);
                require(!candidates.empty(),"human already stuck");
                require(game.submitWord(candidates.front()).valid(),"candidate validation disagreement");
                ++moves;
            }
            // 정답만 내는 자동 플레이는 수천 턴도 이어질 수 있다. 제품의 턴 제한이 아니다.
            // 긴 랠리는 실제 사용자처럼 세 번 틀려 종료해 저장까지 확인한다.
            if (game.state().phase==GamePhase::ComputerTurn) game.playComputerTurn();
            if (game.state().phase==GamePhase::HumanTurn) {
                ++longRallies;
                for (int error=0;error<3;++error) game.submitWord("abc");
            }
            require(game.state().phase==GamePhase::Finished,"termination failed");
            require(records.rankings().size()==1,"shipping ranking missing");
            ++games;
        }
        cout << "EVIDENCE dictionary=" << dictionary.size() << " korean=" << dictionary.koreanSize() <<
            " games=" << games << " moves=" << moves << " long_rallies_ended_by_errors=" << longRallies <<
            " max_start_ms=" << maximumStartMilliseconds <<
            " max_cached_start_ms=" << maximumCachedStartMilliseconds << '\n';
    });
    cout << "SUMMARY " << passed << " groups passed; " << failed << " failed.\n";
    cout << "Artifacts: " << root.u8string() << '\n';
    return failed ? 1 : 0;
}
