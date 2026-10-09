// ============================================================
// 修改电脑设备名 - C++ Win32 GUI 程序
// 功能：
//   1. 显示当前计算机名
//   2. 输入新名称（仅 ASCII 字母/数字/连字符，不支持中文）
//   3. 修改计算机名（需管理员权限，修改后需注销/重启生效）
//   4. 自动备份原名称，支持一键还原
//
// 编译（MinGW-w64）:
//   g++ -O2 -std=c++17 -municode -o RenameComputer.exe main.cpp \
//       -lshlwapi -lshell32 -ladvapi32 -lkernel32 -lgdi32 -luser32 -lcomctl32
//
// 编译（MSVC）:
//   cl /O2 /EHsc /DUNICODE /D_UNICODE /utf-8 main.cpp \
//      /link /OUT:RenameComputer.exe shlwapi.lib shell32.lib advapi32.lib \
//      comctl32.lib user32.lib gdi32.lib kernel32.lib
// ============================================================

#include <windows.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <commctrl.h>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>
#include <thread>
#include <map>
#include <map>

#pragma comment(lib, "shlwapi.lib")
#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "comctl32.lib")

// 控件 ID
#define IDC_CURRENT_NAME    1001
#define IDC_BACKUP_NAME     1002
#define IDC_NEW_NAME        1003
#define IDC_BTN_MODIFY      1004
#define IDC_BTN_RESTORE     1005
#define IDC_BTN_RELAUNCH    1006
#define IDC_STATUS          1007
#define IDC_HINT            1008
#define IDC_BTN_REFRESH     1009
#define IDC_BTN_LOGOFF      1010
#define IDC_BTN_REBOOT      1011
#define IDC_BTN_SAVE        1012
#define IDC_BTN_REGEDIT     1013
#define WM_REFRESH_DONE     (WM_USER + 1)

// 右键菜单命令
#define IDM_COPY            2001
#define IDM_SELECTALL       2002

// 子类化原始过程
WNDPROC g_origEditProc = nullptr;

// 全局句柄
HINSTANCE g_hInst = nullptr;
HWND g_hMainWnd = nullptr;
HWND g_hEditCurrent = nullptr;
HWND g_hEditBackup = nullptr;
HWND g_hEditNew = nullptr;
HWND g_hBtnModify = nullptr;
HWND g_hBtnRestore = nullptr;
HWND g_hBtnRelaunch = nullptr;
HWND g_hBtnRefresh = nullptr;
HWND g_hBtnLogoff = nullptr;
HWND g_hBtnReboot = nullptr;
HWND g_hBtnSave = nullptr;
HWND g_hBtnRegedit = nullptr;
HWND g_hStaticStatus = nullptr;
HWND g_hStaticHint = nullptr;

HFONT g_hFontTitle = nullptr;
HFONT g_hFontNormal = nullptr;
HFONT g_hFontSmall = nullptr;

// 当前状态
std::wstring g_currentName;
std::wstring g_backupName;
bool g_isAdmin = false;

// 获取 exe 所在目录（带末尾反斜杠）
std::wstring GetExeDir() {
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    wchar_t drive[_MAX_DRIVE] = {0};
    wchar_t dir[_MAX_DIR] = {0};
    _wsplitpath_s(exePath, drive, _MAX_DRIVE, dir, _MAX_DIR, nullptr, 0, nullptr, 0);
    wchar_t path[MAX_PATH] = {0};
    _wmakepath_s(path, MAX_PATH, drive, dir, nullptr, nullptr);
    return std::wstring(path);
}

// ==================== 工具函数 ====================
std::wstring Trim(const std::wstring& s) {
    size_t start = s.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return L"";
    size_t end = s.find_last_not_of(L" \t\r\n");
    return s.substr(start, end - start + 1);
}

// 判断是否为 OEM 默认设备名格式（DESKTOP-XXXXXXX / LAPTOP-XXXXXXX 等）
// 规则：前缀为 DESKTOP- / LAPTOP- / TABLET- / WIN- 等大写字母+连字符，
//       后跟至少 1 个字符（通常为随机字母数字）
bool IsOemStyleName(const std::wstring& name) {
    if (name.empty()) return false;
    // 查找连字符
    size_t dash = name.find(L'-');
    if (dash == std::wstring::npos || dash == 0 || dash == name.size() - 1) {
        return false;
    }
    // 连字符前必须全部为大写字母（前缀，如 DESKTOP / LAPTOP / TABLET / WIN）
    for (size_t i = 0; i < dash; ++i) {
        if (!(name[i] >= L'A' && name[i] <= L'Z')) {
            return false;
        }
    }
    // 前缀长度至少 2 个字母
    if (dash < 2) return false;
    // 连字符后必须为字母或数字（至少 1 个）
    for (size_t i = dash + 1; i < name.size(); ++i) {
        wchar_t c = name[i];
        if (!((c >= L'A' && c <= L'Z') ||
              (c >= L'a' && c <= L'z') ||
              (c >= L'0' && c <= L'9'))) {
            return false;
        }
    }
    return true;
}

