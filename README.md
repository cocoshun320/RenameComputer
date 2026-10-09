# 修改电脑设备名 (RenameComputer)

一个用 C++ (Win32 API) 编写的 Windows GUI 程序，用于修改本机的计算机名（如 `DESKTOP-XXXXXXX` → 自定义名称），并支持一键还原。

## 截图

程序采用深色主题 UI，自绘彩色按钮：

- 蓝色按钮 — 修改计算机名
- 绿色按钮 — 还原为备份名称
- 灰色按钮 — 刷新网络名称
- 橙色按钮 — 注销 / 重启 Windows

## 功能特性

### 核心功能
- **显示当前计算机名** — 只读输入框，支持右键复制 / Ctrl+C
- **显示已备份的原名称** — 用于还原参考，同样可复制
- **输入新名称** — 实时校验合法性，显示字符计数（如 `8/15 字符`）
- **修改计算机名** — 写入 4 处注册表位置，确保改名彻底
- **还原为备份名称** — 一键恢复到 OEM 默认名

### 自动备份
- 启动时若检测到当前名是 OEM 默认格式（`DESKTOP-XXXXX`、`LAPTOP-XXXXX` 等），自动备份到注册表
- 备份位置：`HKLM\SOFTWARE\RenameComputer\OriginalName`
- 单个 exe 即可还原，无需额外文件

### 网络刷新
- 点击「刷新网络名称」会异步执行 `nbtstat -R`、`nbtstat -RR`、`ipconfig /renew`
- 让局域网内其他设备立即看到新计算机名（不卡 UI，后台线程执行）

### 注销 / 重启
- 「注销 Windows」— 注销当前用户，重新登录后新名完全生效
- 「重启电脑」— 重启系统，新名完全生效

### 权限要求
- 程序**必须以管理员权限运行**
- 普通模式启动会弹出错误提示并退出
- 请右键 → 「以管理员身份运行」

## 名称规则（Windows 限制）

| 规则 | 说明 |
|------|------|
| 允许字符 | ASCII 字母 `A-Z` `a-z`、数字 `0-9`、连字符 `-` |
| 禁止字符 | 中文、空格、下划线、特殊符号 |
| 长度 | 1 ~ 15 个字符 |
| 不能全为数字 | 至少包含一个字母或连字符 |
| 不能以连字符开头或结尾 | 如 `-abc` 或 `abc-` 非法 |

输入框下方会实时显示校验结果：
- 合法：`✓ 名称合法（8/15 字符）`（绿色）
- 非法：`✗ 已输入 18 个字符，超过 15 上限`（红色）

## 关于「不重启就生效」

### 注册表层（立即生效）
程序通过 `SetComputerNameExW` API + 手动写入 4 处注册表位置：

1. `HKLM\SYSTEM\CurrentControlSet\Control\ComputerName\ComputerName\ComputerName`
2. `HKLM\SYSTEM\CurrentControlSet\Control\ComputerName\ActiveComputerName\ComputerName`
3. `HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\Hostname`
4. `HKLM\SYSTEM\CurrentControlSet\Services\Tcpip\Parameters\NV Hostname`

注册表中的名字**立即更新**，无需重启。

### 网络层（刷新后生效）
点击「刷新网络名称」后，局域网内其他设备可以通过 `nbtstat` / ping 看到新名字。

### 本机会话层（需注销/重启）
当前已登录会话的**环境变量**（`COMPUTERNAME`、`USERDOMAIN` 等）和部分系统显示名称，**必须注销或重启** 才会更新。

这是 Windows 系统的硬性限制，任何程序都无法绕过。

## 系统兼容性

| 系统 | 状态 | 说明 |
|------|------|------|
| Windows 10 | ✅ 已测试 | 在 Win10 22H2 上开发并测试通过 |
| Windows 11 | ⚠️ 未测试 | 理论兼容（使用标准 Win32 API），但未实际验证 |

