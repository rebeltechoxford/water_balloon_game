@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (echo C++ build tools were not found. & exit /b 1)
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist "WindowsDemo" mkdir "WindowsDemo"
if not exist "Saved\PreviewBuild" mkdir "Saved\PreviewBuild"
cl /nologo /std:c++17 /EHsc /W4 /O2 /MT /DUNICODE /D_UNICODE Preview\Preview.cpp /Fo:Saved\PreviewBuild\Preview.obj /Fe:WindowsDemo\ClankaSplashLab.exe /link /SUBSYSTEM:WINDOWS user32.lib gdi32.lib shell32.lib
set "BUILDRESULT=%ERRORLEVEL%"
popd
exit /b %BUILDRESULT%
