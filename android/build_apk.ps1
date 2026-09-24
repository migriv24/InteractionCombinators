# build_apk.ps1 — build, package, and sign the Interaction Combinators APK
# without Gradle: NDK CMake -> aapt2 link -> aapt add -> zipalign -> apksigner.
# Requires an Android SDK (ndk, platforms, build-tools) and Android Studio's
# bundled JBR for keytool/apksigner. Run from anywhere:
#   powershell -File android\build_apk.ps1 [-Abi arm64-v8a] [-Install]
param(
    [string]$Abi = "arm64-v8a",
    [switch]$Install
)
$ErrorActionPreference = "Stop"

# one version for every build: ../VERSION ("0.2.0"); the Android versionCode is
# derived from it (major*10000 + minor*100 + patch), so it always increases
$version = (Get-Content "$PSScriptRoot\..\VERSION" -Raw).Trim()
$parts = $version.Split(".") | ForEach-Object { [int]$_ }
$versionCode = $parts[0] * 10000 + $parts[1] * 100 + $parts[2]

$here = $PSScriptRoot
$sdk = if ($env:ANDROID_HOME) { $env:ANDROID_HOME } else { "$env:LOCALAPPDATA\Android\Sdk" }
if (-not (Test-Path $sdk)) { throw "Android SDK not found at $sdk (set ANDROID_HOME)" }

# newest installed of each component
$ndk = Get-ChildItem "$sdk\ndk" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$bt = Get-ChildItem "$sdk\build-tools" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$platform = Get-ChildItem "$sdk\platforms" -Directory |
    Sort-Object { [double]($_.Name -replace "android-", "") } -Descending |
    Select-Object -First 1
$cmakeDir = Get-ChildItem "$sdk\cmake" -Directory | Sort-Object Name -Descending | Select-Object -First 1
$cmake = "$($cmakeDir.FullName)\bin\cmake.exe"
$ninja = "$($cmakeDir.FullName)\bin\ninja.exe"
$jbr = "C:\Program Files\Android\Android Studio\jbr"
if (-not (Test-Path "$jbr\bin\java.exe")) { throw "JBR not found at $jbr" }
Write-Host "ndk $($ndk.Name) | build-tools $($bt.Name) | $($platform.Name) | $Abi"

# 1. native build (Void Maiz's CMake handles the ANDROID branch)
& $cmake -S $here -B "$here\build" -G Ninja `
    "-DCMAKE_TOOLCHAIN_FILE=$($ndk.FullName)\build\cmake\android.toolchain.cmake" `
    "-DANDROID_ABI=$Abi" `
    "-DANDROID_PLATFORM=android-26" `
    "-DANDROID_STL=c++_static" `
    "-DCMAKE_BUILD_TYPE=Release" `
    "-DCMAKE_MAKE_PROGRAM=$ninja"
if ($LASTEXITCODE) { throw "cmake configure failed" }
& $cmake --build "$here\build"
if ($LASTEXITCODE) { throw "native build failed" }

# 2. stage the native lib at its APK path (aapt stores paths verbatim)
$stage = "$here\build\apk"
New-Item -ItemType Directory -Force "$stage\lib\$Abi" | Out-Null
Copy-Item "$here\build\libinteraction_combinators.so" "$stage\lib\$Abi\" -Force

# 3. resources + manifest -> base apk
$unaligned = "$here\build\unaligned.apk"
& "$($bt.FullName)\aapt2.exe" link -o $unaligned `
    --manifest "$here\AndroidManifest.xml" `
    -I "$($platform.FullName)\android.jar" `
    --min-sdk-version 26 --target-sdk-version 34 `
    --version-code $versionCode --version-name $version
if ($LASTEXITCODE) { throw "aapt2 link failed" }

# 4. add the lib (relative path with forward slashes = the in-APK path)
Push-Location $stage
& "$($bt.FullName)\aapt.exe" add $unaligned "lib/$Abi/libinteraction_combinators.so"
$aaptExit = $LASTEXITCODE
Pop-Location
if ($aaptExit) { throw "aapt add failed" }

# 4b. Void Maiz's one Java class (org.voidmaiz.MaizActivity: Android's own
# keyboard, VoidMaiz okf/concepts/text-input.md), compiled to classes.dex
$maiz = if ($env:VOIDMAIZ_ROOT) { $env:VOIDMAIZ_ROOT } else { "$here\..\..\VoidMaiz" }
& "$maiz\android\build_java.ps1" -OutDir "$here\build\dex"
Push-Location "$here\build\dex"
& "$($bt.FullName)\aapt.exe" add $unaligned "classes.dex"
$aaptExit = $LASTEXITCODE
Pop-Location
if ($aaptExit) { throw "aapt add classes.dex failed" }

# 5. align (16KB pages — Android 15 requirement)
$aligned = "$here\build\aligned.apk"
& "$($bt.FullName)\zipalign.exe" -f -P 16 4 $unaligned $aligned
if ($LASTEXITCODE) { throw "zipalign failed" }

# 6. debug keystore (minted once, kept beside the script)
$ks = "$here\debug.keystore"
if (-not (Test-Path $ks)) {
    & "$jbr\bin\keytool.exe" -genkeypair -keystore $ks -alias androiddebugkey `
        -storepass android -keypass android -keyalg RSA -keysize 2048 `
        -validity 10000 -dname "CN=Android Debug,O=Android,C=US"
    if ($LASTEXITCODE) { throw "keytool failed" }
}

# 7. sign
$env:JAVA_HOME = $jbr
$out = "$here\interaction_combinators.apk"
& "$($bt.FullName)\apksigner.bat" sign --ks $ks --ks-pass pass:android `
    --key-pass pass:android --out $out $aligned
if ($LASTEXITCODE) { throw "apksigner failed" }
Write-Host "APK: $out ($([math]::Round((Get-Item $out).Length / 1MB, 2)) MB)"

# 8. optional: install to a connected device
if ($Install) {
    & "$sdk\platform-tools\adb.exe" install -r $out
}
