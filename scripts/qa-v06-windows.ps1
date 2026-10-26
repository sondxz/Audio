# Run ALL runnable pure C++ automated suites on Windows. Does not modify drivers.
param([switch]$EnableSciPy)
$ErrorActionPreference='Stop'
$root=Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
  cmake -S . -B build-core -DLAH_BUILD_GUI=OFF
  if ($LASTEXITCODE -ne 0) { throw 'Configure failed' }
  cmake --build build-core --config Release
  if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
  ctest --test-dir build-core --output-on-failure -C Release
  if ($LASTEXITCODE -ne 0) { throw 'One or more core tests failed' }
  $bench=Join-Path $root 'build-core\Release\lah_offline_benchmark.exe'
  if (-not(Test-Path $bench)) {$bench=Join-Path $root 'build-core\lah_offline_benchmark.exe'}
  if (Test-Path $bench) { & $bench }
  if ($EnableSciPy) {
    $tf=Join-Path $root 'build-core\Release\lah_tf_cli.exe'
    $fir=Join-Path $root 'build-core\Release\lah_fir_cli.exe'
    if (-not(Test-Path $tf)) {$tf=Join-Path $root 'build-core\lah_tf_cli.exe'}
    if (-not(Test-Path $fir)) {$fir=Join-Path $root 'build-core\lah_fir_cli.exe'}
    python tools/independent_reference_check.py $tf
    if ($LASTEXITCODE -ne 0) {throw 'SciPy TF validation failed'}
    python tools/independent_fir_check.py $fir
    if ($LASTEXITCODE -ne 0) {throw 'SciPy FIR validation failed'}
  }
  Write-Host 'Offline QA finished. Hardware ASIO/SPL validation is STILL required.'
}
finally {Pop-Location}
