@echo off
taskkill /f /im Telegram.exe
call "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 -vcvars_ver=14.44.35207 >nul
if errorlevel 1 (
    echo VsDevCmd failed
    exit /b 1
)
echo Using MSVC at: %VCToolsInstallDir%
cd /d E:\GitRepo\tdesktop
cmake --build out --config Debug --target Telegram
