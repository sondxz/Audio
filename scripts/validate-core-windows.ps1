# Offline verification, no changes to Windows audio configuration or other applications.
param([switch]$EnableSciPyComparison)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
  cmake -S . -B build-core -DLAH_BUILD_GUI=OFF
  if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
  cmake --build build-core --config Release
  if ($LASTEXITCODE -ne 0) { throw 'CMake build failed' }
  ctest --test-dir build-core -C Release --output-on-failure
  if ($LASTEXITCODE -ne 0) { throw 'CTest failed' }
  if ($EnableSciPyComparison) {
    $candidate = Join-Path $root 'build-core\Release\lah_tf_cli.exe'
    if (-not (Test-Path $candidate)) { $candidate = Join-Path $root 'build-core\lah_tf_cli.exe' }
    if (-not (Test-Path $candidate)) { throw 'Cannot locate lah_tf_cli.exe' }
    python tools\independent_reference_check.py $candidate
    if ($LASTEXITCODE -ne 0) { throw 'Independent SciPy comparison failed' }
  }
  Write-Host 'PASS: offline C++ tests completed. This does NOT validate live Windows audio.'
}
finally { Pop-Location }
