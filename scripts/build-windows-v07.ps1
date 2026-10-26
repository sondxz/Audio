# Windows x64 PowerShell / VS Code. Reproducible source build.
# Produces LiveAudioHost.exe ONLY on the Windows host where this script runs.
# No drivers or JUCE are downloaded/installed by this script.
param(
  [string]$JuceRoot = '',
  [switch]$DisableAsio,
  [switch]$EnableSciPy,
  [string]$PythonExecutable = '',
  [string]$Generator = '',
  [ValidateRange(1,64)][int]$Parallel = 4
)
$ErrorActionPreference='Stop'
$env:MSBUILDDISABLENODEREUSE='1'
Set-StrictMode -Version Latest
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$build=Join-Path $root 'build-windows-v07'
$bundledCmake=Join-Path $root '.tools\python\cmake\data\bin'
if (-not (Get-Command cmake -ErrorAction SilentlyContinue) -and (Test-Path (Join-Path $bundledCmake 'cmake.exe'))) {
  $env:PATH="$bundledCmake;$env:PATH"
}
foreach($item in @('cmake','ctest')) {
  if (-not (Get-Command $item -ErrorAction SilentlyContinue)) {
    throw "Missing $item. Install CMake 3.22+ or provide the local .tools CMake bundle."
  }
}
if (-not $JuceRoot) {
  $localJuce=Join-Path $root '.tools\JUCE-8.0.15'
  $JuceRoot=if(Test-Path (Join-Path $localJuce 'CMakeLists.txt')){$localJuce}else{'C:\SDK\JUCE'}
}
if (-not $Generator) {
  $Generator=if((Get-Command cl.exe -ErrorAction SilentlyContinue) -and
                (Get-Command ninja -ErrorAction SilentlyContinue)){'Ninja'}else{'Visual Studio 17 2022'}
}
if (-not (Test-Path (Join-Path $JuceRoot 'modules\juce_audio_devices'))) {
  throw "JUCE root missing: $JuceRoot. Download/extract a supported JUCE 8 release separately."
}
$asio='ON';if($DisableAsio){$asio='OFF'}
$scipy='OFF';if($EnableSciPy){$scipy='ON'}
$qaModules=Join-Path $root '.tools\python'
if($EnableSciPy -and (Test-Path (Join-Path $qaModules 'scipy'))) {
  $env:PYTHONPATH="$qaModules;$env:PYTHONPATH"
}
$generatorArgs=@('-G',$Generator)
$buildTypeArgs=@()
if($Generator -like 'Visual Studio*'){$generatorArgs+=@('-A','x64')}else{$buildTypeArgs+= '-DCMAKE_BUILD_TYPE=Release'}
$pythonArgs=@()
if($PythonExecutable){$pythonArgs+= "-DPython3_EXECUTABLE=$PythonExecutable"}
$evidence=Join-Path $root 'artifacts\windows-v07'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
& cmake -S $root -B $build @generatorArgs @buildTypeArgs "-DJUCE_ROOT=$JuceRoot" "-DLAH_ENABLE_ASIO=$asio" -DLAH_BUILD_GUI=ON "-DLAH_ENABLE_SCIPY_TESTS=$scipy" @pythonArgs 2>&1 | Tee-Object (Join-Path $evidence 'configure.log')
if($LASTEXITCODE -ne 0){throw 'Configuration FAIL'}
& cmake --build $build --config Release --parallel $Parallel 2>&1 | Tee-Object (Join-Path $evidence 'build.log')
if($LASTEXITCODE -ne 0){throw 'Build FAIL'}
& ctest --test-dir $build -C Release --output-on-failure 2>&1 | Tee-Object (Join-Path $evidence 'ctest.log')
if($LASTEXITCODE -ne 0){throw 'Core tests FAIL. EXE cannot be packaged as test-verified.'}
$exe=Get-ChildItem -Path $build -Filter LiveAudioHost.exe -File -Recurse |
  Where-Object {$_.FullName -notmatch '[\\/]CMakeFiles[\\/]'} | Select-Object -First 1
if(-not $exe){throw 'GUI binary not found despite build success'}
$benchmark=Join-Path $build 'Release\lah_offline_benchmark.exe'
if(-not (Test-Path $benchmark)){$benchmark=Join-Path $build 'lah_offline_benchmark.exe'}
& $benchmark 2>&1 | Tee-Object (Join-Path $evidence 'benchmark.log')
if($LASTEXITCODE -ne 0){throw 'Offline benchmark execution failed'}
$nativeBenchmark=Join-Path $build 'Release\lah_builtin_benchmark.exe'
if(-not (Test-Path $nativeBenchmark)){$nativeBenchmark=Join-Path $build 'lah_builtin_benchmark.exe'}
& $nativeBenchmark 2>&1 | Tee-Object (Join-Path $evidence 'native-benchmark.log')
if($LASTEXITCODE -ne 0){throw 'Native rack benchmark execution failed'}
$release=Join-Path $root 'release-v07'
New-Item -ItemType Directory -Force -Path $release | Out-Null
Copy-Item $exe.FullName (Join-Path $release 'LiveAudioHost.exe') -Force
Copy-Item (Join-Path $root 'docs\M32R_USB_INSERT_GUIDE.md') (Join-Path $release 'READ_ME_FIRST.md') -Force
Copy-Item (Join-Path $root 'docs\WINDOWS_DEVELOPMENT_REPORT_v0.7.0.md') $release -Force
foreach($report in @('UI_AND_BUILTIN_RELEASE_REPORT_v0.7.0.md',
                    'M32R_USB_INSERT_GUIDE.md',
                    'M32R_USB_IMPLEMENTATION_REPORT.md',
                    'PLUGIN_EDITORS_GUIDE.md',
                    'VIDEO_ANALYSIS.md',
                    'DEVELOPMENT_PLAN.md',
                    'FEATURE_MATRIX.md',
                    'TEST_MATRIX.md',
                    'APP_UPGRADE_20261026.md',
                    'M32_CONNECTION_CHECK_REPORT.md',
                    'AI_FEEDBACK_IMPLEMENTATION_v0.7.0.md',
                    'FEEDBACK_MODEL_CARD.md',
                    'BUILTIN_PROCESSOR_VALIDATION.md',
                    'FEATURE_STATUS_AND_ROADMAP_v0.7.0.md')) {
  Copy-Item (Join-Path $root "docs\$report") $release -Force
}
$releaseEvidence=Join-Path $release 'evidence'
New-Item -ItemType Directory -Force -Path $releaseEvidence | Out-Null
foreach($log in @('configure.log','build.log','ctest.log','benchmark.log','native-benchmark.log')) {
  Copy-Item (Join-Path $evidence $log) $releaseEvidence -Force
}
Copy-Item (Join-Path $build 'ui-smoke\smoke-report.txt') $releaseEvidence -Force
Copy-Item (Join-Path $build 'plugin-audio-audit\report.txt') (Join-Path $releaseEvidence 'plugin-audio-report.txt') -Force
Copy-Item (Join-Path $build 'plugin-audio-audit\plugin-audio-audit.csv') $releaseEvidence -Force
$hash=Get-FileHash (Join-Path $release 'LiveAudioHost.exe') -Algorithm SHA256
"$($hash.Hash)  LiveAudioHost.exe" | Set-Content -Encoding UTF8 (Join-Path $release 'SHA256.txt')
Write-Host "EXE BUILT LOCALLY: $(Join-Path $release 'LiveAudioHost.exe')" -ForegroundColor Green
Write-Host 'NOT HARDWARE-CERTIFIED. No M32R, microphone calibration or VST QA verified by this script.' -ForegroundColor Yellow
