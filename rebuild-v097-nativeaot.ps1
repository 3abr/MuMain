Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

param(
    [string]$DotNetPath = "C:\Program Files\dotnet",
    [string]$Preset = "windows-x86-v097-release",
    [string]$Config = "Release"
)

function Write-Step {
    param([string]$Message)
    Write-Host "[step] $Message" -ForegroundColor Cyan
}

function Write-Info {
    param([string]$Message)
    Write-Host "[info] $Message" -ForegroundColor Gray
}

function Fail {
    param([string]$Message)
    Write-Host "[error] $Message" -ForegroundColor Red
    exit 1
}

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
Push-Location $scriptDir

try {
    if (-not (Test-Path -LiteralPath $DotNetPath)) {
        Fail "dotnet folder not found: $DotNetPath"
    }

    $dotnetExe = Join-Path $DotNetPath "dotnet.exe"
    if (-not (Test-Path -LiteralPath $dotnetExe)) {
        Fail "dotnet.exe not found: $dotnetExe"
    }

    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Fail "cmake not found in PATH"
    }

    $env:PATH = "$DotNetPath;$env:PATH"
    Write-Info "Using dotnet: $dotnetExe"
    Write-Info "Working directory: $scriptDir"
    Write-Info "Preset: $Preset | Config: $Config"

    Write-Step "Cleaning stale managed client library artifacts"
    $staleFiles = @(
        "out/build/windows-x86-v097/MUnique.Client.Library.dll",
        "out/build/windows-x86-v097/MUnique.Client.Library.deps.json",
        "out/build/windows-x86-v097/MUnique.Client.Library.pdb",
        "out/build/windows-x86-v097/MUnique.Client.Library.xml"
    )
    foreach ($file in $staleFiles) {
        if (Test-Path -LiteralPath $file) {
            Remove-Item -LiteralPath $file -Force
        }
    }

    Write-Step "Configuring CMake preset: $Preset"
    & cmake --preset $Preset

    Write-Step "Building CMake preset: $Preset ($Config)"
    & cmake --build --preset $Preset --config $Config

    $dllPath = "out/build/windows-x86-v097/MUnique.Client.Library.dll"
    if (-not (Test-Path -LiteralPath $dllPath)) {
        Fail "Expected DLL not found: $dllPath"
    }

    Write-Step "Verifying native export: ConnectionManager_Connect"
    $exportFound = $false

    if (Get-Command dumpbin -ErrorAction SilentlyContinue) {
        $dump = & dumpbin /exports $dllPath 2>&1
        if (($dump | Select-String -SimpleMatch "ConnectionManager_Connect")) {
            $exportFound = $true
        }
    }
    elseif (Get-Command llvm-objdump -ErrorAction SilentlyContinue) {
        $dump = & llvm-objdump -p $dllPath 2>&1
        if (($dump | Select-String -SimpleMatch "ConnectionManager_Connect")) {
            $exportFound = $true
        }
    }
    elseif (Get-Command i686-w64-mingw32-objdump -ErrorAction SilentlyContinue) {
        $dump = & i686-w64-mingw32-objdump -p $dllPath 2>&1
        if (($dump | Select-String -SimpleMatch "ConnectionManager_Connect")) {
            $exportFound = $true
        }
    }
    else {
        Fail "No export inspection tool found. Install Visual Studio Build Tools (dumpbin) or LLVM (llvm-objdump)."
    }

    if (-not $exportFound) {
        Fail "Missing ConnectionManager_Connect export in $dllPath. DLL is not a valid NativeAOT native library export build."
    }

    Write-Host "[ok] Native export found in $dllPath" -ForegroundColor Green
    Write-Host "[done] v0.97 NativeAOT client library rebuild succeeded." -ForegroundColor Green
}
finally {
    Pop-Location
}
