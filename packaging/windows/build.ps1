param(
    [Parameter(Mandatory = $true)]
    [string]$Version,

    [string]$QtBin = "",
    [string]$GStreamerRoot = $env:GSTREAMER_ROOT,
    [string]$BuildDir = "build-ci",
    [string]$DistDir = "dist",
    [string]$ReleaseDir = "release"
)

$ErrorActionPreference = "Stop"

if ($QtBin) {
    $env:PATH = "$QtBin;$env:PATH"
}
if (-not $GStreamerRoot) {
    throw "GStreamerRoot is not set"
}
if (-not (Test-Path "$GStreamerRoot\include\gstreamer-1.0\gst\gst.h")) {
    throw "GStreamer development files not found at $GStreamerRoot"
}

$env:APP_VERSION = $Version
$root = (Get-Location).Path
$buildPath = Join-Path $root $BuildDir
$distPath = Join-Path $root $DistDir
$releasePath = Join-Path $root $ReleaseDir

Remove-Item $buildPath, $distPath, $releasePath -Recurse -Force -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force $buildPath, $distPath, $releasePath | Out-Null

Push-Location $buildPath
try {
    & qmake ..\Concord.pro "CONFIG+=release"
    if ($LASTEXITCODE -ne 0) { throw "qmake failed" }

    & nmake
    if ($LASTEXITCODE -ne 0) { throw "nmake failed" }
} finally {
    Pop-Location
}

$exe = Get-ChildItem $buildPath -Recurse -Filter Concord.exe |
    Where-Object { $_.FullName -notmatch "\\debug\\" } |
    Select-Object -First 1
if (-not $exe) {
    throw "Concord.exe was not produced"
}

Copy-Item $exe.FullName (Join-Path $distPath "Concord.exe") -Force

& windeployqt --release --no-translations --compiler-runtime --qmldir $root --dir $distPath $exe.FullName
if ($LASTEXITCODE -ne 0) { throw "windeployqt failed" }

Copy-Item "$GStreamerRoot\bin\*.dll" $distPath -Force
$pluginDir = Join-Path $distPath "lib\gstreamer-1.0"
$scannerDir = Join-Path $distPath "libexec\gstreamer-1.0"
New-Item -ItemType Directory -Force $pluginDir, $scannerDir | Out-Null
Copy-Item "$GStreamerRoot\lib\gstreamer-1.0\*" $pluginDir -Recurse -Force
Copy-Item "$GStreamerRoot\libexec\gstreamer-1.0\*" $scannerDir -Recurse -Force
if (Test-Path "$GStreamerRoot\share\gstreamer-1.0") {
    $shareDir = Join-Path $distPath "share"
    New-Item -ItemType Directory -Force $shareDir | Out-Null
    Copy-Item "$GStreamerRoot\share\gstreamer-1.0" $shareDir -Recurse -Force
}

$deps = & dumpbin /dependents $exe.FullName | Out-String
if ($deps -notmatch "gstreamer-1\.0\.dll") {
    throw "Concord.exe was built without GStreamer"
}

$env:GST_PLUGIN_PATH_1_0 = $pluginDir
$env:GST_PLUGIN_SYSTEM_PATH_1_0 = $pluginDir
$env:GST_PLUGIN_PATH = $pluginDir
$env:GST_PLUGIN_SYSTEM_PATH = $pluginDir
$env:GST_PLUGIN_SCANNER_1_0 = Join-Path $scannerDir "gst-plugin-scanner.exe"
$env:GST_PLUGIN_SCANNER = $env:GST_PLUGIN_SCANNER_1_0
$env:PATH = "$distPath;$env:PATH"

Copy-Item "$GStreamerRoot\bin\gst-inspect-1.0.exe" $distPath -Force
try {
    foreach ($feature in @("webrtcbin", "vp8enc", "opusenc", "autoaudiosrc", "ksvideosrc", "d3d11screencapturesrc")) {
        & (Join-Path $distPath "gst-inspect-1.0.exe") $feature *> $null
        if ($LASTEXITCODE -ne 0) {
            throw "GStreamer feature not available after deployment: $feature"
        }
    }
} finally {
    Remove-Item (Join-Path $distPath "gst-inspect-1.0.exe") -Force -ErrorAction SilentlyContinue
}

$env:QT_QPA_PLATFORM = "offscreen"
$env:QT_QUICK_BACKEND = "software"
$smokeOut = Join-Path $buildPath "smoke.out.log"
$smokeErr = Join-Path $buildPath "smoke.err.log"
$proc = Start-Process -FilePath (Join-Path $distPath "Concord.exe") -PassThru `
    -RedirectStandardOutput $smokeOut -RedirectStandardError $smokeErr
Start-Sleep -Seconds 8
if ($proc.HasExited) {
    Get-Content $smokeOut, $smokeErr -ErrorAction SilentlyContinue
    throw "Concord exited during the smoke test with code $($proc.ExitCode)"
}
Stop-Process -Id $proc.Id -Force -ErrorAction SilentlyContinue

$iscc = "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
if (-not (Test-Path $iscc)) {
    throw "Inno Setup not found at $iscc"
}
& $iscc "/DAPP_VERSION=$Version" "/DSOURCE_DIR=$distPath" "/DOUTPUT_DIR=$releasePath" `
    "packaging\windows\Concord.iss"
if ($LASTEXITCODE -ne 0) { throw "Inno Setup failed" }

Compress-Archive -Path (Join-Path $distPath "*") `
    -DestinationPath (Join-Path $releasePath "Concord-$Version-windows-x64-portable.zip") `
    -CompressionLevel Optimal -Force

Get-ChildItem $releasePath | Format-Table Name, Length
