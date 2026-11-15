@echo off
setlocal EnableExtensions
cd /d "%~dp0"
echo.
echo   TUMPUK - Build Windows x64
echo.
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto compile
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto no_compiler
for /f "usebackq delims=" %%V in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%V"
if not defined VSROOT goto no_compiler
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 goto failed

:compile
if not exist "build" mkdir "build"
if not exist "dist\Tumpuk" mkdir "dist\Tumpuk"
cl /nologo /std:c++17 /O2 /EHsc /MT /W4 /utf-8 /DNOMINMAX /DUNICODE /D_UNICODE ^
  /Fo"build/" /Fe"dist\Tumpuk\Tumpuk.exe" "src\app.cpp" "src\main_win32.cpp" ^
  /link /SUBSYSTEM:WINDOWS opengl32.lib gdi32.lib user32.lib
if errorlevel 1 goto failed
if not exist "dist\Tumpuk\credits.txt" copy /y "credits.txt" "dist\Tumpuk\credits.txt" >nul
if errorlevel 1 goto failed
copy /y "LICENSE-fonts.txt" "dist\Tumpuk\LICENSE-fonts.txt" >nul
if errorlevel 1 goto failed
echo.
echo Berhasil. Jalankan dist\Tumpuk\Tumpuk.exe.
echo Edit nama dan NIM di dist\Tumpuk\credits.txt.
if /i not "%~1"=="--no-pause" pause
exit /b 0

:no_compiler
echo Kompiler C++ Visual Studio tidak ditemukan.
echo Aktifkan workload Desktop development with C++ di Visual Studio Installer,
echo termasuk MSVC x64/x86 dan Windows SDK. Lalu jalankan file ini lagi.
goto failed

:failed
echo.
echo Build belum berhasil. Lihat pesan di atas.
if /i not "%~1"=="--no-pause" pause
exit /b 1
