param([string]$Engine='C:\Program Files\Epic Games\UE_5.8', [string]$DataCache='')
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
if($DataCache) { [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$DataCache,'Process') }
& "$PSScriptRoot/Build.ps1" -Target Editor -Engine $Engine
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& "$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$project/Clanka.uproject" '-run=pythonscript' "-script=$PSScriptRoot/CreateContent.py" -unattended -nullrhi -nosplash "-abslog=$project/Saved/CreateContent.log"
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
if(!(Test-Path "$project/Content/Maps/SplashLab.umap")) { throw 'Unreal did not create the playable map. Check Saved/CreateContent.log.' }
& "$PSScriptRoot/Build.ps1" -Target Solution -Engine $Engine
exit $LASTEXITCODE
