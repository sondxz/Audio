# Non-invasive device/driver inventory. No audio playback, no driver installation.
$ErrorActionPreference='Stop'
$target=Join-Path $PSScriptRoot '..\hardware-inventory.txt'
$os=Get-CimInstance Win32_OperatingSystem | Select-Object Caption,Version,OSArchitecture
$devices=Get-CimInstance Win32_PnPSignedDriver |
   Where-Object {$_.DeviceName -match '(?i)MIDAS|M32|DN32|USB.Audio|ASIO|M-Audio'} |
   Select-Object DeviceName,Manufacturer,DriverVersion,DriverDate,IsSigned
"INVENTORY ONLY (not JUCE/ASIO stream validation): $(Get-Date -Format o)" | Out-File $target
$os | Format-List | Out-String | Add-Content $target
$devices | Format-Table -AutoSize | Out-String | Add-Content $target
$asioRegistry=foreach($registryPath in @('HKLM:\SOFTWARE\ASIO','HKLM:\SOFTWARE\WOW6432Node\ASIO')) {
  Get-ChildItem -LiteralPath $registryPath -ErrorAction SilentlyContinue | ForEach-Object {
    [pscustomobject]@{Registry=$registryPath;Driver=$_.PSChildName}
  }
}
'REGISTERED ASIO DRIVERS (registration does not establish device connectivity):' | Add-Content $target
$asioRegistry | Format-Table -AutoSize | Out-String | Add-Content $target
Write-Host "Hardware inventory at: $target" -ForegroundColor Green
Write-Host 'Check official Midas driver compatibility separately. Presence in Windows does not prove ASIO worked.'