> **注意**：本程序在 Windows 10 上开发并测试通过。Windows 11 未进行实际测试，但程序仅使用标准 Win32 API 和注册表操作，理论上应该兼容。如在 Win11 上遇到问题，欢迎反馈。

## 编译

### 前置条件
- Visual Studio 2022（或仅安装 Build Tools）
- 需要包含 C++ 桌面开发组件

### 方式一：使用编译脚本

```bat
:: 在 "x64 Native Tools Command Prompt for VS 2022" 中运行
rc /utf-8 app.rc
cl /O2 /EHsc /DUNICODE /D_UNICODE /utf-8 main.cpp app.res /link /OUT:RenameComputer.exe ^
   shlwapi.lib shell32.lib advapi32.lib comctl32.lib user32.lib gdi32.lib kernel32.lib
```

### 方式二：MinGW-w64（不推荐，可能缺图标）

```bash
g++ -O2 -std=c++17 -municode -o RenameComputer.exe main.cpp app.rc \
    -lshlwapi -lshell32 -ladvapi32 -lkernel32 -lgdi32 -luser32 -lcomctl32
```

### 编译参数说明

| 参数 | 作用 |
|------|------|
| `/O2` | 优化级别 2 |
| `/EHsc` | C++ 异常处理 |
| `/DUNICODE /D_UNICODE` | 使用 Unicode 宽字符 API |
| `/utf-8` | 源文件以 UTF-8 编码（含中文字符串） |
| `app.res` | 资源文件（含图标） |

## 使用方法

1. **以管理员身份运行** `RenameComputer.exe`
2. 程序自动显示当前计算机名和已备份的原名称
3. 在「输入新的计算机名」框中输入新名称
4. 下方提示显示校验结果（绿色合法 / 红色非法）
5. 点击蓝色「修改计算机名」按钮
6. 根据需要：
   - 点击「刷新网络名称」让局域网立即看到新名
   - 点击「注销 Windows」或「重启电脑」让本机完全生效
7. 如需恢复原名称，点击绿色「还原为备份名称」按钮

## 文件说明

| 文件 | 说明 |
|------|------|
| `main.cpp` | 主程序源码（C++ Win32 API） |
| `app.rc` | 资源文件（引用图标） |
| `app.ico` | 程序图标（32x32，深色主题电脑+改名标识） |
| `gen_icon.cpp` | 图标生成器源码（用于生成 app.ico） |
| `build.bat` | 自动检测编译器的编译脚本 |
| `RenameComputer.exe` | 编译后的可执行文件（约 176 KB） |

## 技术细节

### 修改计算机名的 4 处注册表位置

`SetComputerNameExW` API 只写入其中 2 处，另外 2 处需要手动写入，否则注销/重启后名字可能不生效：

| 注册表路径 | 值名 | 说明 |
|-----------|------|------|
| `Control\ComputerName\ComputerName` | `ComputerName` | 主计算机名（API 写入） |
| `Control\ComputerName\ActiveComputerName` | `ComputerName` | 活动计算机名（**手动写入**） |
| `Services\Tcpip\Parameters` | `Hostname` | TCP/IP 主机名（**手动写入**） |
| `Services\Tcpip\Parameters` | `NV Hostname` | NV 主机名（API 写入） |

### 自绘按钮实现

程序使用 `BS_OWNERDRAW` + 子类化实现彩色按钮：
- `WM_PAINT` 自绘背景和文字
- `WM_MOUSEMOVE` + `TrackMouseEvent` 实现悬停效果
- `WM_LBUTTONDOWN` / `WM_LBUTTONUP` 实现按下效果

### 异步网络刷新

使用 `std::thread` 后台执行 `nbtstat` / `ipconfig` 命令，完成后通过 `PostMessageW(WM_REFRESH_DONE)` 通知主线程更新 UI，避免界面卡顿。

## 许可

本程序可自由使用和修改。
