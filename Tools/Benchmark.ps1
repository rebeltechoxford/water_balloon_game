param([ValidateRange(0,2)][int]$Quality=1, [ValidateRange(60,180)][int]$Seconds=65,
      [int]$Width=1920, [int]$Height=1080)
$ErrorActionPreference='Stop'
$project=Split-Path $PSScriptRoot -Parent
$game=Join-Path $project 'PackagedUnreal/Windows/Clanka.exe'
if(!(Test-Path -LiteralPath $game)) { throw 'Package the Unreal game first with Tools/PackageUnreal.ps1.' }
$saved=Join-Path $project 'PackagedUnreal/Windows/Clanka/Saved'
$started=Get-Date
# A visible rendering window is required for the graphics benchmark.
$run=Start-Process -FilePath $game -WorkingDirectory (Split-Path $game) -WindowStyle Normal -PassThru -ArgumentList @('-windowed','-ForceRes',"-ResX=$Width","-ResY=$Height",'-PerfTest',"-PerfSeconds=$Seconds","-Quality=$Quality")
$run | Wait-Process -Timeout ($Seconds+180)
$report=Join-Path $saved 'Performance.json'
if(!(Test-Path -LiteralPath $report) -or (Get-Item -LiteralPath $report).LastWriteTime -lt $started) { throw 'This run did not produce a fresh performance report. Check the game log.' }
$result=Get-Content -LiteralPath $report -Raw | ConvertFrom-Json
if($result.width -ne $Width -or $result.height -ne $Height) { throw 'The game did not render at the requested benchmark resolution.' }
if($result.average_fps -le 0 -or @($result.cameras | Where-Object { $_.frames -le 0 }).Count -gt 0) { throw 'The benchmark did not sample all three cameras.' }
$destination=Join-Path $project ('Media/Benchmarks/'+$started.ToString('yyyyMMdd-HHmmss')+'-Q'+$Quality)
New-Item -ItemType Directory -Force $destination | Out-Null
foreach($name in @('Performance.json','Performance.csv','Realism-1.png','Realism-2.png','Realism-3.png')) {
    $source=Join-Path $saved $name
    if(!(Test-Path -LiteralPath $source) -or (Get-Item -LiteralPath $source).LastWriteTime -lt $started) { throw "Missing or stale benchmark output: $name" }
    Copy-Item -LiteralPath $source -Destination $destination
}
$result | Format-List
Write-Host "Full report and screenshots saved to $destination"
