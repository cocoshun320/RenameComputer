@echo off
chcp 65001 >nul
echo ============================================
echo  修改电脑设备名 - 编译脚本
echo ============================================
echo.

REM 优先尝试 MSVC (cl.exe)
where cl.exe >nul 2>&1
if %errorlevel%==0 (
    echo [使用 MSVC 编译]
    cl /O2 /EHsc /DUNICODE /D_UNICODE /utf-8 main.cpp /link /OUT:RenameComputer.exe ^
       shlwapi.lib shell32.lib advapi32.lib comctl32.lib user32.lib gdi32.lib kernel32.lib
    if exist RenameComputer.exe (
        echo.
        echo 编译成功：RenameComputer.exe
        echo 可以直接双击运行。
    ) else (
        echo 编译失败，请检查错误信息。
    )
    goto :end
)

REM 尝试 MinGW-w64 (g++)
where g++.exe >nul 2>&1
if %errorlevel%==0 (
    echo [使用 MinGW-w64 编译]
    g++ -O2 -std=c++17 -municode -o RenameComputer.exe main.cpp ^
        -lshlwapi -lshell32 -ladvapi32 -lkernel32 -lgdi32 -luser32 -lcomctl32
    if exist RenameComputer.exe (
        echo.
        echo 编译成功：RenameComputer.exe
        echo 可以直接双击运行。
    ) else (
        echo 编译失败，请检查错误信息。
    )
    goto :end
)

echo [错误] 未找到编译器。
echo 请安装以下任一工具：
echo   - Visual Studio (含 C++ 桌面开发) 并在「开发者命令提示」中运行本脚本
echo   - MinGW-w64 (g++) 并将其加入 PATH
echo.

:end
pause
