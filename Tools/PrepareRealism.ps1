param([string]$Engine='C:\Program Files\Epic Games\UE_5.8', [string]$DataCache='')
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
if(!(Test-Path -LiteralPath "$project/AssetsSource/manifest.json")) { throw 'Run python Tools/DownloadRealism.py --help and prepare the source kit first. Committed Content/Realism assets need no reimport.' }
if($DataCache) { [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$DataCache,'Process') }
& "$Engine/Engine/Binaries/Win64/UnrealEditor-Cmd.exe" "$project/Clanka.uproject" '-run=pythonscript' "-script=$PSScriptRoot/PrepareRealism.py" -unattended -nullrhi -nosplash "-abslog=$project/Saved/PrepareRealism.log"
exit $LASTEXITCODE
