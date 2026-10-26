# Core-only MSVC memory checks; never opens an audio device or installs software.
param([string]$PythonExecutable='', [switch]$EnableSciPy,
      [ValidateRange(1,64)][int]$Parallel=3)
$ErrorActionPreference='Stop'
Set-StrictMode -Version Latest
$root=(Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$cmakeBin=Join-Path $root '.tools\python\cmake\data\bin'
if(-not(Get-Command cmake -ErrorAction SilentlyContinue) -and (Test-Path (Join-Path $cmakeBin 'cmake.exe'))) {
  $env:PATH="$cmakeBin;$env:PATH"
}
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if(-not(Test-Path $vswhere)){throw 'Visual Studio Installer/vswhere not found'}
$vsPath=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.ASAN -property installationPath
if(-not $vsPath){throw 'Install the MSVC AddressSanitizer component in Visual Studio Installer first'}
$toolVersion=Get-ChildItem (Join-Path $vsPath 'VC\Tools\MSVC') -Directory |
  Sort-Object { [version]$_.Name } -Descending | Select-Object -First 1
$asanBin=Join-Path $toolVersion.FullName 'bin\Hostx64\x64'
if(-not(Test-Path (Join-Path $asanBin 'clang_rt.asan_dynamic-x86_64.dll'))) {
  throw 'MSVC x64 AddressSanitizer runtime missing'
}
$env:PATH="$asanBin;$env:PATH"
$env:MSBUILDDISABLENODEREUSE='1'
$build=Join-Path $root 'build-asan-core'
$evidence=Join-Path $root 'artifacts\windows-v07'
New-Item -ItemType Directory -Force $evidence | Out-Null
$pythonArgs=@()
if($PythonExecutable){$pythonArgs+="-DPython3_EXECUTABLE=$PythonExecutable"}
$scipy=if($EnableSciPy){'ON'}else{'OFF'}
$qaModules=Join-Path $root '.tools\python'
if($EnableSciPy -and (Test-Path (Join-Path $qaModules 'scipy'))) {
  $env:PYTHONPATH="$qaModules;$env:PYTHONPATH"
}
& cmake -S $root -B $build -G 'Visual Studio 17 2022' -A x64 -DLAH_BUILD_GUI=OFF `
  "-DLAH_ENABLE_SCIPY_TESTS=$scipy" '-DCMAKE_CXX_FLAGS=/fsanitize=address /EHsc' `
  -DCMAKE_TRY_COMPILE_CONFIGURATION=RelWithDebInfo `
  '-DCMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO=/INCREMENTAL:NO /DEBUG' @pythonArgs
if($LASTEXITCODE -ne 0){throw 'ASan configure failed'}
& cmake --build $build --config RelWithDebInfo --parallel $Parallel 2>&1 | Tee-Object (Join-Path $evidence 'asan-build.log')
if($LASTEXITCODE -ne 0){throw 'ASan build failed'}
& ctest --test-dir $build -C RelWithDebInfo --output-on-failure 2>&1 | Tee-Object (Join-Path $evidence 'asan-ctest.log')
if($LASTEXITCODE -ne 0){throw 'ASan tests failed'}
