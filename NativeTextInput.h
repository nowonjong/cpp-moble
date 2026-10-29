#pragma once
#include <string>

using namespace std;

// 6번 UI: 한글 조합/커서/복사 붙여넣기는 Windows 기본 입력창이 처리한다.
// 게임에는 textUtf8()으로 변환한 UTF-8 문자열만 전달한다.
class NativeTextInput {
public:
    NativeTextInput();
    ~NativeTextInput();
    NativeTextInput(const NativeTextInput&) = delete;
    NativeTextInput& operator=(const NativeTextInput&) = delete;

    bool create(void* parentWindow, int x, int y, int width, int height);
    void setBounds(int x, int y, int width, int height);
    void setVisible(bool visible);
    void setEnabled(bool enabled);
    void focus();
    void clear();
    string textUtf8() const;
    void setTextUtf8(const string& text);
    bool consumeSubmit();
    bool consumePause();
    bool isComposing() const;

private:
    // Windows 자료형을 이 헤더 밖으로 숨겨 다른 C++ 파일과 충돌하지 않게 한다.
    struct Impl;
    Impl* impl_;
};

