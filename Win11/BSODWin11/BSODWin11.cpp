#include <windows.h>
#include <thread>
#include <string>
#include <random>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys.h>
#include <comdef.h>

// Window class name
#define CLASS_NAME L"BSODWindow"

// Global variables
HWND hwnd;
int percent = 0;
COLORREF bsodColor = RGB(0, 0, 0); // Matches the real BSOD blue (#0078D7)
HHOOK keyboardHook = NULL;
extern int percent;
bool textAppeared = false;
bool bottomBlockAppeared = false;


float originalVolume = 1.0f;
IAudioEndpointVolume* pVolume = nullptr;

void MuteSystemSound() {
    CoInitialize(NULL);

    IMMDeviceEnumerator* pEnumerator = NULL;
    IMMDevice* pDevice = NULL;

    HRESULT hr = CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL, CLSCTX_ALL,
        __uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
    if (FAILED(hr)) return;

    hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
    if (FAILED(hr)) {
        pEnumerator->Release();
        return;
    }

    hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL, NULL, (void**)&pVolume);
    if (FAILED(hr)) {
        pDevice->Release();
        pEnumerator->Release();
        return;
    }

    pVolume->GetMasterVolumeLevelScalar(&originalVolume);

    pVolume->SetMasterVolumeLevelScalar(0.0f, NULL);

    pDevice->Release();
    pEnumerator->Release();
}

void RestoreSystemSound() {
    if (pVolume) {
        pVolume->SetMasterVolumeLevelScalar(originalVolume, NULL);
        pVolume->Release();
        pVolume = nullptr;
    }

    CoUninitialize();
}

// Simulate progress bar timing
void ProgressThread() {
    std::random_device rd;
    std::mt19937 gen(rd());

    int totalPercent = 0;
    int slower = 2;

    while (totalPercent < 100) {
        int delayMs;
        int increment;

        if (totalPercent < 10) {
            std::uniform_int_distribution<> delayDist(150, 200);
            std::uniform_int_distribution<> incDist(1, 3);
            delayMs = delayDist(gen);
            increment = incDist(gen);
        }
        else if (totalPercent < 40) {
            std::uniform_int_distribution<> delayDist(200, 300);
            std::uniform_int_distribution<> incDist(1, 2);
            delayMs = delayDist(gen);
            increment = incDist(gen);
        }
        else if (totalPercent < 75) {
            std::uniform_int_distribution<> delayDist(400, 700);
            std::uniform_int_distribution<> incDist(1, 2);
            delayMs = delayDist(gen);
            increment = incDist(gen);
        }
        else {
            std::uniform_int_distribution<> delayDist(100, 300);
            std::uniform_int_distribution<> incDist(1, 2);
            delayMs = delayDist(gen);
            increment = incDist(gen);
        }

        Sleep(delayMs * slower);
        totalPercent += increment;
        if (totalPercent > 100) totalPercent = 100;
        percent = totalPercent;
        InvalidateRect(hwnd, NULL, TRUE);
    }
    Sleep(3000);

    bsodColor = RGB(0, 0, 0);
    percent = -1;

    InvalidateRect(hwnd, NULL, TRUE);

    Sleep(3000);

    RestoreSystemSound();
    ExitProcess(0);
}

void DrawLightTextLineCentered(HDC hdc, LPCWSTR text, int centerX, int y, int fontSize, int weight = FW_LIGHT) {
    HFONT font = CreateFontW(fontSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Light");

    HFONT oldFont = (HFONT)SelectObject(hdc, font);

    SIZE textSize;
    GetTextExtentPoint32W(hdc, text, lstrlenW(text), &textSize);

    int x = centerX - textSize.cx / 2;

    TextOutW(hdc, x, y, text, lstrlenW(text));

    SelectObject(hdc, oldFont);
    DeleteObject(font);
}


// Function to draw text with specified font size and weight (Segoe UI Semilight)
void DrawSemilightTextLine(HDC hdc, LPCWSTR text, int x, int y, int fontSize, int weight = FW_REGULAR) {
    HFONT font = CreateFontW(fontSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Semilight");
    SelectObject(hdc, font);
    TextOutW(hdc, x, y, text, lstrlenW(text));
    DeleteObject(font);
}

// Keyboard hook procedure
LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0) {
        int vkCode = ((KBDLLHOOKSTRUCT*)lParam)->vkCode;
        bool block = false;

        switch (vkCode) {
        case VK_CONTROL:
        case VK_LCONTROL:
        case VK_RCONTROL:
        case VK_MENU:
        case VK_LMENU:
        case VK_RMENU:
        case VK_SHIFT:
        case VK_LSHIFT:
        case VK_RSHIFT:
        case VK_LWIN:
        case VK_RWIN:
        case VK_ESCAPE:
        case VK_TAB:
        case VK_F1:
        case VK_F2:
        case VK_F3:
        case VK_F4:
        case VK_F5:
        case VK_F6:
        case VK_F7:
        case VK_F8:
        case VK_F9:
        case VK_F10:
        case VK_F11:
        case VK_F12:
        case VK_DELETE:
            block = true;
            break;
        }

        if (block) {
            return 1; // Block the key
        }
    }
    return CallNextHookEx(keyboardHook, nCode, wParam, lParam);
}

// Window procedure
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);

        RECT rc;
        GetClientRect(hwnd, &rc);

        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP memBitmap = CreateCompatibleBitmap(hdc, rc.right - rc.left, rc.bottom - rc.top);
        HBITMAP oldBitmap = (HBITMAP)SelectObject(memDC, memBitmap);

        HBRUSH bgBrush = CreateSolidBrush(bsodColor);
        FillRect(memDC, &rc, bgBrush);
        DeleteObject(bgBrush);

        if (percent >= 0) {
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));

            int screenW = rc.right - rc.left;
            int screenH = rc.bottom - rc.top;

            int X = screenW * 0.5;

            int textY = screenH * 0.4;
            DrawLightTextLineCentered(memDC, L"Your PC ran into a problem and needs to restart", X, textY, 28 * 2);

            Sleep(50);

            int progressY = textY + 60 * 1.7;
            std::wstring progressStr = std::to_wstring(percent) + L"% complete";
            DrawLightTextLineCentered(memDC, progressStr.c_str(), X, progressY, 24 * 2.4);

            int stopCodeY = screenH * 0.9;
            DrawLightTextLineCentered(memDC, L"Stop code: CRITICAL_PROCESS_DIED", X, stopCodeY, 25);

            int whatFailedY = stopCodeY + 35;
            DrawLightTextLineCentered(memDC, L"What failed:", X, whatFailedY, 25);
        }

        BitBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, SRCCOPY);

        SelectObject(memDC, oldBitmap);
        DeleteObject(memBitmap);
        DeleteDC(memDC);

        EndPaint(hwnd, &ps);
        break;
    }

    case WM_DESTROY:
        if (keyboardHook) {
            UnhookWindowsHookEx(keyboardHook);
        }
        ShowCursor(TRUE);
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// Main entry point
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    WNDCLASS wc = {};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    RegisterClass(&wc);

    hwnd = CreateWindowEx(
        WS_EX_TOPMOST,
        CLASS_NAME,
        L"BSOD",
        WS_POPUP,
        0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN),
        NULL, NULL, hInstance, NULL);

    if (!hwnd) return 0;

    ShowCursor(FALSE);
    MuteSystemSound();
    ShowWindow(hwnd, SW_SHOW);
    UpdateWindow(hwnd);

    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);
    if (!keyboardHook) {
        MessageBox(NULL, L"Failed to install keyboard hook!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    std::thread t(ProgressThread);
    t.detach();

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}