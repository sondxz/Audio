# Run inside "Developer PowerShell for VS 2022/2026" with MSVC Build Tools installed.
# Example: .\scripts\build-windows.ps1 -JuceRoot "C:\SDK\JUCE"
param(
    [string]$JuceRoot = "C:\SDK\JUCE",
    [switch]$FetchJuce,
    [switch]$DisableAsio
)
$ErrorActionPreference = "Stop"
$root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$build = Join-Path $root "build"

if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
    throw "Khong tim thay CMake. Cai CMake va mo lai Developer PowerShell."
}
if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "Khong tim thay cl.exe. Mo Developer PowerShell cua Visual Studio Build Tools voi workload C++."
}
if (-not $FetchJuce -and -not (Test-Path (Join-Path $JuceRoot 'CMakeLists.txt'))) {
    throw "Khong tim thay JUCE/CMakeLists.txt tai $JuceRoot. Chi dinh -JuceRoot hoac -FetchJuce."
}
$asio = if ($DisableAsio) { "OFF" } else { "ON" }
$fetch = if ($FetchJuce) { "ON" } else { "OFF" }
Write-Host "[1/4] CMake configure"
& cmake -S $root -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DJUCE_ROOT=$JuceRoot" "-DLAH_FETCH_JUCE=$fetch" "-DLAH_ENABLE_ASIO=$asio"
if ($LASTEXITCODE -ne 0) { throw "CMake configure failed" }
Write-Host "[2/4] Build"
& cmake --build $build --parallel
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
Write-Host "[3/4] Automated tests"
& ctest --test-dir $build --output-on-failure
if ($LASTEXITCODE -ne 0) { throw "Tests failed" }
Write-Host "[4/4] Locate EXE"
$executable = Get-ChildItem -Path $build -Filter "LiveAudioHost.exe" -File -Recurse |
    Where-Object { $_.FullName -notmatch "[\\/]CMakeFiles[\\/]" } |
    Select-Object -First 1
if (-not $executable) { throw "Build thanh cong nhung khong tim thay LiveAudioHost.exe" }
Write-Host "Thanh cong: $($executable.FullName)" -ForegroundColor Green