bool IsAdmin() {
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY NtAuthority = SECURITY_NT_AUTHORITY;
    PSID AdminGroup = nullptr;
    if (AllocateAndInitializeSid(&NtAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID, DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0, &AdminGroup)) {
        CheckTokenMembership(nullptr, AdminGroup, &isAdmin);
        FreeSid(AdminGroup);
    }
    return isAdmin != FALSE;
}

std::wstring GetMyComputerName() {
    wchar_t buf[256] = {0};
    DWORD size = 256;
    // ComputerNamePhysicalDnsHostname = 5
    if (GetComputerNameExW((COMPUTER_NAME_FORMAT)5, buf, &size)) {
        return std::wstring(buf);
    }
    // 回退
    size = 256;
    if (GetComputerNameW(buf, &size)) {
        return std::wstring(buf);
    }
    return L"unknown";
}

bool SetMyComputerName(const std::wstring& newName, std::wstring& errMsg) {
    // ComputerNamePhysicalDnsHostname = 5
    if (!SetComputerNameExW((COMPUTER_NAME_FORMAT)5, newName.c_str())) {
        DWORD err = GetLastError();
        if (err == 87) {
            errMsg = L"参数错误(87)：名称可能不合法或与当前名相同";
        } else if (err == 5) {
            errMsg = L"拒绝访问(5)：需要管理员权限";
        } else {
            errMsg = L"SetComputerNameExW 失败，错误码：" + std::to_wstring(err);
        }
        return false;
    }

    // SetComputerNameExW 只写 ComputerName 和 NV Hostname，
    // 还需要手动写以下注册表项才能让当前会话立即生效：
    std::wstring upperName = newName;
    for (auto& c : upperName) c = towupper(c);

    // 1. ActiveComputerName（重启时系统读取的当前活动名）
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ActiveComputerName",
        0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dataSize = (DWORD)((upperName.size() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, L"ComputerName", 0, REG_SZ,
            (const BYTE*)upperName.c_str(), dataSize);
        RegCloseKey(hKey);
    }

    // 2. Tcpip\Parameters\Hostname（hostname 命令和 DNS 读取的主机名）
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Services\\Tcpip\\Parameters",
        0, KEY_SET_VALUE, &hKey) == ERROR_SUCCESS) {
        DWORD dataSize = (DWORD)((newName.size() + 1) * sizeof(wchar_t));
        RegSetValueExW(hKey, L"Hostname", 0, REG_SZ,
            (const BYTE*)newName.c_str(), dataSize);
        RegCloseKey(hKey);
    }

    return true;
}

// ==================== 名称校验 ====================
bool ValidateComputerName(const std::wstring& name, std::wstring& errMsg) {
    if (name.empty()) {
        errMsg = L"名称不能为空";
        return false;
    }
    if (name.size() > 15) {
        errMsg = L"名称长度不能超过 15 个字符";
        return false;
    }
    for (wchar_t c : name) {
        if (!((c >= L'A' && c <= L'Z') ||
              (c >= L'a' && c <= L'z') ||
              (c >= L'0' && c <= L'9') ||
              c == L'-')) {
            errMsg = L"名称包含非法字符（仅支持字母、数字、连字符，不支持中文）";
            return false;
        }
    }
    bool allDigit = true;
    for (wchar_t c : name) {
        if (!(c >= L'0' && c <= L'9')) { allDigit = false; break; }
    }
    if (allDigit) {
        errMsg = L"名称不能全为数字";
        return false;
    }
    if (name.front() == L'-' || name.back() == L'-') {
        errMsg = L"名称不能以连字符(-)开头或结尾";
        return false;
    }
    errMsg.clear();
    return true;
}

bool EqNoCase(const std::wstring& a, const std::wstring& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i) {
        if (towupper(a[i]) != towupper(b[i])) return false;
    }
    return true;
}

// ==================== 备份存储（注册表优先，txt 兼容旧版） ====================
// 注册表位置：HKLM\SOFTWARE\RenameComputer\OriginalName
// 旧版兼容：exe 同目录下的 <设备名>.txt 文件

// 注册表路径
const wchar_t* REG_KEY = L"SOFTWARE\\RenameComputer";
const wchar_t* REG_VAL = L"OriginalName";

// 前向声明
bool SaveBackupName(const std::wstring& name);

bool LoadBackupName(std::wstring& outName) {
    outName.clear();

    // 只从注册表读取备份
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t buf[256] = {0};
        DWORD bufSize = sizeof(buf);
        DWORD type = 0;
        if (RegQueryValueExW(hKey, REG_VAL, nullptr, &type, (LPBYTE)buf, &bufSize) == ERROR_SUCCESS) {
            outName = buf;
        }
        RegCloseKey(hKey);
    }
    return !outName.empty();
}

bool SaveBackupName(const std::wstring& name) {
    bool ok = false;

    // 1. 写注册表
    HKEY hKey;
    DWORD disp = 0;
    if (RegCreateKeyExW(HKEY_LOCAL_MACHINE, REG_KEY, 0, nullptr, 0,
        KEY_WRITE, nullptr, &hKey, &disp) == ERROR_SUCCESS) {
        DWORD dataSize = (DWORD)((name.size() + 1) * sizeof(wchar_t));
        if (RegSetValueExW(hKey, REG_VAL, 0, REG_SZ, (const BYTE*)name.c_str(), dataSize) == ERROR_SUCCESS) {
            ok = true;
        }
        RegCloseKey(hKey);
    }

    return ok;
}

