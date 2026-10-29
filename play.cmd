@echo off
cd /d "%~dp0"
if exist "bin\Release\prototype.exe" (
    start "" "bin\Release\prototype.exe"
    exit /b
)
if exist "bin\Debug\prototype.exe" (
    start "" "bin\Debug\prototype.exe"
    exit /b
)
echo Open prototype.sln in Visual Studio and build x64 first.
pause