# Enumerate first; optional observation opens ONLY a DN32/M32 ASIO driver and
# keeps all outputs muted. Never installs drivers, changes USB devices, saves
# application settings, plays a tone, or accepts a software licence.
param(
  [ValidateRange(1,8)][int]$Channels=8,
  [ValidateRange(1,30)][int]$Seconds=10,
  [switch]$ObserveMuted
)
$ErrorActionPreference='Stop'
$taskRoot=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$taskExe=Join-Path $taskRoot 'release-v07/LiveAudioHost.exe'
if(-not (Test-Path -LiteralPath $taskExe)){throw 'Build/package LiveAudioHost.exe first.'}
$taskEvidence=Join-Path $taskRoot 'artifacts/windows-v07'
New-Item -ItemType Directory -Path $taskEvidence -Force | Out-Null
$taskInventory=Join-Path $taskEvidence 'm32-current-audio-inventory.json'
$taskProcess=Start-Process -FilePath $taskExe -ArgumentList @('--inspect-audio',('"'+$taskInventory+'"')) -WorkingDirectory $taskRoot -WindowStyle Hidden -PassThru -Wait
if($taskProcess.ExitCode -ne 0){throw 'Audio backend enumeration failed.'}
$taskData=Get-Content -LiteralPath $taskInventory -Raw | ConvertFrom-Json
$taskAsio=@($taskData.backends | Where-Object {$_.type -eq 'ASIO'} | ForEach-Object {$_.outputs})
$taskMidas=@($taskAsio | Where-Object {$_ -match 'DN32|M32'})
& pnputil.exe /enum-devices /connected /problem | Set-Content -LiteralPath (Join-Path $taskEvidence 'm32-current-pnp-problems.txt') -Encoding UTF8
if($taskMidas.Count -eq 0){
  Write-Host 'BLOCKED: Windows/JUCE currently exposes no DN32/M32 ASIO driver.' -ForegroundColor Yellow
  Write-Host 'Test a short USB data A-B cable directly to the rear DN32-LIVE card, then install its official signed driver.'
  Write-Host "Evidence: $taskInventory"
  return
}
Write-Host ('DN32/M32 ASIO found: '+($taskMidas -join ', ')) -ForegroundColor Green
if(-not $ObserveMuted){Write-Host 'Enumeration only. Add -ObserveMuted to request a silent ASIO stream check.';return}
$taskResult=Join-Path $taskEvidence ('m32-muted-'+$Channels+'ch.json')
$taskProcess=Start-Process -FilePath $taskExe -ArgumentList @('--m32-muted-check',"$Channels","$Seconds",('"'+$taskResult+'"')) -WorkingDirectory $taskRoot -WindowStyle Hidden -PassThru -Wait
if(-not (Test-Path -LiteralPath $taskResult)){throw 'ASIO diagnostic produced no report; use the newly upgraded EXE.'}
$taskReport=Get-Content -LiteralPath $taskResult -Raw | ConvertFrom-Json
Write-Host "Muted ASIO report: $taskResult"
if($taskProcess.ExitCode -eq 0 -and $taskReport.observation_criteria_met){
  Write-Host 'PASS for muted stream criteria only. Audible return and hardware round-trip latency remain unverified.' -ForegroundColor Green
}else{
  Write-Host ('NOT ACCEPTED: '+$taskReport.error) -ForegroundColor Yellow
}