// ==================== 颜色主题 ====================
// 深色主题配色
#define COLOR_BG          RGB(32, 34, 37)      // 主背景 深灰
#define COLOR_PANEL       RGB(45, 48, 56)      // 面板背景
#define COLOR_TEXT        RGB(230, 230, 230)   // 主文字
#define COLOR_TEXT_DIM    RGB(160, 160, 160)   // 次要文字
#define COLOR_EDIT_BG     RGB(55, 58, 67)      // 输入框背景
#define COLOR_ACCENT      RGB(0, 120, 215)     // 蓝色（修改）
#define COLOR_SUCCESS     RGB(0, 150, 80)      // 绿色（还原）
#define COLOR_WARN        RGB(180, 120, 0)     // 橙色（注销/重启）
#define COLOR_INFO        RGB(80, 80, 85)      // 灰色（刷新）
#define COLOR_CYAN        RGB(0, 150, 170)     // 青色（保存）
#define COLOR_HINT_OK     RGB(80, 200, 120)    // 合法提示绿
#define COLOR_HINT_ERR    RGB(240, 100, 100)   // 错误提示红

HBRUSH g_hbrBg = nullptr;
HBRUSH g_hbrPanel = nullptr;
HBRUSH g_hbrEditBg = nullptr;

// 按钮类型
enum BtnType { BTN_BLUE, BTN_GREEN, BTN_ORANGE, BTN_GRAY, BTN_CYAN };
struct BtnStyle { BtnType type; bool hover; bool pressed; };
std::map<HWND, BtnStyle> g_btnStyles;

// ==================== 字体 ====================
HFONT CreateFontX(int size, bool bold = false) {
    LOGFONTW lf = {0};
    HDC hdc = GetDC(nullptr);
    lf.lfHeight = -MulDiv(size, GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(nullptr, hdc);
    lf.lfWeight = bold ? FW_BOLD : FW_NORMAL;
    lf.lfCharSet = DEFAULT_CHARSET;
    wcscpy_s(lf.lfFaceName, LF_FACESIZE, L"Microsoft YaHei UI");
    return CreateFontIndirectW(&lf);
}

// ==================== 自绘按钮 ====================
LRESULT CALLBACK BtnSubProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

WNDPROC g_origBtnProc = nullptr;

void SubclassButton(HWND hBtn, BtnType type) {
    if (!g_origBtnProc) {
        g_origBtnProc = (WNDPROC)SetWindowLongPtrW(hBtn, GWLP_WNDPROC, (LONG_PTR)BtnSubProc);
    } else {
        SetWindowLongPtrW(hBtn, GWLP_WNDPROC, (LONG_PTR)BtnSubProc);
    }
    g_btnStyles[hBtn] = {type, false, false};
}

COLORREF GetBtnColor(BtnType type, bool hover, bool pressed) {
    COLORREF base;
    switch (type) {
    case BTN_BLUE:   base = COLOR_ACCENT;  break;
    case BTN_GREEN:  base = COLOR_SUCCESS; break;
    case BTN_ORANGE: base = COLOR_WARN;    break;
    case BTN_CYAN:   base = COLOR_CYAN;    break;
    default:         base = COLOR_INFO;    break;
    }
    if (pressed) {
        return RGB(max(0, GetRValue(base)-30), max(0, GetGValue(base)-30), max(0, GetBValue(base)-30));
    }
    if (hover) {
        return RGB(min(255, GetRValue(base)+25), min(255, GetGValue(base)+25), min(255, GetBValue(base)+25));
    }
    return base;
}

LRESULT CALLBACK BtnSubProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_MOUSEMOVE: {
        auto& style = g_btnStyles[hWnd];
        bool wasHover = style.hover;
        style.hover = true;
        if (!wasHover) {
            // 跟踪鼠标离开
            TRACKMOUSEEVENT tme = {sizeof(tme)};
            tme.dwFlags = TME_LEAVE;
            tme.hwndTrack = hWnd;
            TrackMouseEvent(&tme);
            InvalidateRect(hWnd, nullptr, FALSE);
        }
        break;
    }
    case WM_MOUSELEAVE: {
        auto& style = g_btnStyles[hWnd];
        style.hover = false;
        style.pressed = false;
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    }
    case WM_LBUTTONDOWN: {
        g_btnStyles[hWnd].pressed = true;
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    }
    case WM_LBUTTONUP: {
        g_btnStyles[hWnd].pressed = false;
        InvalidateRect(hWnd, nullptr, FALSE);
        break;
    }
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        RECT rc;
        GetClientRect(hWnd, &rc);

        auto& style = g_btnStyles[hWnd];
        COLORREF bgColor = GetBtnColor(style.type, style.hover, style.pressed);

        // 填充背景
        HBRUSH hBrush = CreateSolidBrush(bgColor);
        FillRect(hdc, &rc, hBrush);
        DeleteObject(hBrush);

        // 绘制文字
        wchar_t text[256] = {0};
        GetWindowTextW(hWnd, text, 256);
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, RGB(255, 255, 255));
        HFONT hFont = (HFONT)SendMessageW(hWnd, WM_GETFONT, 0, 0);
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
        UINT align = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
        // 小按钮用小字体
        if (rc.bottom - rc.top <= 30) {
            align |= DT_CENTER;
        }
        DrawTextW(hdc, text, -1, &rc, align);
        SelectObject(hdc, hOldFont);

        EndPaint(hWnd, &ps);
        return 0;
    }
    case WM_ERASEBKGND:
        return 1;  // 阻止默认背景绘制
    }
    return CallWindowProcW(g_origBtnProc, hWnd, msg, wParam, lParam);
}

