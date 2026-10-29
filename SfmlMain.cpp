// Windows 헤더를 먼저 읽어 표준 라이브러리 이름과 충돌하지 않게 한다.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <shellapi.h>
#endif
#include "SfmlApplication.h"
#include <stdexcept>
using namespace std;

namespace {
filesystem::path dataFolder(const filesystem::path& executable) {
    const filesystem::path base = executable.parent_path();
    const vector<filesystem::path> candidates {
        base / "data", base.parent_path() / "data",
        base.parent_path().parent_path() / "data", filesystem::current_path() / "data"
    };
    for (const auto& candidate : candidates) {
        if (filesystem::is_regular_file(candidate / "words.txt")) return candidate;
    }
    return base.parent_path().parent_path() / "data";
}
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // Windows 화면 배율 때문에 창 전체가 비트맵으로 확대되지 않도록 한다.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (!argv) throw runtime_error("Cannot read program arguments.");
        const filesystem::path executable = filesystem::absolute(argv[0]);
        filesystem::path data = dataFolder(executable);
        filesystem::path capture;
        bool uiTest = false;
        bool valid = true;
        bool explicitData = false;
        for (int i = 1; i < argc; ++i) {
            const wstring argument = argv[i];
            if (argument == L"--data-dir" && i + 1 < argc) { data = argv[++i]; explicitData = true; }
            else if (argument == L"--capture-dir" && i + 1 < argc) capture = argv[++i];
            else if (argument == L"--ui-test") uiTest = true;
            else valid = false;
        }
        LocalFree(argv);
        if (!valid || (uiTest && (capture.empty() || !explicitData))) {
            MessageBoxW(nullptr, L"사용법: prototype.exe [--data-dir 폴더] [--capture-dir 폴더] [--ui-test]", L"끗마릳기", MB_OK);
            return 1;
        }
        wchar_t windowsDirectory[MAX_PATH] = {};
        GetWindowsDirectoryW(windowsDirectory, MAX_PATH);
        const filesystem::path font = filesystem::path(windowsDirectory) / "Fonts" / "malgun.ttf";
        SfmlApplication application(data, font);
        return application.run(capture, uiTest);
    } catch (const exception& error) {
        const string detail = error.what();
        const int size = MultiByteToWideChar(CP_UTF8, 0, detail.c_str(), -1, nullptr, 0);
        wstring message(static_cast<size_t>(size > 0 ? size : 1), L'\0');
        if (size > 0) MultiByteToWideChar(CP_UTF8, 0, detail.c_str(), -1, &message[0], size);
        MessageBoxW(nullptr, message.c_str(), L"끗마릳기 실행 오류", MB_OK | MB_ICONERROR);
        return 1;
    }
}
