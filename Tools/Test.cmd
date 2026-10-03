@echo off
setlocal
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSROOT=%%i"
if not defined VSROOT (echo C++ build tools were not found. & exit /b 1)
call "%VSROOT%\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
if errorlevel 1 exit /b 1
pushd "%~dp0.."
if not exist "Saved\Tests" mkdir "Saved\Tests"
cl /nologo /std:c++17 /EHsc /W4 /WX /O2 Tests\SimulationTests.cpp /Fo:Saved\Tests\SimulationTests.obj /Fe:Saved\Tests\SimulationTests.exe
if errorlevel 1 (popd & exit /b 1)
Saved\Tests\SimulationTests.exe
set "TESTRESULT=%ERRORLEVEL%"
popd
exit /b %TESTRESULT%
