#include "framework.h"
#include "resource.h"
#include <sstream>
#include <iomanip>
#include <vector>           // vector
#include <functional>       // function
#include <exception>
using namespace std;
#define OUT_ID    1001
#define EDIT_ID   1002
#define ENDL "\r\n"
template<class T>
struct ForthList {          /// vector helper template class
    vector<T> v;            /// use proxy pattern
    T& operator[](int i) { return i < 0 ? v[v.size() + i] : v[i]; }
    T operator<<(T t)    { v.push_back(t); }
    T dec_i() { return v.back() -= 1; }     /// decrement stack top
    T pop()   {
        if (v.empty()) throw underflow_error("ERR: stack empty");
        T t = v.back(); v.pop_back(); return t; }
    void push(T t)            { v.push_back(t); }
    void clear()              { v.clear(); }
    void merge(vector<T>& v2) { v.insert(v.end(), v2.begin(), v2.end()); }
    void erase(int i)         { v.erase(v.begin() + i, v.end()); }
};
class Code;                                 /// forward declaration
using fop = function<void(Code*)>;          /// Forth operator
class Code {
public:
    static int fence;                       /// token incremental counter
    string name;                            /// name of word
    int    token = 0;                       /// dictionary order token
    bool   immd  = false;                   /// immediate flag
    int    stage = 0;                       /// branching stage
    fop    xt    = NULL;                    /// primitive function
    string literal;                         /// string literal
    ForthList<Code*> pf;
    ForthList<Code*> pf1;
    ForthList<Code*> pf2;
    ForthList<int>   qf;
    Code(string n, fop fn, bool im=false) { name = n; token = fence++; xt = fn; immd = im; }
    Code(string n, bool f=false)       { name = n; if (f) token = fence++; }
	Code(Code* c, string s = string()) { name = c->name; xt = c->xt; if (s != string()) literal = s; }
	Code(Code *c, int v)    { name = c->name; xt = c->xt; qf.push(v); }
	Code *immediate()       { immd = true;  return this; }
	Code *addcode(Code* w)  { pf.push(w);   return this; }
	string to_s() { return name + " " + to_string(token) + (immd ? "*" : ""); }
    string see(int dp) {
		stringstream cout("");
		auto see_pf = [&cout](int dp, string s, vector<Code*> v) {   // lambda for indentation and recursive dump
			int i = dp; cout << ENDL; while (i--) cout << "  "; cout << s;
			for (Code* w: v) cout << w->see(dp + 1); };
		auto see_qf = [&cout](vector<int> v) { cout << " = "; for (int i : v) cout << i << " "; };
		see_pf(dp, "[ " + to_s(), pf.v);
		if (pf1.v.size() > 0) see_pf(dp, "1--", pf1.v);
		if (pf2.v.size() > 0) see_pf(dp, "2--", pf2.v);
		if (qf.v.size()  > 0) see_qf(qf.v);
		cout << "]";
		return cout.str(); }
    void exec() {
		if (xt) xt(this);
		else { for (Code* w : pf.v) w->exec(); }}
};
int Code::fence = 0;
#define CODE(s, g) new Code(string(s), [&](Code *c){ g; })
#define IMMD(s, g) new Code(string(s), [&](Code *c){ g; }, true)
#define ALU(a, OP, b)  (static_cast<int>(a) OP static_cast<int>(b))
#define BOOL(f) ((f) ? -1 : 0)
class ForthVM {
    istream          &cin;                  /// stream input
	ostream          &cout;					/// stream output
    ForthList<int>   rs;                    /// return stack
    ForthList<int>   ss;                    /// parameter stack
    ForthList<Code*> dict;                  /// dictionary
    bool compile = false;                   /// compiling flag
    int  base    = 10;                      /// numeric radix
    int  top     = -1;                      /// cached top of stack
    int  WP      = 0;                       /// instruction and parameter pointers
	inline int POP()       { int n = top; top = ss.pop(); return n; }
	inline int PUSH(int v) { ss.push(top); return top = v; }
	Code *find(string s) {
        for (int i = (int)dict.v.size() - 1; i >= 0; --i) {
			if (s == dict.v[i]->name) return dict.v[i]; }
		return NULL; }
	string next_idiom(char delim=0) {
		string s; delim ? getline(cin, s, delim) : cin >> s; return s; }
	void dot_r(int n, int v) { cout << setw(n) << setfill(' ') << v; }
	void ss_dump() {
		cout << " <"; for (int i : ss.v) { cout << i << " "; }
		cout << top << "> ok" << ENDL; }
	void words() {
		int i = 0;
		for (Code* w : dict.v) {
			if ((i++ % 8) == 0) cout << ENDL;
			cout << w->to_s() << " "; }}
	void call(Code *w) {
		int tmp = WP;                                       /// * setup call frame
		WP = w->token;
		try { w->exec(); }                                  /// * run inner interpreter recursively
		catch (exception& e) {
			string msg = e.what();                          /// * capture exception message
			if (msg!=string()) cout << msg << ENDL; }
		WP = tmp; }                                         /// * restore call frame
public:
    ForthVM(istream& in, ostream& out) : cin(in), cout(out) {}
	void init() {
		static vector<Code*> prim = {                       /// singleton, build once only
			// stack op
			CODE("dup",  PUSH(top)),
			CODE("over", PUSH(ss[-1])),
			CODE("2dup", PUSH(ss[-1]); PUSH(ss[-1])),
			CODE("2over",PUSH(ss[-3]); PUSH(ss[-3])),
			CODE("4dup", PUSH(ss[-3]); PUSH(ss[-3]); PUSH(ss[-3]); PUSH(ss[-3])),
			CODE("swap", int n = ss.pop(); PUSH(n)),
			CODE("rot",  int n = ss.pop(); int m = ss.pop(); ss.push(n); PUSH(m)),
			CODE("-rot", int n = ss.pop(); int m = ss.pop(); PUSH(n); PUSH(m)),
			CODE("2swap",
				 int n = ss.pop(); int m = ss.pop(); int l = ss.pop();
				 ss.push(n); PUSH(l); PUSH(m)),
			CODE("pick", int i = top; top = ss[-i]),
			//    CODE("roll", int i = top; top = ss[-i]),
			CODE("drop", top = ss.pop()),
			CODE("nip",  ss.pop()),
			CODE("2drop",ss.pop(); top = ss.pop()),
			CODE(">r",   rs.push(POP())),
			CODE("r>",   PUSH(rs.pop())),
			CODE("r@",   PUSH(rs[-1])),
			CODE("push", rs.push(POP())),
			CODE("pop",  PUSH(rs.pop())),
			// ALU ops
			CODE("+",    top = ALU(ss.pop(), +, top)),
			CODE("-",    top = ALU(ss.pop(), -, top)),
			CODE("*",    top = ALU(ss.pop(), *, top)),
			CODE("/",    top = ALU(ss.pop(), /, top)),
			CODE("mod",  top = ALU(ss.pop(), %, top)),
			CODE("*/",   top = ss.pop() * ss.pop() / top),
			CODE("*/mod",
				 int n = static_cast<int>(ss.pop() * ss.pop());
				 int t = static_cast<int>(top);
				 ss.push(n % t); top = (n / t)),
			CODE("and",  top = ALU(ss.pop(), &, top)),
			CODE("or",   top = ALU(ss.pop(), |, top)),
			CODE("xor",  top = ALU(ss.pop(), ^, top)),
			CODE("negate", top = -top),
			CODE("abs",  top = abs(top)),
			// logic ops
			CODE("0= ",  top = BOOL(top == 0)),
			CODE("0<",   top = BOOL(top <  0)),
			CODE("0>",   top = BOOL(top >  0)),
			CODE("=",    top = BOOL(ss.pop() == top)),
			CODE(">",    top = BOOL(ss.pop() >  top)),
			CODE("<",    top = BOOL(ss.pop() <  top)),
			CODE("<>",   top = BOOL(ss.pop() != top)),
			CODE(">=",   top = BOOL(ss.pop() >= top)),
			CODE("<=",   top = BOOL(ss.pop() <= top)),
			// output
			CODE("base@",   PUSH(base)),
			CODE("base!",   cout << setbase(base = POP())),
			CODE("hex",     cout << setbase(base = 16)),
			CODE("decimal", cout << setbase(base = 10)),
			CODE("cr",      cout << ENDL),
			CODE(".",       cout << POP() << " "),
			CODE(".r",      int n = POP(); dot_r(n, POP())),
			CODE("u.r",     int n = POP(); dot_r(n, abs(POP()))),
			CODE(".f",      int n = POP(); cout << setprecision(n) << POP()),
			CODE("key",     PUSH(next_idiom()[0])),
			CODE("emit",    char b = (char)POP(); cout << b),
			CODE("space",   cout << " "),
			CODE("spaces",  for (int n = POP(), i = 0; i < n; i++) cout << " "),
			// literals
			CODE("dotstr",  cout << c->literal),
			CODE("dolit",   PUSH(c->qf[0])),
			CODE("dovar",   PUSH(c->token)),
			CODE("[",       compile = false),
			CODE("]",       compile = true),
			IMMD("(",       next_idiom(')')),
			IMMD(".(",      cout << next_idiom(')')),
			CODE("\\",      cout << next_idiom('\n')),
			CODE("$\"",
				 string s = next_idiom('"').substr(1);
				 dict[-1]->addcode(new Code(find("dovar"), s))),
			IMMD(".\"",
				 string s = next_idiom('"').substr(1);
				 dict[-1]->addcode(new Code(find("dotstr"), s))),
			// branching - if...then, if...else...then
			IMMD("bran",
				 bool f = POP() != 0;                        // check flag
				 for (Code* w : (f ? c->pf.v : c->pf1.v)) call(w)),
			IMMD("if",
				 dict[-1]->addcode(new Code(find("bran")));
				 dict.push(new Code("temp"))),               // use last cell of dictionay as scratch pad
			IMMD("else",
				 Code *temp = dict[-1]; Code *last = dict[-2]->pf[-1];
				 last->pf.merge(temp->pf.v);
				 temp->pf.clear();
				 last->stage = 1),
			IMMD("then",
				 Code *temp = dict[-1]; Code *last = dict[-2]->pf[-1];
				 if (last->stage == 0) {                     // if...then
					 last->pf.merge(temp->pf.v);
					 dict.pop();
				 }
				 else {                                      // if...else...then, or
					 last->pf1.merge(temp->pf.v);            // for...aft...then...next
					 if (last->stage == 1) dict.pop();
					 else temp->pf.clear();
				 }),
			// loops - begin...again, begin...f until, begin...f while...repeat
			CODE("loop",
				 while (true) {
					 for (Code* w : c->pf.v) call(w);                       // begin...
					 int f = top;
					 if (c->stage == 0 && (top = ss.pop(), f != 0)) break;  // ...until
					 if (c->stage == 1) continue;                           // ...again
					 if (c->stage == 2 && (top = ss.pop(), f == 0)) break;  // while...repeat
					 for (Code* w : c->pf1.v) call(w);
				 }),
			IMMD("begin",
				 dict[-1]->addcode(new Code(find("loop")));
				 dict.push(new Code("temp"))),
			IMMD("while",
				 Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
				 last->pf.merge(temp->pf.v);
				 temp->pf.clear(); last->stage = 2),
			IMMD("repeat",
				 Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
				 last->pf1.merge(temp->pf.v); dict.pop()),
			IMMD("again",
				 Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
				 last->pf.merge(temp->pf.v);
				 last->stage = 1; dict.pop()),
			IMMD("until",
				 Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
				 last->pf.merge(temp->pf.v); dict.pop()),
			// loops - for...next, for...aft...then...next
			CODE("cycle",
				 do { for (Code* w : c->pf.v) call(w); }
				 while (c->stage == 0 && rs.dec_i() >= 0);    // for...next only
				 while (c->stage > 0) {                       // aft
					 for (Code* w : c->pf2.v) call(w);        // then...next
					 if (rs.dec_i() < 0) break;
					 for (Code* w : c->pf1.v) call(w); }      // aft...then
				 rs.pop()),
			IMMD("for",
				 dict[-1]->addcode(new Code(find(">r")));
				 dict[-1]->addcode(new Code(find("cycle")));
				 dict.push(new Code("temp"))),
			IMMD("aft",
				 Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
				 last->pf.merge(temp->pf.v);
				 temp->pf.clear(); last->stage = 3),
			IMMD("next",
				 Code *last = dict[-2]->pf[-1]; Code *temp = dict[-1];
				 if (last->stage == 0) last->pf.merge(temp->pf.v);
				 else last->pf2.merge(temp->pf.v); dict.pop()),
			// compiler
			CODE("exit", throw domain_error(string())),
			CODE("exec", int n = top; call(dict[n])),
			CODE(":",
				 dict.push(new Code(next_idiom(), true));            // create new word
				 compile = true),
			IMMD(";", compile = false),
			CODE("variable",
				 dict.push(new Code(next_idiom(), true));
				 Code *last = dict[-1]->addcode(new Code(find("dovar"), 0));
				 last->pf[0]->token = last->token),
			CODE("constant",
				 dict.push(new Code(next_idiom(), true));
				 Code *last = dict[-1]->addcode(new Code(find("dolit"), POP()));
				 last->pf[0]->token = last->token),
			CODE("@",      int w = POP(); PUSH(dict[w]->pf[0]->qf[0])),         // w -- n
			CODE("!",      int w = POP(); dict[w]->pf[0]->qf[0] = POP()),       // n w --
			CODE("+!",     int w = POP(); dict[w]->pf[0]->qf[0] += POP()),      // n w --
			CODE("?",      int w = POP(); cout << dict[w]->pf[0]->qf[0] << " "),// w --
			CODE("array@", int a = POP(); PUSH(dict[POP()]->pf[0]->qf[a])),     // w a -- n
			CODE("array!", int a = POP(); int w = POP();  dict[w]->pf[0]->qf[a] = POP()),   // n w a --
			CODE("allot",                                            // n --
				 for (int n = POP(), i = 0; i < n; i++) dict[-1]->pf[0]->qf.push(0)),
			CODE(",",      dict[-1]->pf[0]->qf.push(POP())),
			CODE("create",
				 dict.push(new Code(next_idiom(), true));            // create a new word
				 Code *last = dict[-1]->addcode(new Code(find("dovar"), 0));
				 last->pf[0]->token = last->token;
				 last->pf[0]->qf.clear()),
			CODE("does",
				 vector<Code*> src = dict[WP]->pf.v;                 // source word : xx create...does...;
				 int i = 0; int n = (int)src.size();
				 while (i < n && src[i]->name != "does") i++;        // find the "does"
				 while (++i < n) dict[-1]->pf.push(src[i]);          // copy words after "does" to new the word
				 throw domain_error(string())),                      // break out of for { c->exec() } loop
			CODE("[to]",
				 vector<Code*> src = dict[WP]->pf.v;                 // source word : xx create...does...;
				 int i = 0; int n = (int)src.size();
				 while (i < n && src[i]->name != "[to]") i++;        // find the "does"
				 src[++i]->pf[0]->qf[0] = POP()),                    // change the following constant
			CODE("to",                                               // n -- , compile only
				 Code *tgt = find(next_idiom());
				 if (tgt) tgt->pf[0]->qf[0] = POP()),                // update constant
			CODE("is",                                               // w -- , execute only
				 Code *tgt = find(next_idiom());
				 if (tgt) {
					 tgt->pf.clear();
					 tgt->pf.merge(dict[POP()]->pf.v); }),
			// tools
			CODE("here",  PUSH(dict[-1]->token)),
			CODE("words", words()),
			CODE(".s",    ss_dump()),
			CODE("'",     Code *w = find(next_idiom()); PUSH(w->token)),
			CODE("see",
				 Code *w = find(next_idiom());
				 if (w) cout << w->see(0) << ENDL),
			CODE("forget",
				 Code *w = find(next_idiom());
				 if (w == NULL) return;
				 dict.erase(Code::fence=max(w->token, find("boot")->token + 1))),
			CODE("boot", dict.erase(Code::fence=find("boot")->token + 1))
		};
		dict.merge(prim); }                                 /// * populate dictionary
	void outer() {
		string idiom;
		while (cin >> idiom) {
			//printf("%s=>", idiom.c_str());
			Code *w = find(idiom);                          /// * search through dictionary
			if (w) {                                        /// * word found?
				//printf("%s\n", w->to_s().c_str());
				if (compile && !w->immd)                    /// * in compile mode?
					dict[-1]->addcode(w);                   /// * add to colon word
				else call(w); }                             /// * execute forth word 
			else {                                          /// * treat as a numeric?
				char *p;
				int n = (int)strtol(idiom.c_str(), &p, base);
				//printf("%d\n", n);
				if (*p != '\0') {                           /// * not number
					cout << idiom << "? " << ENDL;          ///> display error prompt
					compile = false;                        ///> reset to interpreter mode
					ss.clear(); top=-1;                     ///> clear stack
					getline(cin, idiom, '\n'); }            ///> skip the entire line
				else if (compile)                           /// * a number in compile mode?
					dict[-1]->addcode(new Code(find("dolit"), n)); ///> add to current word
				else PUSH(n); }}                           	///> or, add value onto data stack
		if (!compile) ss_dump(); }                          /// * dump stack and display ok prompt
};
std::istringstream forth_in;
std::ostringstream forth_out;
ForthVM* forth_vm = new ForthVM(forth_in, forth_out);
LRESULT CALLBACK WindowProc(HWND, UINT, WPARAM, LPARAM);
TCHAR  szClassName[] = _T("wineForth - eForth for Windows");
HWND   hwnd, TextBox, SendButton, TextField;
HACCEL Accel;
HFONT  Font = CreateFont(0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, _T("MingLiU"));
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpszArg, int nCmdShow) {
    MSG msg;                 /* Here messages to the application are saved */
    WNDCLASSEX wincl;        /* Data structure for the windowclass */
    setlocale(LC_ALL, ".utf8");                // enable Chinese
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
        _T("wineForth v501"),/* Title Text */
        WS_OVERLAPPEDWINDOW, /* default window */
        CW_USEDEFAULT,       /* Windows decides the position */
        CW_USEDEFAULT,       /* where the window ends up on the screen */
        1080,                /* The programs width */
        680,                 /* and height in pixels */
        HWND_DESKTOP,        /* The window is a child-window to desktop */
        NULL,                /* No menu */
        hInst,               /* Program Instance handler */
        NULL);               /* No Window Creation data */
    ShowWindow(hwnd, nCmdShow);
    TextField = CreateWindow(_T("EDIT"), _T(""),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
        0, 0, 580, 660,
        hwnd, (HMENU)OUT_ID, hInst, NULL);
    TextBox = CreateWindow(_T("EDIT"), _T("words"),
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | ES_AUTOVSCROLL | ES_MULTILINE,
        580, 0, 480, 660,
        hwnd, (HMENU)EDIT_ID, hInst, NULL);
    Accel = LoadAccelerators(hInst, MAKEINTRESOURCE(IDR_ACCELERATOR1));
    SendMessage(TextField, WM_SETFONT, WPARAM(Font), TRUE);
    SendMessage(TextBox, WM_SETFONT, WPARAM(Font), TRUE);
    forth_vm->init();
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(hwnd, Accel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg); }}
    return LOWORD(msg.wParam);
}
void ProcessCommand() {
    const int len = GetWindowTextLength(TextBox) + 1;
    TCHAR* text = new TCHAR[len];
    GetWindowText(TextBox, &text[0], len);
    char* cmd = (char*)malloc(len*4);
    size_t xlen;
    wcstombs_s(&xlen, cmd, len*4, text, len*4);
    SendMessage(TextField, EM_SETSEL, -1, -1);
    SendMessage(TextField, EM_REPLACESEL, 0, (LPARAM)text);
    forth_in.clear();
    forth_in.str(cmd);
    forth_vm->outer();
    string out = forth_out.str();
    size_t wclen = out.size() + 1;
    if (wclen > 1) {
        TCHAR* result = new TCHAR[wclen];
        mbstowcs_s(&xlen, result, wclen, out.c_str(), wclen);
        SendMessage(TextField, EM_SETSEL, -1, -1);
        SendMessage(TextField, EM_REPLACESEL, 0, (LPARAM)result);
        delete[] result;
        forth_out.str(""); }
    free(cmd);
    delete[] text;
    SetWindowText(TextBox, _T(""));
}
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg) {
    case WM_KEYDOWN: if (wParam == VK_ESCAPE) { ProcessCommand(); } break;
    case WM_COMMAND: if (LOWORD(wParam) == IDM_ABOUT) { ProcessCommand(); } break;
    case WM_DESTROY: PostQuitMessage(0); break;
    default: return DefWindowProc(hwnd, msg, wParam, lParam); }
    return 0;
}
