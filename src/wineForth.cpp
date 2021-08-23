#include "framework.h"
#include "resource.h"
#include "src/ceforth.h"

#define OUT_ID    1001
#define EDIT_ID   1002
//
// ForthVM and IO streams
std::istringstream forth_in;
std::ostringstream forth_out;
ForthVM* forth_vm = new ForthVM(forth_in, forth_out);

// Win32 global variables
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
TCHAR  szClassName[] = _T("wineForth - eForth for Windows");
HWND   hwnd, hTextBox, hTextField;
HACCEL hAccel;
HFONT  hFont = CreateFont(0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, _T("MingLiU"));

// Win32 main
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpszArg, int nCmdShow)
{
    setlocale(LC_ALL, ".utf8");                // enable Chinese
    
    WNDCLASSEX wincl;                          // Data structure for the windowclass
    wincl.hInstance = hInst;
    wincl.lpszClassName = szClassName;
    wincl.lpfnWndProc = WindowProc;            // main message handler
    wincl.style = CS_DBLCLKS;                  // catch double-clicks
    wincl.cbSize = sizeof(WNDCLASSEX);
    wincl.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    wincl.hCursor = LoadCursor(NULL, IDC_ARROW);
    wincl.lpszMenuName = NULL;                 // no menu
    wincl.cbClsExtra = 0;                      // no extra bytes
    wincl.cbWndExtra = 0;                      // structure or the window instance
    wincl.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
    RegisterClassEx(&wincl);
    
    hwnd = CreateWindowEx(
        0,                   // Extended possibilites for variation
        szClassName,         // Classname
        _T("wineForth v501"),// Title Text
        WS_OVERLAPPEDWINDOW, // default window
        CW_USEDEFAULT,       // Windows decides the position
        CW_USEDEFAULT,       // where the window ends up on the screen
        1080,                // The programs width
        680,                 // and height in pixels
        HWND_DESKTOP,        // The window is a child-window to desktop
        NULL,                // No menu
        hInst,               // Program Instance handle
        NULL                 // No Window Creation data
    );
    ShowWindow(hwnd, nCmdShow);

    hTextField = CreateWindow(_T("EDIT"), _T(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
        0, 0, 580, 660,
        hwnd, (HMENU)OUT_ID, hInst, NULL);
    hTextBox = CreateWindow(_T("EDIT"), _T("words"),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
        580, 0, 480, 660,
        hwnd, (HMENU)EDIT_ID, hInst, NULL);
    // setup keyboard accelerator Alt-Return
    hAccel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));

    // change to fixed font
    SendMessage(hTextField, WM_SETFONT, WPARAM(hFont), TRUE);
    SendMessage(hTextBox, WM_SETFONT, WPARAM(hFont), TRUE);

    // intialize ForthVM
    forth_vm->init();
    
    // start Win32 message loop
    MSG msg;                                   // application message
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    return LOWORD(msg.wParam);
}
//
// execute ForthVM outer interpreter
//
void ProcessCommand() {
    // fetch utf-8 input into char[] buffer
    const int len = GetWindowTextLength(hTextBox) + 1;
    // allocate char memory and retrieve user input
    TCHAR* text = new TCHAR[len];
    GetWindowText(hTextBox, &text[0], len);
    char* cmd = (char*)malloc(len*4);
    size_t xlen;
    wcstombs_s(&xlen, cmd, len*4, text, len*4);
    
    // paste command into output panel
    SendMessage(hTextField, EM_SETSEL, -1, -1);
    SendMessage(hTextField, EM_REPLACESEL, 0, (LPARAM)text);
    
    // reset input stream of ForthVM with new command line
    forth_in.clear();
    forth_in.str(cmd);
    
    // kick off outer interpreter
    forth_vm->outer();
    
    // fetch output stream from ForthVM (in string<char>)
    string out = forth_out.str();
    size_t wclen = out.size() + 1;
    if (wclen > 1) {
        TCHAR* result = new TCHAR[wclen];
        mbstowcs_s(&xlen, result, wclen, out.c_str(), wclen);
        
        // show it on output panel
        SendMessage(hTextField, EM_SETSEL, -1, -1);
        SendMessage(hTextField, EM_REPLACESEL, 0, (LPARAM)result);
        delete[] result;
        forth_out.str("");
    }
    // free allocated memory
    free(cmd);
    delete[] text;
    SetWindowText(hTextBox, _T(""));
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE) { ProcessCommand(); } break;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDM_ABOUT) { ProcessCommand(); } break;
    case WM_DESTROY: PostQuitMessage(0); break;  // send a WM_QUIT
    default: return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}
