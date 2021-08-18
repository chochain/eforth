#if defined(UNICODE) && !defined(_UNICODE)
#define _UNICODE
#elif defined(_UNICODE) && !defined(UNICODE)
#define UNICODE
#endif
#define OUT_ID    1001
#define EDIT_ID   1002
#define PRESS_ID  1003
#define IDM_ABOUT        104
#define IDR_ACCELERATOR1 129

#include <tchar.h>
#include <windows.h>
#include <sstream>
#include <cstdlib>
#include "ceforth.h"
//
// ForthVM and IO streams
std::istringstream forth_in;
std::ostringstream forth_out;
ForthVM* forth_vm = new ForthVM(forth_in, forth_out);
// Win32 variables
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
TCHAR  szClassName[] = _T("ForthWindowsApp");
HWND   hwnd, TextBox, SendButton, TextField;
HACCEL Accel;
HFONT  Font = (HFONT)GetStockObject(ANSI_FIXED_FONT);
// Win32 main
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpszArg, int nCmdShow)
{
    MSG msg;                 /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */
    wincl.hInstance = hInst;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProc;            /* This function is called by windows */
    wincl.style = CS_DBLCLKS;                  /* Catch double-clicks */
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;                 /* No menu */
    wincl.cbClsExtra = 0;                      /* No extra bytes after the window class */
    wincl.cbWndExtra = 0;                      /* structure or the window instance */
    wincl.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
    RegisterClassEx(&wincl);
    hwnd = CreateWindowEx(
        0,                   /* Extended possibilites for variation */
        szClassName,         /* Classname */
        _T("ceforth207"),    /* Title Text */
        WS_OVERLAPPEDWINDOW, /* default window */
        CW_USEDEFAULT,       /* Windows decides the position */
        CW_USEDEFAULT,       /* where the window ends up on the screen */
        1080,                /* The programs width */
        680,                 /* and height in pixels */
        HWND_DESKTOP,        /* The window is a child-window to desktop */
        NULL,                /* No menu */
        hInst,               /* Program Instance handler */
        NULL                 /* No Window Creation data */
    );
    ShowWindow(hwnd, nCmdShow);
    TextField = CreateWindow(_T("EDIT"), _T(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
        0, 0, 580, 600,
        hwnd, (HMENU)OUT_ID, hInst, NULL);
    TextBox = CreateWindow(_T("EDIT"), _T("words"),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
        580, 0, 480, 600,
        hwnd, (HMENU)EDIT_ID, hInst, NULL);
    SendButton = CreateWindow(_T("BUTTON"), _T("Send"),
        WS_VISIBLE | WS_CHILD | WS_BORDER,
        10, 608, 65, 28,
        hwnd, (HMENU)PRESS_ID, hInst, NULL);
    // add Alt-Return
    Accel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));
    // change to fixed font
    SendMessage(TextField, WM_SETFONT, WPARAM(Font), TRUE);
    SendMessage(TextBox, WM_SETFONT, WPARAM(Font), TRUE);
    // setup keyboard accelerator Alt-Return
    // intialize ForthVM
    forth_vm->init();
    // start Win32 message loop
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hwnd, Accel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return msg.wParam;
}
//
// execute ForthVM outer interpreter
//
void ProcessCommand() {
    // fetch input (wchar_t) string
    const int len = GetWindowTextLength(TextBox) + 1;
    TCHAR* text = new TCHAR[len];
    GetWindowText(TextBox, &text[0], len);
    // prepare command for input in char[]
    char* cmd = (char*)malloc(len + 1);
    size_t xlen;
    wcstombs_s(&xlen, cmd, len, text, len);
    printf("%s in %d bytes\n", cmd, xlen);
    // paste command into output panel
    SendMessage(TextField, EM_SETSEL, -1, -1);
    SendMessage(TextField, EM_REPLACESEL, 0, (LPARAM)text);
    // send command to ForthVM outer interpreter
    forth_in.clear();
    forth_in.str(cmd);
    forth_vm->outer();
    // process output (in string<char>)
    string out = forth_out.str();
    printf("%s\n", out.c_str());
    size_t wclen = out.size() + 1;
    if (wclen > 1) {
        TCHAR* result = new TCHAR[wclen];
        mbstowcs_s(&xlen, result, wclen, out.c_str(), wclen);
        // show it on output panel
        SendMessage(TextField, EM_SETSEL, -1, -1);
        SendMessage(TextField, EM_REPLACESEL, 0, (LPARAM)result);
        // clean up memory blocks allocated
        delete[] result;
        forth_out.str("");
    }
    free(cmd);
    delete[] text;
    SetWindowText(TextBox, _T(""));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)                  /* handle the messages */
    {
    case WM_COMMAND:
        if ((LOWORD(wParam) == PRESS_ID) ||
            (LOWORD(wParam) == IDM_ABOUT)) {  // Alt-Return
            ProcessCommand();
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);       /* send a WM_QUIT to the message queue */
        break;
    default:                      /* for messages that we don't deal with */
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
