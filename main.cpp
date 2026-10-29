// Windows 헤더를 먼저 읽어 byte 이름 충돌을 방지한다.
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#endif

#include "Application.h"
#include <filesystem>
#include <iostream>
#include <random>
#include <vector>

using namespace std;

namespace {
filesystem::path findDataDirectory(const filesystem::path& executable) {
    error_code error;
    const auto working = filesystem::current_path(error);
    if (error) return executable.parent_path() / "data";
    const auto executableDirectory = executable.parent_path();
    const vector<filesystem::path> candidates {
        working / "data",
        executableDirectory / "data",
        executableDirectory.parent_path() / "data",
        executableDirectory.parent_path().parent_path() / "data"
    };
    for (const auto& candidate : candidates) {
        error.clear();
        if (filesystem::is_regular_file(candidate / "words.txt", error) && !error) return candidate;
    }
    return working / "data";
}
int start(const filesystem::path& executable, const filesystem::path& requestedData) {
    try {
        const auto data = requestedData.empty() ? findDataDirectory(executable) : requestedData;
        return runApplication(data, cin, cout, random_device{}());
    } catch (const exception& error) {
        cerr << "프로그램 실행 오류: " << error.what() << '\n';
        return 1;
    }
}
}

#ifdef _WIN32
int wmain(int argc, wchar_t* argv[]) {
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    filesystem::path requested;
    if (argc == 3 && wstring(argv[1]) == L"--data-dir") requested = argv[2];
    else if (argc != 1) {
        cout << "사용법: prototype.exe [--data-dir 사전과기록폴더]\n"; return 1;
    }
    return start(filesystem::absolute(argv[0]), requested);
}
#else
int main(int argc, char* argv[]) {
    filesystem::path requested;
    if (argc == 3 && string(argv[1]) == "--data-dir") requested = filesystem::u8path(argv[2]);
    else if (argc != 1) {
        cout << "Usage: prototype [--data-dir directory]\n"; return 1;
    }
    return start(filesystem::absolute(argv[0]), requested);
}
#endif

