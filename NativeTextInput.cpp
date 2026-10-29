// Windows 헤더를 먼저 읽어 using namespace std와 byte 이름 충돌을 피한다.
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <CommCtrl.h>
#include <imm.h>
#include "NativeTextInput.h"
#include <vector>
#include <limits>
#include <algorithm>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "comctl32.lib")
#pragma comment(lib, "imm32.lib")

struct NativeTextInput::Impl {
    HWND parent = nullptr;
    HWND edit = nullptr;
    HFONT font = nullptr;
    bool composing = false;
    bool submit = false;
    bool pause = false;
    bool ignoreReturn = false;
    bool returnHandled = false;
    bool submitAfterComposition = false;

    static constexpr UINT SubmitAfterIme = WM_APP + 41;

    bool hasComposition() const {
        if (composing) return true;
        if (!edit) return false;
        HIMC context = ImmGetContext(edit);
        if (!context) return false;
        const LONG length = ImmGetCompositionStringW(context, GCS_COMPSTR, nullptr, 0);
        ImmReleaseContext(edit, context);
        return length > 0;
    }

    void resetKeys() {
        ignoreReturn = false;
        returnHandled = false;
    }

    static LRESULT CALLBACK editProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
        Impl* self = reinterpret_cast<Impl*>(data);
        switch (message) {
        case WM_IME_STARTCOMPOSITION:
            self->composing = true;
            break;
        case WM_IME_ENDCOMPOSITION:
            self->composing = false;
            // 한글 조합을 확정한 같은 Enter로 제출한다. 조합 결과가 EDIT에
            // 반영된 다음 프레임에 읽히도록 사용자 메시지를 큐에 넣는다.
            if (self->submitAfterComposition)
                PostMessageW(window, SubmitAfterIme, 0, 0);
            break;
        case WM_IME_KEYDOWN:
            if (wParam == VK_RETURN && self->hasComposition()) {
                self->submitAfterComposition = true;
                self->returnHandled = true;
            }
            break;
        case WM_IME_KEYUP:
            if (wParam == VK_RETURN) self->resetKeys();
            break;
        case WM_KEYDOWN:
            // VK_PROCESSKEY는 한/영 입력기가 처리한 키이다. 원래 문자 입력은 EDIT에 맡긴다.
            if (wParam == VK_PROCESSKEY && self->hasComposition()
                && (GetKeyState(VK_RETURN) & 0x8000) != 0) {
                self->ignoreReturn = true;
            }
            if (wParam == 'A' && (GetKeyState(VK_CONTROL) & 0x8000) != 0) {
                SendMessageW(window, EM_SETSEL, 0, -1);
                return 0;
            }
            if (wParam == VK_ESCAPE) {
                if (self->hasComposition()) break;
                const bool repeated = (lParam & (static_cast<LPARAM>(1) << 30)) != 0;
                if (!repeated) self->pause = true;
                return 0;
            }
            if (wParam == VK_RETURN) {
                if (self->hasComposition()) {
                    self->submitAfterComposition = true;
                    self->returnHandled = true;
                    break; // 조합 확정 처리는 원래 EDIT가 수행한다.
                }
                if (self->ignoreReturn) break;
                const bool repeated = (lParam & (static_cast<LPARAM>(1) << 30)) != 0;
                if (!repeated && !self->returnHandled) self->submit = true;
                self->returnHandled = true;
                return 0;
            }
            break;
        case SubmitAfterIme:
            if (self->submitAfterComposition) {
                self->submitAfterComposition = false;
                self->submit = true;
            }
            return 0;
        case WM_CHAR:
            if (wParam == '\r') {
                // KEYDOWN에서 이미 제출했다면 WM_CHAR에서는 중복 제출하지 않는다.
                if (!self->returnHandled && !self->ignoreReturn && !self->hasComposition()) {
                    self->submit = true;
                    self->returnHandled = true;
                }
                return 0; // 한 줄 입력창의 Enter 경고음을 막는다.
            }
            if (wParam == 1) return 0; // Ctrl+A의 문자 메시지
            break;
        case WM_KEYUP:
            if (wParam == VK_RETURN || wParam == VK_PROCESSKEY) self->resetKeys();
            break;
        case WM_KILLFOCUS:
            self->resetKeys();
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(window, editProc, id);
            self->edit = nullptr;
            self->composing = false;
            self->submit = false;
            self->pause = false;
            self->submitAfterComposition = false;
            break;
        default:
            break;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    static LRESULT CALLBACK parentProc(HWND window, UINT message, WPARAM wParam,
        LPARAM lParam, UINT_PTR id, DWORD_PTR data) {
        Impl* self = reinterpret_cast<Impl*>(data);
        if ((message == WM_CTLCOLOREDIT || message == WM_CTLCOLORSTATIC)
            && reinterpret_cast<HWND>(lParam) == self->edit) {
            HDC dc = reinterpret_cast<HDC>(wParam);
            SetTextColor(dc, RGB(28, 38, 55));
            SetBkColor(dc, RGB(255, 255, 255));
            return reinterpret_cast<LRESULT>(GetStockObject(WHITE_BRUSH));
        }
        if (message == WM_NCDESTROY) {
            RemoveWindowSubclass(window, parentProc, id);
            self->parent = nullptr;
        }
        return DefSubclassProc(window, message, wParam, lParam);
    }

    void destroy() {
        if (parent && IsWindow(parent)) {
            RemoveWindowSubclass(parent, parentProc, reinterpret_cast<UINT_PTR>(this));
        }
        if (edit && IsWindow(edit)) DestroyWindow(edit);
        edit = nullptr;
        parent = nullptr;
        if (font) DeleteObject(font);
        font = nullptr;
        composing = false;
        submit = false;
        pause = false;
        submitAfterComposition = false;
        resetKeys();
    }
};

NativeTextInput::NativeTextInput() : impl_(new Impl) {}

NativeTextInput::~NativeTextInput() {
    impl_->destroy();
    delete impl_;
}

bool NativeTextInput::create(void* parentWindow, int x, int y, int width, int height) {
    impl_->destroy();
    HWND parent = static_cast<HWND>(parentWindow);
    if (!IsWindow(parent) || width <= 0 || height <= 0) return false;
    // SFML이 매 프레임 다시 그려도 자식 입력창 영역은 덮어 그리지 않는다.
    const LONG_PTR parentStyle = GetWindowLongPtrW(parent, GWL_STYLE);
    SetWindowLongPtrW(parent, GWL_STYLE, parentStyle | WS_CLIPCHILDREN);
    impl_->parent = parent;
    impl_->edit = CreateWindowExW(0, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | WS_TABSTOP | WS_CLIPSIBLINGS | ES_AUTOHSCROLL,
        x, y, width, height, parent, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!impl_->edit) {
        impl_->destroy();
        return false;
    }
    const UINT_PTR id = reinterpret_cast<UINT_PTR>(impl_);
    const DWORD_PTR data = reinterpret_cast<DWORD_PTR>(impl_);
    if (!SetWindowSubclass(impl_->edit, Impl::editProc, id, data)
        || !SetWindowSubclass(parent, Impl::parentProc, id, data)) {
        impl_->destroy();
        return false;
    }
    impl_->font = CreateFontW(-22, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
        DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
    HFONT font = impl_->font ? impl_->font : static_cast<HFONT>(GetStockObject(DEFAULT_GUI_FONT));
    SendMessageW(impl_->edit, WM_SETFONT, reinterpret_cast<WPARAM>(font), TRUE);
    SendMessageW(impl_->edit, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(10, 10));
    // 실제 게임은 64음절까지 검사한다. 그보다 여유 있게 받아 오류 안내가 가능하게 한다.
    SendMessageW(impl_->edit, EM_SETLIMITTEXT, 128, 0);
    SendMessageW(impl_->edit, EM_SETCUEBANNER, TRUE, reinterpret_cast<LPARAM>(L"단어를 입력하세요"));
    return true;
}

void NativeTextInput::setBounds(int x, int y, int width, int height) {
    if (impl_->edit && width > 0 && height > 0) {
        SetWindowPos(impl_->edit, nullptr, x, y, width, height, SWP_NOZORDER | SWP_NOACTIVATE);
        // 입력 글자도 창 크기와 함께 확대한다. IME와 커서는 기본 EDIT가 처리한다.
        HFONT resizedFont = CreateFontW(-max(18, height * 2 / 3), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"맑은 고딕");
        if (resizedFont) {
            SendMessageW(impl_->edit, WM_SETFONT, reinterpret_cast<WPARAM>(resizedFont), TRUE);
            if (impl_->font) DeleteObject(impl_->font);
            impl_->font = resizedFont;
        }
    }
}

void NativeTextInput::setVisible(bool visible) {
    if (!impl_->edit) return;
    if (!visible && GetFocus() == impl_->edit) SetFocus(impl_->parent);
    ShowWindow(impl_->edit, visible ? SW_SHOWNA : SW_HIDE);
    if (!visible) {
        impl_->submit = false;
        impl_->pause = false;
        impl_->resetKeys();
    }
}

void NativeTextInput::setEnabled(bool enabled) {
    if (!impl_->edit) return;
    if (!enabled && GetFocus() == impl_->edit) SetFocus(impl_->parent);
    EnableWindow(impl_->edit, enabled ? TRUE : FALSE);
    if (!enabled) {
        impl_->submit = false;
        impl_->resetKeys();
    }
}

void NativeTextInput::focus() {
    if (impl_->edit && IsWindowVisible(impl_->edit) && IsWindowEnabled(impl_->edit)) {
        SetFocus(impl_->edit);
    }
}

void NativeTextInput::clear() {
    if (!impl_->edit) return;
    if (impl_->hasComposition()) {
        HIMC context = ImmGetContext(impl_->edit);
        if (context) {
            ImmNotifyIME(context, NI_COMPOSITIONSTR, CPS_CANCEL, 0);
            ImmReleaseContext(impl_->edit, context);
        }
    }
    SetWindowTextW(impl_->edit, L"");
    impl_->composing = false;
    impl_->submit = false;
    impl_->submitAfterComposition = false;
    // Enter를 길게 눌러도 clear() 이후 같은 키가 다시 제출되지 않도록 키 상태는 유지한다.
}

string NativeTextInput::textUtf8() const {
    if (!impl_->edit) return {};
    const int length = GetWindowTextLengthW(impl_->edit);
    if (length <= 0) return {};
    vector<wchar_t> buffer(static_cast<size_t>(length) + 1);
    const int copied = GetWindowTextW(impl_->edit, buffer.data(), length + 1);
    if (copied <= 0) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS,
        buffer.data(), copied, nullptr, 0, nullptr, nullptr);
    if (bytes <= 0) return string(1, static_cast<char>(0xff));
    string result(static_cast<size_t>(bytes), '\0');
    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, buffer.data(), copied,
        result.data(), bytes, nullptr, nullptr);
    return result;
}

void NativeTextInput::setTextUtf8(const string& text) {
    if (!impl_->edit) return;
    if (text.empty()) {
        clear();
        return;
    }
    if (text.size() > static_cast<size_t>((numeric_limits<int>::max)())) return;
    const int bytes = static_cast<int>(text.size());
    const int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), bytes, nullptr, 0);
    if (length <= 0 || length > 128 || text.find('\0') != string::npos) return;
    wstring result(static_cast<size_t>(length), L'\0');
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), bytes, result.data(), length);
    clear();
    SetWindowTextW(impl_->edit, result.c_str());
    SendMessageW(impl_->edit, EM_SETSEL, length, length);
}

bool NativeTextInput::consumeSubmit() {
    const bool submitted = impl_->submit;
    impl_->submit = false;
    return submitted;
}

bool NativeTextInput::consumePause() {
    const bool paused = impl_->pause;
    impl_->pause = false;
    return paused;
}

bool NativeTextInput::isComposing() const {
    return impl_->hasComposition();
}