// ==================== 提权重启 ====================
bool RelaunchAsAdmin() {
    wchar_t exePath[MAX_PATH] = {0};
    GetModuleFileNameW(nullptr, exePath, MAX_PATH);
    SHELLEXECUTEINFOW sei = {0};
    sei.cbSize = sizeof(sei);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.lpVerb = L"runas";
    sei.lpFile = exePath;
    sei.nShow = SW_SHOWNORMAL;
    return ShellExecuteExW(&sei) != FALSE;
}

// ==================== Edit 控件辅助 ====================
void SetEditText(HWND hEdit, const std::wstring& text) {
    SetWindowTextW(hEdit, text.c_str());
}

std::wstring GetEditText(HWND hEdit) {
    int len = GetWindowTextLengthW(hEdit);
    if (len <= 0) return L"";
    std::vector<wchar_t> buf(len + 1, 0);
    GetWindowTextW(hEdit, buf.data(), len + 1);
    return std::wstring(buf.data());
}

// ==================== UI 刷新 ====================
void RefreshStatus() {
    if (g_isAdmin) {
        SetWindowTextW(g_hStaticStatus, L"● 管理员权限：已获取");
    } else {
        SetWindowTextW(g_hStaticStatus, L"○ 管理员权限：未获取（修改需提权）");
    }
    SetEditText(g_hEditCurrent, g_currentName);
    SetEditText(g_hEditBackup, g_backupName.empty() ? L"（无备份）" : g_backupName);
}

void RefreshHint() {
    std::wstring name = Trim(GetEditText(g_hEditNew));
    if (name.empty()) {
        SetWindowTextW(g_hStaticHint, L"");
        return;
    }
    size_t len = name.size();
    std::wstring errMsg;
    if (ValidateComputerName(name, errMsg)) {
        std::wstring msg = L"✓ 名称合法（" + std::to_wstring(len) + L"/15 字符）";
        SetWindowTextW(g_hStaticHint, msg.c_str());
    } else {
        std::wstring msg = L"✗ " + errMsg + L"（已输入 " + std::to_wstring(len) + L" 字符）";
        SetWindowTextW(g_hStaticHint, msg.c_str());
    }
}

// ==================== Edit 子类化：支持右键复制菜单 ====================
void ShowEditContextMenu(HWND hEdit) {
    HMENU hMenu = CreatePopupMenu();
    AppendMenuW(hMenu, MF_STRING, IDM_COPY, L"复制(&C)\tCtrl+C");
    AppendMenuW(hMenu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(hMenu, MF_STRING, IDM_SELECTALL, L"全选(&A)\tCtrl+A");

    // 根据是否有选中文本决定"复制"是否可用
    DWORD selStart = 0, selEnd = 0;
    SendMessageW(hEdit, EM_GETSEL, (WPARAM)&selStart, (LPARAM)&selEnd);
    if (selStart == selEnd) {
        EnableMenuItem(hMenu, IDM_COPY, MF_BYCOMMAND | MF_GRAYED);
    }

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hEdit);
    int cmd = TrackPopupMenu(hMenu, TPM_RIGHTBUTTON | TPM_RETURNCMD,
        pt.x, pt.y, 0, hEdit, nullptr);
    DestroyMenu(hMenu);

    if (cmd == IDM_COPY) {
        SendMessageW(hEdit, WM_COPY, 0, 0);
    } else if (cmd == IDM_SELECTALL) {
        SendMessageW(hEdit, EM_SETSEL, 0, -1);
    }
}

LRESULT CALLBACK EditSubProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_RBUTTONUP: {
        ShowEditContextMenu(hWnd);
        return 0;
    }
    case WM_KEYDOWN: {
        // Ctrl+C 复制
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'C') {
            SendMessageW(hWnd, WM_COPY, 0, 0);
            return 0;
        }
        // Ctrl+A 全选
        if ((GetKeyState(VK_CONTROL) & 0x8000) && wParam == 'A') {
            SendMessageW(hWnd, EM_SETSEL, 0, -1);
            return 0;
        }
        break;
    }
    case WM_GETDLGCODE: {
        // 让 Edit 接收方向键和 Ctrl+C/Ctrl+A
        return DLGC_WANTCHARS | DLGC_WANTARROWS;
    }
    }
    return CallWindowProcW(g_origEditProc, hWnd, msg, wParam, lParam);
}

void SubclassEdit(HWND hEdit) {
    if (!g_origEditProc) {
        g_origEditProc = (WNDPROC)SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubProc);
    } else {
        SetWindowLongPtrW(hEdit, GWLP_WNDPROC, (LONG_PTR)EditSubProc);
    }
}

