@echo off
setlocal
set "GAME=%~dp0PackagedUnreal\Windows\Clanka.exe"
if not exist "%GAME%" (
  echo The Unreal Windows package is not present yet.
  echo Build it with Tools\PackageUnreal.ps1 or open Clanka.uproject in Unreal.
  pause
  exit /b 1
)
start "" "%GAME%" -fullscreen -ResX=1920 -ResY=1080
