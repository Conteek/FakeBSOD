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
int process = 0;
COLORREF bsodColor = RGB(0, 0, 130); // Matches the real BSOD blue (#0078D7)
HHOOK keyboardHook = NULL;
extern int percent;
bool bottomBlockAppeared = false;

int revealHeight = 0;     // Высота синей области, которая «проявляется»
bool revealFinished = false;  // Закончена ли анимация синего фона
bool revealStarted = false;

int phase = 0;

float originalVolume = 1.0f; // Сохраняем начальный уровень громкости
IAudioEndpointVolume* pVolume = nullptr;

struct LineInfo {
    const wchar_t* text;
    int yOffset;
};

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
    if (process != 10)
    {
        for (int i = 0; i <= 10; i++)
        {
            Sleep(1000);
            process = i;
            InvalidateRect(hwnd, NULL, TRUE);
        }
    }

    while (totalPercent < 100) {
        if (process >= 10) {
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
    }

    Sleep(1500);

    bsodColor = RGB(0, 0, 0);
    percent = -1;

    InvalidateRect(hwnd, NULL, TRUE);

    Sleep(5000);

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
void DrawTextLine(HDC hdc, LPCWSTR text, int x, int y, int fontSize, int weight = FW_REGULAR) {
    HFONT font = CreateFontW(fontSize, 0, 0, 0, weight, FALSE, FALSE, FALSE,
        ANSI_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Lucida Console");
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
        SetTimer(hwnd, 1, 30, NULL); // 50 мс — для появления синего фона
        break;
    }
    case WM_TIMER: {
        RECT rc;
        GetClientRect(hwnd, &rc);

        if (!revealFinished) {
            if (!revealStarted)
            {
                Sleep(500);
            }
            revealStarted = true;
            revealHeight += 20; // шаг роста синей полосы
            InvalidateRect(hwnd, NULL, FALSE);

            if (revealHeight >= rc.bottom) {
                revealFinished = true;
                revealHeight = rc.bottom;

                // Переустановить таймер на 100 мс для фаз
                SetTimer(hwnd, 1, 100, NULL);

                // Запустить процесс через 500 мс
                std::thread([] {
                    Sleep(500);
                    std::thread(ProgressThread).detach();
                    }).detach();
            }
        }
        else if (phase <= 20) {
            phase++;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        else {
            KillTimer(hwnd, 1);
        }
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
        RECT revealRect = rc;
        revealRect.bottom = revealHeight;
        FillRect(memDC, &revealRect, bgBrush); // только до текущей высоты
        DeleteObject(bgBrush);

        int screenW = rc.right - rc.left;
        int screenH = rc.bottom - rc.top;

        int titleX = screenW * 0.01;
        int titleY = screenH * 0.03;
        int infoY = titleY + 130;
        int info2Y = infoY + 100;
        int info3Y = info2Y + 140;
        int info4Y = info3Y + 140;
        int stopcodeY = info4Y + 250;
        int processY = stopcodeY + 110;

        LineInfo lines[] = {
                        {L"A problem has been detected and Windows has been shut down to prevent damage", titleY},
                        {L"to your computer.", titleY + 40},
                        {L"A process or thread crucial to system operation has unexpectedly exited to been", infoY},
                        {L"terminated.", infoY + 40},
                        {L"If this is the first time you’ve seen this Stop error screen,", info2Y},
                        {L"restart your computer. If this screen appears again, follow", info2Y + 40},
                        {L"these steps:", info2Y + 80},
                        {L"Check to make sure any new hardware or software is properly installed.", info3Y},
                        {L"If this is a new installation, ask your hardware or software manufacturer", info3Y + 40},
                        {L"for any Windows updates you might need.", info3Y + 80},
                        {L"If problems continue, disable or remove any newly installed hardware", info4Y},
                        {L"or software. Disable BIOS memory options such as caching or shadowing.", info4Y + 40},
                        {L"If you need to use Safe Mode to remove or disable components, restart", info4Y + 80},
                        {L"your computer, press F8 to select Advanced Startup Options, and then", info4Y + 120},
                        {L"select Safe Mode.", info4Y + 160},
                        {L"Technical information", stopcodeY},
                        {L"*** STOP: 0x0000000A (0x0000000000000003, 0xFFFFFA80207F42A0, 0xFFFFFA80207F4580, 0xFFFFF80002DE0780)", stopcodeY + 40},
                        {nullptr, processY},
                        {nullptr, processY + 40},
                        {nullptr, processY + 80},
                        {nullptr, processY + 120},
        };

        std::wstring progressStr = L" ";

        wchar_t process1[64];
        wchar_t process2[64];
        wchar_t process3[64];

        switch (process) {
        case 0: swprintf(process1, 64, L" "); break;
        case 1: swprintf(process1, 64, L"Collecting data for crash dump ."); break;
        case 2: swprintf(process1, 64, L"Collecting data for crash dump .."); break;
        case 3: swprintf(process1, 64, L"Collecting data for crash dump ..."); break;
        default:
            if (process >= 3)
                swprintf(process1, 64, L"Collecting data for crash dump ...");
            else
                swprintf(process1, 64, L" ");
            break;
        }

        switch (process) {
        case 4: swprintf(process2, 64, L"Initializing disk for crash dump ."); break;
        case 5: swprintf(process2, 64, L"Initializing disk for crash dump .."); break;
        case 6: swprintf(process2, 64, L"Initializing disk for crash dump ..."); break;
        default:
            if (process <= 3)
                swprintf(process2, 64, L" ");
            else if (process >= 6)
                swprintf(process2, 64, L"Initializing disk for crash dump ...");
            break;
        }

        switch (process) {
        case 7: swprintf(process3, 64, L"Beginning dump of physical memory ."); break;
        case 8: swprintf(process3, 64, L"Beginning dump of physical memory .."); break;
        case 9: swprintf(process3, 64, L"Beginning dump of physical memory ..."); break;
        default:
            if (process <= 6)
                swprintf(process3, 64, L" ");
            else if (process >= 9)
                swprintf(process3, 64, L"Beginning dump of physical memory ...");
            break;
        }

        switch (process) {
        case 10: progressStr = L"Dumping physical memory to disk: " + std::to_wstring(percent); break;
        }

        lines[17].text = process1;
        lines[18].text = process2;
        lines[19].text = process3;
        lines[20].text = progressStr.c_str();


        if (percent >= 0) {
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, RGB(255, 255, 255));
            
            for (int i = 0; i < phase && i < _countof(lines); ++i) {
                if (lines[i].text != nullptr) {
                    DrawTextLine(memDC, lines[i].text, titleX, lines[i].yOffset, 14 * 2, FW_NORMAL);
                }
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

    keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, hInstance, 0);
    if (!keyboardHook) {
        MessageBox(NULL, L"Failed to install keyboard hook!", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    return 0;
}