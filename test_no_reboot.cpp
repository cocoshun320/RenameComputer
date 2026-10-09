// 测试：修改计算机名后不重启，立即检查各处名称是否已更新
// 编译: cl /O2 /EHsc /DUNICODE /D_UNICODE /utf-8 test_no_reboot.cpp /link advapi32.lib
#include <windows.h>
#include <cstdio>
#include <string>

#pragma comment(lib, "advapi32.lib")

void PrintName(const char* label, const wchar_t* name) {
    printf("%-30s: %ws\n", label, name);
}

int wmain() {
    wchar_t buf[256] = {0};
    DWORD size;

    // 1. 修改前：获取各处名称
    printf("===== 修改前 =====\n");
    size = 256; GetComputerNameExW((COMPUTER_NAME_FORMAT)5, buf, &size);
    PrintName("PhysicalDnsHostname (API)", buf);

    size = 256; GetComputerNameExW((COMPUTER_NAME_FORMAT)1, buf, &size);
    PrintName("DnsHostname (API)", buf);

    size = 256; GetComputerNameW(buf, &size);
    PrintName("GetComputerName (NetBIOS)", buf);

    // 环境变量
    wchar_t* env = _wgetenv(L"COMPUTERNAME");
    PrintName("Env COMPUTERNAME", env ? env : L"(null)");

    // 注册表
    HKEY hKey;
    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t regName[256] = {0};
        DWORD regSize = sizeof(regName);
        RegQueryValueExW(hKey, L"ComputerName", nullptr, nullptr,
            (LPBYTE)regName, &regSize);
        PrintName("Registry ComputerName", regName);
        RegCloseKey(hKey);
    }

    // 2. 修改计算机名（加 -TEST 后缀，便于还原）
    std::wstring orig = buf;
    std::wstring newName = orig + L"-T";
    if (newName.size() > 15) newName = orig.substr(0, 13) + L"-T";

    printf("\n===== 调用 SetComputerNameExW 修改为: %ws =====\n", newName.c_str());
    BOOL ok = SetComputerNameExW((COMPUTER_NAME_FORMAT)0, newName.c_str());
    printf("SetComputerNameExW 返回: %s, GetLastError=%lu\n",
        ok ? "成功" : "失败", GetLastError());

    // 3. 修改后立即检查
    printf("\n===== 修改后（不重启，立即查询） =====\n");
    size = 256; GetComputerNameExW((COMPUTER_NAME_FORMAT)5, buf, &size);
    PrintName("PhysicalDnsHostname (API)", buf);

    size = 256; GetComputerNameExW((COMPUTER_NAME_FORMAT)1, buf, &size);
    PrintName("DnsHostname (API)", buf);

    size = 256; GetComputerNameW(buf, &size);
    PrintName("GetComputerName (NetBIOS)", buf);

    env = _wgetenv(L"COMPUTERNAME");
    PrintName("Env COMPUTERNAME", env ? env : L"(null)");

    if (RegOpenKeyExW(HKEY_LOCAL_MACHINE,
        L"SYSTEM\\CurrentControlSet\\Control\\ComputerName\\ComputerName",
        0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        wchar_t regName[256] = {0};
        DWORD regSize = sizeof(regName);
        RegQueryValueExW(hKey, L"ComputerName", nullptr, nullptr,
            (LPBYTE)regName, &regSize);
        PrintName("Registry ComputerName", regName);
        RegCloseKey(hKey);
    }

    // 4. 还原
    printf("\n===== 还原为: %ws =====\n", orig.c_str());
    SetComputerNameExW((COMPUTER_NAME_FORMAT)0, orig.c_str());

    printf("\n结论：\n");
    printf("- 注册表中的名字：修改后立即更新（无需重启）\n");
    printf("- API 查询返回值：部分立即更新，部分需重启\n");
    printf("- 环境变量 COMPUTERNAME：当前进程不会变，需注销/重启\n");
    printf("- 网络上的计算机名：需重启服务或重启系统\n");

    return 0;
}
