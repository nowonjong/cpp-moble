@echo off
cd /d "%~dp0"
if exist "bin\Console\Release\wordchain_console.exe" (
    "bin\Console\Release\wordchain_console.exe"
) else (
    echo Build wordchain_console.vcxproj with Visual Studio x64 Release first.
)
pause