// ==================== 创建控件 ====================
void CreateControls(HWND hWnd) {
    g_hFontTitle = CreateFontX(20, true);
    g_hFontNormal = CreateFontX(13);
    g_hFontSmall = CreateFontX(11);

    // 创建画刷
    g_hbrBg = CreateSolidBrush(COLOR_BG);
    g_hbrPanel = CreateSolidBrush(COLOR_PANEL);
    g_hbrEditBg = CreateSolidBrush(COLOR_EDIT_BG);

    int x = 24;
    int y = 20;
    int w = 492;

    // 标题
    HWND hTitle = CreateWindowExW(0, L"STATIC", L"修改电脑设备名",
        WS_CHILD | WS_VISIBLE | SS_CENTER,
        x, y, w, 32, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hTitle, WM_SETFONT, (WPARAM)g_hFontTitle, TRUE);
    y += 42;

    // 分隔线
    HWND hLine1 = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        x, y, w, 2, hWnd, nullptr, g_hInst, nullptr);
    y += 12;

    // 当前计算机名 - 标签
    HWND hLbl1 = CreateWindowExW(0, L"STATIC", L"当前计算机名",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hLbl1, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    y += 22;
    g_hEditCurrent = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY | ES_CENTER,
        x, y, w, 28, hWnd, (HMENU)IDC_CURRENT_NAME, g_hInst, nullptr);
    SendMessageW(g_hEditCurrent, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SubclassEdit(g_hEditCurrent);
    y += 38;

    // 备份的原名称 - 标签
    HWND hLbl2 = CreateWindowExW(0, L"STATIC", L"已备份的原名称（用于还原）",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hLbl2, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    y += 22;
    g_hEditBackup = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_READONLY | ES_CENTER,
        x, y, w, 28, hWnd, (HMENU)IDC_BACKUP_NAME, g_hInst, nullptr);
    SendMessageW(g_hEditBackup, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    SubclassEdit(g_hEditBackup);
    y += 38;

    // 输入新名称 - 标签
    HWND hLbl3 = CreateWindowExW(0, L"STATIC", L"输入新的计算机名",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hLbl3, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    y += 22;
    g_hEditNew = CreateWindowExW(WS_EX_CLIENTEDGE, L"EDIT", L"",
        WS_CHILD | WS_VISIBLE | ES_AUTOHSCROLL | ES_CENTER,
        x, y, w, 28, hWnd, (HMENU)IDC_NEW_NAME, g_hInst, nullptr);
    SendMessageW(g_hEditNew, WM_SETFONT, (WPARAM)g_hFontNormal, TRUE);
    y += 32;
    g_hStaticHint = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, hWnd, (HMENU)IDC_HINT, g_hInst, nullptr);
    SendMessageW(g_hStaticHint, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    y += 26;

    // 主操作按钮：修改 / 保存当前名 / 还原
    int btnW = (w - 24) / 3;
    g_hBtnModify = CreateWindowExW(0, L"BUTTON", L"修改计算机名",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x, y, btnW, 36, hWnd, (HMENU)IDC_BTN_MODIFY, g_hInst, nullptr);
    SendMessageW(g_hBtnModify, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SubclassButton(g_hBtnModify, BTN_BLUE);

    g_hBtnSave = CreateWindowExW(0, L"BUTTON", L"保存当前名",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x + btnW + 12, y, btnW, 36, hWnd, (HMENU)IDC_BTN_SAVE, g_hInst, nullptr);
    SendMessageW(g_hBtnSave, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SubclassButton(g_hBtnSave, BTN_CYAN);

    g_hBtnRestore = CreateWindowExW(0, L"BUTTON", L"还原为备份名",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x + (btnW + 12) * 2, y, btnW, 36, hWnd, (HMENU)IDC_BTN_RESTORE, g_hInst, nullptr);
    SendMessageW(g_hBtnRestore, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SubclassButton(g_hBtnRestore, BTN_GREEN);
    y += 46;

    // 刷新网络名称 / 打开注册表
    int btnW3 = (w - 12) / 2;
    g_hBtnRefresh = CreateWindowExW(0, L"BUTTON", L"刷新网络名称",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x, y, btnW3, 32, hWnd, (HMENU)IDC_BTN_REFRESH, g_hInst, nullptr);
    SendMessageW(g_hBtnRefresh, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SubclassButton(g_hBtnRefresh, BTN_GRAY);

    g_hBtnRegedit = CreateWindowExW(0, L"BUTTON", L"打开注册表",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x + btnW3 + 12, y, btnW3, 32, hWnd, (HMENU)IDC_BTN_REGEDIT, g_hInst, nullptr);
    SendMessageW(g_hBtnRegedit, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SubclassButton(g_hBtnRegedit, BTN_GRAY);
    y += 40;

    // 注销 / 重启
    int btnW2 = (w - 12) / 2;
    g_hBtnLogoff = CreateWindowExW(0, L"BUTTON", L"注销 Windows",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x, y, btnW2, 32, hWnd, (HMENU)IDC_BTN_LOGOFF, g_hInst, nullptr);
    SendMessageW(g_hBtnLogoff, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SubclassButton(g_hBtnLogoff, BTN_ORANGE);

    g_hBtnReboot = CreateWindowExW(0, L"BUTTON", L"重启电脑",
        WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | BS_OWNERDRAW,
        x + btnW2 + 12, y, btnW2, 32, hWnd, (HMENU)IDC_BTN_REBOOT, g_hInst, nullptr);
    SendMessageW(g_hBtnReboot, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    SubclassButton(g_hBtnReboot, BTN_ORANGE);
    y += 40;

    // 管理员状态
    g_hStaticStatus = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, hWnd, (HMENU)IDC_STATUS, g_hInst, nullptr);
    SendMessageW(g_hStaticStatus, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    y += 24;

    // 分隔线
    HWND hLine2 = CreateWindowExW(0, L"STATIC", L"",
        WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ,
        x, y, w, 2, hWnd, nullptr, g_hInst, nullptr);
    y += 8;

    // 说明文字（每行单独一个 Static，避免截断）
    HWND hTip1 = CreateWindowExW(0, L"STATIC",
        L"仅支持字母/数字/连字符(-)，不支持中文，长度 1~15",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hTip1, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    y += 20;
    HWND hTip2 = CreateWindowExW(0, L"STATIC",
        L"修改后需注销或重启才能完全生效，OEM 默认名自动备份",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hTip2, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
    y += 20;
    HWND hTip3 = CreateWindowExW(0, L"STATIC",
        L"备份位置：HKLM\\SOFTWARE\\RenameComputer\\OriginalName",
        WS_CHILD | WS_VISIBLE | SS_LEFT,
        x, y, w, 18, hWnd, nullptr, g_hInst, nullptr);
    SendMessageW(hTip3, WM_SETFONT, (WPARAM)g_hFontSmall, TRUE);
}

// ==================== 主窗口过程 ====================
LRESULT CALLBACK MainWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE: {
        CreateControls(hWnd);
        g_isAdmin = true;  // 能运行到这里说明已是管理员
        g_currentName = GetMyComputerName();
        // 启动时加载已有备份
        LoadBackupName(g_backupName);
        // 如果没有备份，且当前名是 OEM 默认格式（DESKTOP-XXX / LAPTOP-XXX 等），
        // 则自动保存当前名为备份
        if (g_backupName.empty() && IsOemStyleName(g_currentName)) {
            g_backupName = g_currentName;
            SaveBackupName(g_backupName);
        }
        RefreshStatus();
        return 0;
    }
    case WM_COMMAND: {
        WORD id = LOWORD(wParam);
        WORD code = HIWORD(wParam);
        if (code == EN_CHANGE && (HWND)lParam == g_hEditNew) {
            RefreshHint();
            return 0;
        }
        switch (id) {
        case IDC_BTN_MODIFY: {
            std::wstring newName = Trim(GetEditText(g_hEditNew));
            std::wstring errMsg;
            if (!ValidateComputerName(newName, errMsg)) {
                MessageBoxW(hWnd, errMsg.c_str(), L"名称不合法", MB_OK | MB_ICONWARNING);
                return 0;
            }
            if (EqNoCase(newName, g_currentName)) {
                MessageBoxW(hWnd, L"新名称与当前名称相同，无需修改", L"提示", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            std::wstring confirm = L"将把计算机名从\r\n    " + g_currentName +
                L"\r\n修改为\r\n    " + newName + L"\r\n\r\n确认继续？";
            if (MessageBoxW(hWnd, confirm.c_str(), L"确认修改", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                return 0;
            }
            if (g_backupName.empty()) {
                g_backupName = g_currentName;
            }
            if (!SetMyComputerName(newName, errMsg)) {
                MessageBoxW(hWnd, errMsg.c_str(), L"修改失败", MB_OK | MB_ICONERROR);
                return 0;
            }
            SaveBackupName(g_backupName);
            g_currentName = newName;
            SetEditText(g_hEditNew, L"");
            SetWindowTextW(g_hStaticHint, L"");
            RefreshStatus();
            MessageBoxW(hWnd,
                L"计算机名已成功写入注册表。\r\n\r\n"
                L"注意：需要注销或重启 Windows 后新名称才会完全生效。",
                L"修改成功", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case IDC_BTN_RESTORE: {
            if (g_backupName.empty()) {
                MessageBoxW(hWnd, L"没有可还原的备份名称", L"提示", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            if (EqNoCase(g_backupName, g_currentName)) {
                MessageBoxW(hWnd, L"当前名称已是备份的原名称，无需还原", L"提示", MB_OK | MB_ICONINFORMATION);
                return 0;
            }
            std::wstring confirm = L"将把计算机名还原为：\r\n    " + g_backupName + L"\r\n\r\n确认继续？";
            if (MessageBoxW(hWnd, confirm.c_str(), L"确认还原", MB_YESNO | MB_ICONQUESTION) != IDYES) {
                return 0;
            }
            std::wstring errMsg;
            if (!SetMyComputerName(g_backupName, errMsg)) {
                MessageBoxW(hWnd, errMsg.c_str(), L"还原失败", MB_OK | MB_ICONERROR);
                return 0;
            }
            g_currentName = g_backupName;
            SetEditText(g_hEditNew, L"");
            SetWindowTextW(g_hStaticHint, L"");
            RefreshStatus();
            MessageBoxW(hWnd,
                L"计算机名已成功还原。\r\n\r\n"
                L"注意：需要注销或重启 Windows 后新名称才会完全生效。",
                L"还原成功", MB_OK | MB_ICONINFORMATION);
            return 0;
        }
        case IDC_BTN_SAVE: {
            // 保存当前计算机名到注册表作为备份
            if (g_currentName.empty()) {
                MessageBoxW(hWnd, L"无法获取当前计算机名", L"保存失败", MB_OK | MB_ICONERROR);
                return 0;
            }
            if (SaveBackupName(g_currentName)) {
                g_backupName = g_currentName;
                SetWindowTextW(g_hEditBackup, g_backupName.c_str());
                RefreshStatus();
                MessageBoxW(hWnd,
                    (L"已将当前计算机名 \"" + g_currentName + L"\" 保存为备份名称。").c_str(),
                    L"保存成功", MB_OK | MB_ICONINFORMATION);
            } else {
                MessageBoxW(hWnd, L"保存到注册表失败，请检查权限", L"保存失败", MB_OK | MB_ICONERROR);
            }
            return 0;
        }
        case IDC_BTN_REGEDIT: {
            // 打开注册表编辑器并自动定位到 HKLM\SOFTWARE\RenameComputer
            // 原理：设置 HKCU\...\Regedit\LastKey，regedit 启动时会自动跳到该键

            const wchar_t* targetKey = L"计算机\\HKEY_LOCAL_MACHINE\\SOFTWARE\\RenameComputer";

            // 写入 LastKey
            HKEY hKey;
            if (RegCreateKeyExW(HKEY_CURRENT_USER,
                L"Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit",
                0, nullptr, 0, KEY_WRITE, nullptr, &hKey, nullptr) == ERROR_SUCCESS) {
                DWORD size = (DWORD)((wcslen(targetKey) + 1) * sizeof(wchar_t));
                RegSetValueExW(hKey, L"LastKey", 0, REG_SZ, (const BYTE*)targetKey, size);
                RegCloseKey(hKey);
            }

            // 启动 regedit，会自动定位到 LastKey
            HINSTANCE hRet = ShellExecuteW(hWnd, L"open", L"regedit.exe", L"-m", nullptr, SW_SHOWNORMAL);
            if ((INT_PTR)hRet <= 32) {
                // -m 参数失败，尝试不带参数
                hRet = ShellExecuteW(hWnd, L"open", L"regedit.exe", nullptr, nullptr, SW_SHOWNORMAL);
                if ((INT_PTR)hRet <= 32) {
                    MessageBoxW(hWnd, L"无法打开注册表编辑器", L"错误", MB_OK | MB_ICONERROR);
                    return 0;
                }
            }
            return 0;
        }
        case IDC_BTN_REFRESH: {
            // 禁用按钮，防止重复点击
            EnableWindow(g_hBtnRefresh, FALSE);
            SetWindowTextW(g_hBtnRefresh, L"正在刷新...");
            // 异步执行，避免卡 UI
            std::thread([]() {
                // 增强的网络刷新命令
                const wchar_t* cmds[] = {
                    L"nbtstat -R",        // 清除 NetBIOS 名称缓存
                    L"nbtstat -RR",       // 释放并刷新 NetBIOS 名称
                    L"ipconfig /flushdns", // 清除 DNS 缓存
                    L"ipconfig /renew",   // 重新获取 IP
                    // 重启网络相关服务，让新计算机名在网络中发现
                    L"net stop fdPHost",
                    L"net stop FDResPub",
                    L"net stop LanmanServer",
                    L"net start LanmanServer",
                    L"net start fdPHost",
                    L"net start FDResPub",
                };
                std::wstring result;
                int cmdCount = sizeof(cmds) / sizeof(cmds[0]);
                for (int i = 0; i < cmdCount; ++i) {
                    STARTUPINFOW si = {sizeof(si)};
                    si.dwFlags = STARTF_USESHOWWINDOW;
                    si.wShowWindow = SW_HIDE;
                    PROCESS_INFORMATION pi = {0};
                    wchar_t cmd[256] = {0};
                    wcscpy_s(cmd, cmds[i]);
                    if (CreateProcessW(nullptr, cmd, nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
                        WaitForSingleObject(pi.hProcess, 15000);
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                        result += std::wstring(cmds[i]) + L"  OK\r\n";
                    } else {
                        result += std::wstring(cmds[i]) + L"  FAILED\r\n";
                    }
                }
                result += L"\r\n网络名称已刷新。\r\n如果网络中仍显示旧名称，请重启电脑。";
                // 发消息回主线程
                wchar_t* pResult = _wcsdup(result.c_str());
                PostMessageW(g_hMainWnd, WM_REFRESH_DONE, 0, (LPARAM)pResult);
            }).detach();
            return 0;
        }
        case IDC_BTN_LOGOFF: {
            if (MessageBoxW(hWnd,
                L"注销将关闭所有正在运行的程序。\r\n\r\n确认注销？",
                L"确认注销", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
                return 0;
            }
            // ExitWindowsEx(EWX_LOGOFF, 0)
            ExitWindowsEx(EWX_LOGOFF, 0);
            return 0;
        }
        case IDC_BTN_REBOOT: {
            if (MessageBoxW(hWnd,
                L"重启将关闭所有正在运行的程序。\r\n\r\n确认重启？",
                L"确认重启", MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES) {
                return 0;
            }
            // 需要提权才能重启
            HANDLE hToken = nullptr;
            if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) {
                TOKEN_PRIVILEGES tp = {0};
                LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &tp.Privileges[0].Luid);
                tp.PrivilegeCount = 1;
                tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
                AdjustTokenPrivileges(hToken, FALSE, &tp, 0, nullptr, nullptr);
                CloseHandle(hToken);
            }
            ExitWindowsEx(EWX_REBOOT, 0);
            return 0;
        }
        }
        return 0;
    }
    case WM_REFRESH_DONE: {
        wchar_t* pResult = (wchar_t*)lParam;
        // 恢复按钮
        EnableWindow(g_hBtnRefresh, TRUE);
        SetWindowTextW(g_hBtnRefresh, L"刷新网络名称（让局域网立即看到新名）");
        if (pResult) {
            std::wstring msg = L"已执行网络名称刷新：\r\n\r\n" + std::wstring(pResult) +
                L"\r\n局域网其他机器现在可以看到新名称。\r\n"
                L"本机环境变量等仍需注销/重启才会更新。";
            MessageBoxW(hWnd, msg.c_str(), L"刷新完成", MB_OK | MB_ICONINFORMATION);
            free(pResult);
        }
        return 0;
    }
    case WM_CTLCOLORSTATIC: {
        HDC hdc = (HDC)wParam;
        HWND hCtrl = (HWND)lParam;
        SetBkMode(hdc, TRANSPARENT);
        // 标题用白色，其他用浅灰
        wchar_t text[256] = {0};
        GetWindowTextW(hCtrl, text, 256);
        if (hCtrl == g_hStaticHint) {
            // 提示文字：根据内容判断颜色
            if (wcsstr(text, L"✓")) {
                SetTextColor(hdc, COLOR_HINT_OK);
            } else if (wcsstr(text, L"✗")) {
                SetTextColor(hdc, COLOR_HINT_ERR);
            } else {
                SetTextColor(hdc, COLOR_TEXT_DIM);
            }
        } else if (hCtrl == g_hStaticStatus) {
            if (wcsstr(text, L"●")) {
                SetTextColor(hdc, COLOR_HINT_OK);
            } else {
                SetTextColor(hdc, COLOR_TEXT_DIM);
            }
        } else if (text[0] == L'修' && text[1] == L'改') {
            // 标题
            SetTextColor(hdc, RGB(255, 255, 255));
        } else {
            SetTextColor(hdc, COLOR_TEXT_DIM);
        }
        return (LRESULT)g_hbrBg;
    }
    case WM_CTLCOLOREDIT: {
        HDC hdc = (HDC)wParam;
        SetBkMode(hdc, TRANSPARENT);
        SetTextColor(hdc, COLOR_TEXT);
        SetBkColor(hdc, COLOR_EDIT_BG);
        return (LRESULT)g_hbrEditBg;
    }
    case WM_ERASEBKGND: {
        HDC hdc = (HDC)wParam;
        RECT rc;
        GetClientRect(hWnd, &rc);
        FillRect(hdc, &rc, g_hbrBg);
        return 1;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(hWnd, msg, wParam, lParam);
    }
}

// ==================== 入口 ====================
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR, int) {
    g_hInst = hInstance;

    // 必须管理员权限运行
    if (!IsAdmin()) {
        MessageBoxW(nullptr,
            L"启动失败，请以管理员身份运行本程序。\r\n\r\n"
            L"右键点击 RenameComputer.exe，选择「以管理员身份运行」。",
            L"需要管理员权限",
            MB_OK | MB_ICONERROR);
        return 0;
    }

    const wchar_t* CLASS_NAME = L"RenameComputerWndClass";
    WNDCLASSEXW wcx = {0};
    wcx.cbSize = sizeof(wcx);
    wcx.lpfnWndProc = MainWndProc;
    wcx.hInstance = hInstance;
    wcx.lpszClassName = CLASS_NAME;
    wcx.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcx.hbrBackground = CreateSolidBrush(COLOR_BG);
    wcx.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
    RegisterClassExW(&wcx);

    HWND hWnd = CreateWindowExW(
        0, CLASS_NAME, L"修改电脑设备名",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 540, 520,
        nullptr, nullptr, hInstance, nullptr
    );
    if (!hWnd) return 0;

    g_hMainWnd = hWnd;

    RECT rc;
    GetWindowRect(hWnd, &rc);
    int sw = GetSystemMetrics(SM_CXSCREEN);
    int sh = GetSystemMetrics(SM_CYSCREEN);
    int px = (sw - (rc.right - rc.left)) / 2;
    int py = (sh - (rc.bottom - rc.top)) / 2;
    SetWindowPos(hWnd, nullptr, px, py, 0, 0, SWP_NOSIZE | SWP_NOZORDER);

    ShowWindow(hWnd, SW_SHOWNORMAL);
    UpdateWindow(hWnd);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        if (!IsDialogMessageW(hWnd, &msg)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
    }

    if (g_hFontTitle) DeleteObject(g_hFontTitle);
    if (g_hFontNormal) DeleteObject(g_hFontNormal);
    if (g_hFontSmall) DeleteObject(g_hFontSmall);

    return (int)msg.wParam;
}
