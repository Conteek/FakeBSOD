#include <windows.h>
#include <thread>
#include <string>
#include "resource.h" // Убедитесь, что этот файл существует и содержит #define IDB_BITMAP1 134
#include <random>
#include <mmdeviceapi.h>
#include <endpointvolume.h>
#include <functiondiscoverykeys.h>
#include <comdef.h>

// Window class name
#define CLASS_NAME L"BSODWindow"

// Resource ID for the bitmap
#define IDB_BITMAP1 129 // Соответствует вашему определению, убедитесь, что совпадает с resource.h

// Global variables
HWND hwnd;
int percent = 0;
COLORREF bsodColor = RGB(0, 120, 215); // Matches the real BSOD blue (#0078D7)
HHOOK keyboardHook = NULL;
extern int percent;
bool bottomBlockAppeared = false;


float originalVolume = 1.0f; // Сохраняем начальный уровень громкости
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
    int slower = 1;

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

    Sleep(6000);

    RestoreSystemSound();
    ExitProcess(0);
}

// Function to draw text (Segoe UI Light)
void DrawLightTextLine(HDC hdc, LPCWSTR text, int x, int y, int fontSize, int weight = FW_LIGHT) {
    HFONT font = CreateFontW(fontSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Segoe UI Light");
    SelectObject(hdc, font);
    TextOutW(hdc, x, y, text, lstrlenW(text));
    DeleteObject(font);
}

// Function to draw text (Segoe UI Semilight)
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
    case WM_CREATE: {
        SetTimer(hwnd, 1, 400, NULL);
        break;
    }
    case WM_TIMER: {
        bottomBlockAppeared = true;
        KillTimer(hwnd, 1);
        InvalidateRect(hwnd, NULL, FALSE);
        break;
    }
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

            int emoticonX = screenW * 0.1;
            int emoticonY = screenH * 0.11;
            DrawSemilightTextLine(memDC, L":(", emoticonX, emoticonY, 112 * 2.4, FW_NORMAL);

            int textX = screenW * 0.1;
            int textY = emoticonY + 150 * 1.85;
            DrawSemilightTextLine(memDC, L"Your PC ran into a problem and needs to restart. We're", textX, textY, 28 * 2);
            textY += 60;
            DrawSemilightTextLine(memDC, L"just collecting some error info, and then we'll restart for", textX, textY, 28 * 2);
            textY += 60;
            DrawSemilightTextLine(memDC, L"you.", textX, textY, 28 * 2.2);

            int progressX = screenW * 0.1;
            int progressY = textY + 60 * 1.67;
            std::wstring progressStr = std::to_wstring(percent) + L"% complete";
            DrawLightTextLine(memDC, progressStr.c_str(), progressX, progressY, 24 * 2.4);

            if (bottomBlockAppeared == true)
            {
                int qrX = screenW * 0.1;
                int qrY = screenH * 0.665;
                HBITMAP hBitmap = LoadBitmap(GetModuleHandle(NULL), MAKEINTRESOURCE(IDB_BITMAP1));
                if (hBitmap) {
                    HDC qrDC = CreateCompatibleDC(memDC);
                    HBITMAP oldQrBmp = (HBITMAP)SelectObject(qrDC, hBitmap);
                    BITMAP bmp;
                    GetObject(hBitmap, sizeof(BITMAP), &bmp);
                    int qrSize = min(bmp.bmWidth, 115);
                    BitBlt(memDC, qrX, qrY, qrSize, qrSize, qrDC, 0, 0, SRCCOPY);
                    SelectObject(qrDC, oldQrBmp);
                    DeleteDC(qrDC);
                    DeleteObject(hBitmap);
                }
                else {
                    int qrSize = 115;
                    HBRUSH whiteBrush = CreateSolidBrush(RGB(255, 255, 255));
                    RECT qrRect = { qrX, qrY, qrX + qrSize, qrY + qrSize };
                    FillRect(memDC, &qrRect, whiteBrush);
                    DeleteObject(whiteBrush);
                    wchar_t errorMsg[256];
                    wsprintf(errorMsg, L"Failed to load IDB_BITMAP1 (Error: %d). Check if qrcode.bmp is a valid 24-bit BMP.", GetLastError());
                    OutputDebugString(errorMsg);
                }

                int siteX = screenW * 0.1 + 115 + 20;
                int siteY = qrY - 5;
                DrawLightTextLine(memDC, L"For more information about this issue and possible fixes, visit https://www.windows.com/stopcode", siteX, siteY, 30);

                int infoX = screenW * 0.1 + 115 + 20;
                int infoY = siteY + 60;
                DrawLightTextLine(memDC, L"If you call a support person, give them this info:", infoX, infoY, 23);

                int stopCodeX = screenW * 0.1 + 115 + 20;
                int stopCodeY = infoY + 35;
                DrawLightTextLine(memDC, L"Stop code: CRITICAL_PROCESS_DIED", stopCodeX, stopCodeY, 25);
            }
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

    //keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);
    //if (!keyboardHook) {
    //    MessageBox(NULL, L"Failed to install keyboard hook!", L"Error", MB_OK | MB_ICONERROR);
    //    return 1;
    //}

    std::thread t(ProgressThread);
    t.detach();

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}