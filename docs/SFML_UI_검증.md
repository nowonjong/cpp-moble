> 변경 전 문서입니다. 현재 기능은 [2026-09-10 변경 사항](기능변경_2026-09-10.md)을 기준으로 확인하세요.

# SFML UI 검증 결과

2026-09-09, 현재 Windows 개발 PC 기준입니다.

## 구현 상태

기본 실행을 SFML 창으로 전환했습니다. 홈·게임·전적·규칙, 난이도·힌트 선택, 한글 입력칸, 점수·기회·단어 이력, 재시작, 포기·중단 확인과 기록 오류 안내를 연결했습니다. 게임 판정과 저장은 기존 GameManager와 RecordManager를 사용합니다.

Visual Studio 2022 v143 / C++17 / x64, SFML 3.0.2 VC2022 x64 구성입니다. Debug와 Release 모두 빌드 성공했으며 실행에 필요한 SFML DLL이 각 출력 폴더에 복사됩니다. Release에는 재배포 가능한 Visual C++ 런타임 DLL도 포함했습니다.

## 확인 결과

| 검사 | 결과 |
|---|---|
| 기존 자동 검사 | 30개 그룹 통과, 실패 0 |
| 시작 음절 인덱스와 전체 탐색 비교 | 임의 상태 2,000개 일치 |
| 실제 사전 27,832개로 게임 검증 | 200판, 사람 제출 802회, 66승·134패 |
| SFML 화면·게임 흐름 | Debug / Release 모두 통과 |
| Windows 입력칸과 GUI 연결 | 독립 검사 8개 통과 |
| 화면 확인 | 홈·게임·입력 오류·승리·전적·규칙·종료 확인·사전 오류 화면 확인 |
| 기존 사전·개인 전적 보존 | 작업 전후 SHA-256 일치 |

SFML 흐름 검사는 별도 4단어 사전으로 시작 → 잘못된 입력 → 한글 제출 → AI 응답 → 기회 초기화 → 승리 → 전적 → 규칙 → 재시작 → 3회 오류 패배 → 종료 취소 → 중단 저장을 확인했습니다. 최종 결과는 1승·1패·1중단, 최고 20점입니다.

Windows 입력칸 검사는 실제 게임 실행 파일에 마우스·문자·Enter 메시지를 보내 확인했습니다. `과자` 제출이 정확히 WIN 10 한 건으로 저장되고, 1200×900 창 크기에서도 입력칸의 위치와 크기가 맞으며, 종료 시 입력칸이 숨겨지는 것을 확인했습니다.

**검증 범위:** 자동 검사는 Win32 메시지 기반입니다. 실제 Microsoft 한글 IME에서 사람이 한/영 전환과 자음·모음을 직접 눌러 조합하는 검사는 아직 하지 않았습니다. 팀원이 첫 실행 시 `사과` 등을 입력하면서 조합 확정, Enter 제출, 붙여넣기를 확인하면 됩니다. 다른 PC의 그래픽 드라이버·배율 환경에서는 아직 실행하지 않았습니다.

## 팀원이 다시 검사하는 방법

먼저 솔루션을 원하는 구성으로 빌드한 뒤 prototype 폴더에서 PowerShell로 실행합니다.

```powershell
.\tests\RunSfmlUiTests.ps1 -Configuration Release
.\tests\RunSfmlUiTests.ps1 -Configuration Debug
```

현재 사용자 TEMP 아래에 매번 새 폴더를 만들며, 화면 PNG·검사 결과·테스트 전적을 남깁니다. 실제 `data/records.txt`는 사용하지 않습니다. 테스트 실행 옵션을 직접 사용할 때도 실제 사전·전적 경로를 지정하지 마세요.

기존 코어 검사는 `tests/word_game_tests.vcxproj`를 빌드한 뒤 아래처럼 실행합니다.

```powershell
.\bin\Tests\word_game_tests.exe .\data\words.txt
```

GUI 결과 요약은 `tests/sfml_results.txt`, 입력 연결 결과는 `tests/sfml_native_results.txt`에 보관했습니다.

## 화면 자료

- `screenshots/sfml_home.png`: 실제 사전과 기존 전적을 불러온 홈 화면.
- `screenshots/sfml_game.png`: 4단어 테스트 사전의 게임 화면.
- `screenshots/sfml_victory.png`: 테스트 사전에서 20점 승리한 화면.

SFML 자체 캡처는 Windows 자식 입력칸의 내부 글자를 포함하지 않습니다. 화면 캡처와 별개로 실제 입력칸의 문자열 전달·Enter 제출은 위 통합 검사로 확인했습니다.

## 데이터 보존 확인

- words.txt: `5DDDFFE08F32529887DDFC264D3408F5687F715D30071AF63EC065A963FF3137`
- records.txt: `1C50CB7D2E647A93A02835FBC36F5431A544719E073C46BF7D0E2BA479DD0A99`

개인 연습용 `C:\TeamProject2\word_game`은 변경하지 않았습니다.