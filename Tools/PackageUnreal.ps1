param([string]$Engine='C:\Program Files\Epic Games\UE_5.8', [string]$StagingDirectory='', [string]$DataCache='')
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
if(!(Test-Path "$project/Content/Maps/SplashLab.umap")) { throw 'Run Tools/PrepareUnreal.ps1 first.' }
$env:UBA_ROOT=Join-Path $project 'Saved/BuildWorkspace/UBA'
$extra=@()
if($StagingDirectory) { $extra+="-stagingdirectory=$StagingDirectory" }
if($DataCache) { [Environment]::SetEnvironmentVariable('UE-LocalDataCachePath',$DataCache,'Process') }
& "$Engine/Engine/Build/BatchFiles/RunUAT.bat" BuildCookRun "-project=$project/Clanka.uproject" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive "-archivedirectory=$project/PackagedUnreal" -unattended -utf8output @extra
exit $LASTEXITCODE
