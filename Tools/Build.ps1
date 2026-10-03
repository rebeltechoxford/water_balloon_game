param([ValidateSet('Editor','Game','Solution')][string]$Target='Editor', [string]$Engine='C:\Program Files\Epic Games\UE_5.8')
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$scratch=Join-Path $project 'Saved/BuildWorkspace'
New-Item -ItemType Directory -Force -Path $scratch | Out-Null
$env:DOTNET_CLI_HOME=Join-Path $scratch 'DotNet'
$env:DOTNET_CLI_TELEMETRY_OPTOUT='1'
$env:UBA_ROOT=Join-Path $scratch 'UBA'
$ubt=Join-Path $Engine 'Engine/Binaries/DotNET/UnrealBuildTool/UnrealBuildTool.dll'
$dotnet=Join-Path $Engine 'Engine/Binaries/ThirdParty/DotNet/10.0/win-x64/dotnet.exe'
$argsList=@("-Project=$project/Clanka.uproject","-Log=$scratch/Build-$Target.log",'-NoUBA','-NoHotReloadFromIDE','-WaitMutex')
if($Target -eq 'Solution') { $argsList=@('-ProjectFiles','-Game','-Engine','-2026')+$argsList }
else { $name=if($Target -eq 'Editor'){'ClankaEditor'}else{'Clanka'}; $argsList=@($name,'Win64','Development')+$argsList }
& $dotnet $ubt @argsList
exit $LASTEXITCODE
