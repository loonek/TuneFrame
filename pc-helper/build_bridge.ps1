# Builds the TuneFrame bridge into a standalone binary with Nuitka and packages
# it as dist/tuneframe-bridge.zip for a GitHub release.
#
# Nuitka compiles the Python to a real native binary, which (unlike a PyInstaller
# one-file exe) does not trip antivirus heuristics.
#
# Usage:  powershell -ExecutionPolicy Bypass -File build_bridge.ps1

$ErrorActionPreference = "Stop"
$here = $PSScriptRoot
$py = Join-Path $here ".venv\Scripts\python.exe"

Push-Location $here
try {
    & $py -m nuitka `
        --standalone `
        --assume-yes-for-downloads `
        --windows-console-mode=disable `
        --enable-plugin=tk-inter `
        --include-module=pystray._win32 `
        --include-package=websockets `
        --include-data-files=tuneframe.png=tuneframe.png `
        --windows-icon-from-ico=tuneframe.ico `
        --output-filename=tuneframe-bridge.exe `
        --output-dir=..\dist_nuitka `
        bridge.py
    if ($LASTEXITCODE -ne 0) { throw "Nuitka build failed" }

    # Package the standalone folder as dist/tuneframe-bridge.zip (top-level
    # folder named tuneframe-bridge/).
    New-Item -ItemType Directory -Force ..\dist | Out-Null
    $named = "..\dist_nuitka\tuneframe-bridge"
    $zip = "..\dist\tuneframe-bridge.zip"
    Remove-Item $named -Recurse -Force -ErrorAction SilentlyContinue
    Remove-Item $zip -ErrorAction SilentlyContinue
    Rename-Item "..\dist_nuitka\bridge.dist" $named
    Compress-Archive -Path $named -DestinationPath $zip

    Write-Output "Done -> dist\tuneframe-bridge.zip"
}
finally {
    Pop-Location
}
