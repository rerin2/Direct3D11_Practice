@echo off
setlocal

rem 1) Load the MSVC environment if cl.exe is not available yet.
where cl >nul 2>nul
if not errorlevel 1 goto have_cl
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL goto no_msvc
set "PATH=%PATH%;%ProgramFiles(x86)%\Microsoft Visual Studio\Installer"
call "%VSINSTALL%\VC\Auxiliary\Build\vcvars64.bat" >nul
:have_cl

rem 2) Output folder
cd /d "%~dp0"
if not exist build mkdir build

rem 3) Compile and link
cl /std:c++17 /utf-8 /W4 /EHsc /Zi /nologo /Fe:build\MiniRenderer.exe /Fo:build\ /Fd:build\ src\*.cpp user32.lib d3d11.lib d3dcompiler.lib
if errorlevel 1 goto build_failed

echo [OK] build\MiniRenderer.exe
exit /b 0

:no_msvc
echo [ERROR] MSVC Build Tools not found.
exit /b 1

:build_failed
echo [ERROR] Build failed.
exit /b 1
