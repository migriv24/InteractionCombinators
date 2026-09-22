# package_release.ps1 — build and package a release of Interaction Combinators:
# a Windows x64 zip (the app, the duo bench, and the DLLs they need) and the
# Android APK. Artifacts land in release/, which is not committed: they are
# uploaded to GitHub Releases.
#
#   powershell -File tools\package_release.ps1 [-SkipApk]
#
# Requires the MSYS2 UCRT64 toolchain (C:\msys64\ucrt64) for the desktop build
# and an Android SDK for the APK (see android\build_apk.ps1).
param([switch]$SkipApk)
$ErrorActionPreference = "Stop"

$root = Split-Path $PSScriptRoot -Parent
$version = (Get-Content "$root\VERSION" -Raw).Trim()
$ucrt = "C:\msys64\ucrt64\bin"
if (-not (Test-Path "$ucrt\g++.exe")) { throw "MSYS2 UCRT64 toolchain not found at $ucrt" }
$env:PATH = "$ucrt;" + $env:PATH

$out = "$root\release"
New-Item -ItemType Directory -Force $out | Out-Null

# ── desktop: an optimized build of its own (the dev build/ stays unoptimized) ──
& cmake -S $root -B "$root\build-release" -G Ninja -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE) { throw "cmake configure failed" }
& cmake --build "$root\build-release" --target interaction_combinators interaction_combinators_duo
if ($LASTEXITCODE) { throw "desktop build failed" }

$name = "InteractionCombinators-$version-windows-x64"
$stage = "$out\$name"
if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
New-Item -ItemType Directory -Force $stage | Out-Null
$bin = "$root\build-release\bin"
Copy-Item "$bin\interaction_combinators.exe", "$bin\interaction_combinators_duo.exe" $stage
# Void Core's DLL: take it from where CMake found it. (Void Maiz copies it into
# bin/ only as a post-build step of its own test target, which a release build
# of just these two targets never runs.)
$coreDll = (Select-String -Path "$root\build-release\CMakeCache.txt" -Pattern '^VOIDCORE_DLL:FILEPATH=(.+)$').Matches[0].Groups[1].Value
if (-not $coreDll -or -not (Test-Path $coreDll)) { throw "libvoidcore.dll not found (VOIDCORE_DLL=$coreDll)" }
Copy-Item $coreDll $stage
# the MinGW runtime the executables link (objdump -p lists exactly these three)
foreach ($dll in "libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll") {
    Copy-Item "$ucrt\$dll" $stage
}
@"
Interaction Combinators $version for Windows (x64)

  interaction_combinators.exe       the app
  interaction_combinators_duo.exe   two windows, one desk: collaboration on one
                                    machine (Ana hosts, Bo joins, a simulated
                                    network between them you can slow, make
                                    lossy, or cut)

Keep the DLLs beside the executables. Windows 10 or later.
Source, the mathematics, and how to build: https://github.com/migriv24/InteractionCombinators
"@ | Set-Content -Encoding utf8 "$stage\README.txt"

# prove the folder is self-contained: run the duo selftest with a PATH that has
# no toolchain on it, from the staged folder itself
$saved = $env:PATH
$env:PATH = "$env:SystemRoot\System32;$env:SystemRoot"
Push-Location $stage
& ".\interaction_combinators_duo.exe" --selftest
$selftest = $LASTEXITCODE
Pop-Location
$env:PATH = $saved
Remove-Item -Recurse -Force "$stage\duo" -ErrorAction SilentlyContinue
if ($selftest) { throw "the staged desktop build failed its selftest (exit $selftest)" }

$zip = "$out\$name.zip"
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Write-Host "desktop: $zip"

# ── Android ──
if (-not $SkipApk) {
    & powershell -NoProfile -ExecutionPolicy Bypass -File "$root\android\build_apk.ps1"
    if ($LASTEXITCODE) { throw "APK build failed" }
    Copy-Item "$root\android\interaction_combinators.apk" "$out\InteractionCombinators-$version.apk" -Force
    Write-Host "android: $out\InteractionCombinators-$version.apk"
}
