# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Resolve-Path (Join-Path $ScriptDir "..\..")
$BuildDir = Join-Path $RootDir "build-onexplayer-1195g7-022"
$PkgDir = Join-Path $BuildDir "pkg"
$ArtifactsDir = Join-Path $RootDir "artifacts"
$ZipPath = Join-Path $ArtifactsDir "Eden-onexplayer-1195g7-022.zip"
$UseBundledQt = $true

function Invoke-Native {
    param(
        [Parameter(Mandatory = $true)]
        [string]$FilePath,
        [Parameter(ValueFromRemainingArguments = $true)]
        [string[]]$Arguments
    )

    & $FilePath @Arguments
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Add-DirectoryToPath {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Directory
    )

    if ((Test-Path $Directory) -and ($env:PATH -notlike "*$Directory*")) {
        $env:PATH = "$Directory;$env:PATH"
        Write-Host "-- Added to PATH: $Directory"
    }
}

function Find-GlslangValidator {
    $CandidateFiles = New-Object System.Collections.Generic.List[string]

    $PathCommand = Get-Command "glslangValidator.exe" -ErrorAction SilentlyContinue
    if ($PathCommand) {
        $CandidateFiles.Add($PathCommand.Source)
    }

    if ($env:VULKAN_SDK) {
        $CandidateFiles.Add((Join-Path $env:VULKAN_SDK "Bin\glslangValidator.exe"))
    }

    $SearchRoots = New-Object System.Collections.Generic.List[string]
    @(
        "C:\VulkanSDK",
        "C:\ProgramData\chocolatey\lib\vulkan-sdk"
    ) | ForEach-Object { $SearchRoots.Add($_) }

    if ($env:ProgramFiles) {
        $SearchRoots.Add((Join-Path $env:ProgramFiles "VulkanSDK"))
    }

    $ProgramFilesX86 = [Environment]::GetEnvironmentVariable("ProgramFiles(x86)")
    if ($ProgramFilesX86) {
        $SearchRoots.Add((Join-Path $ProgramFilesX86 "VulkanSDK"))
    }

    foreach ($Root in $SearchRoots) {
        if (Test-Path $Root) {
            Get-ChildItem -Path $Root -Filter "glslangValidator.exe" -Recurse -ErrorAction SilentlyContinue |
                ForEach-Object { $CandidateFiles.Add($_.FullName) }
        }
    }

    $Candidate = $CandidateFiles |
        Where-Object { $_ -and (Test-Path $_) } |
        Sort-Object -Unique |
        Select-Object -First 1

    if (-not $Candidate) {
        throw "glslangValidator.exe was not found. Install the Vulkan SDK or ensure its Bin directory is available."
    }

    $Candidate = (Resolve-Path $Candidate).Path
    Add-DirectoryToPath (Split-Path -Parent $Candidate)

    if (-not $env:VULKAN_SDK) {
        $BinDir = Split-Path -Parent $Candidate
        $SdkRoot = Split-Path -Parent $BinDir
        if ((Split-Path -Leaf $BinDir) -ieq "Bin") {
            $env:VULKAN_SDK = $SdkRoot
            Write-Host "-- Inferred VULKAN_SDK=$env:VULKAN_SDK"
        }
    }

    Write-Host "-- Using glslangValidator: $Candidate"
    return $Candidate
}

Push-Location $RootDir
try {
    & (Join-Path $ScriptDir "load-msvc-env.ps1")

    foreach ($tool in @("cmake", "ninja")) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "$tool was not found in PATH"
        }
    }

    $GlslangValidator = Find-GlslangValidator

    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    New-Item -ItemType Directory -Force -Path $PkgDir | Out-Null
    New-Item -ItemType Directory -Force -Path $ArtifactsDir | Out-Null

    $BuildPreset = "v3"
    $compilerArgs = @()
    if (Get-Command clang-cl -ErrorAction SilentlyContinue) {
        $Onexplayer1195G7CpuFlags = @(
            "/O2",
            "/Oi",
            "/Gy",
            "/Gw",
            "/clang:-march=x86-64-v3",
            "/clang:-mtune=tigerlake",
            "/clang:-mprefer-vector-width=128"
        )
        $BuildPreset = "custom"
        $Onexplayer1195G7Flags = $Onexplayer1195G7CpuFlags -join " "
        $compilerArgs = @(
            "-DCMAKE_C_COMPILER=clang-cl",
            "-DCMAKE_CXX_COMPILER=clang-cl",
            "-DCMAKE_C_FLAGS=$Onexplayer1195G7Flags",
            "-DCMAKE_CXX_FLAGS=$Onexplayer1195G7Flags"
        )
    }

    $CMakeConfigureArgs = @(
        "-S", ".",
        "-B", $BuildDir,
        "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DENABLE_QT_TRANSLATION=ON",
        "-DUSE_DISCORD_PRESENCE=ON",
        "-DYUZU_USE_BUNDLED_SDL2=ON",
        "-DBUILD_TESTING=OFF",
        "-DYUZU_TESTS=OFF",
        "-DDYNARMIC_TESTS=OFF",
        "-DYUZU_CMD=OFF",
        "-DYUZU_ROOM_STANDALONE=OFF",
        "-DYUZU_USE_QT_MULTIMEDIA=OFF",
        "-DYUZU_USE_QT_WEB_ENGINE=OFF",
        "-DYUZU_USE_BUNDLED_QT=$($UseBundledQt.ToString().ToUpperInvariant())",
        "-DYUZU_BUILD_PRESET=$BuildPreset",
        "-DENABLE_LTO=ON",
        "-DGLSLANGVALIDATOR=$GlslangValidator"
    ) + $compilerArgs

    Invoke-Native "cmake" @CMakeConfigureArgs
    Invoke-Native "cmake" "--build" $BuildDir "--config" "Release"

    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $BuildDir "bin\*.pdb")
    if (-not (Test-Path (Join-Path $BuildDir "bin"))) {
        throw "Build output directory was not found: $(Join-Path $BuildDir "bin")"
    }
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue $PkgDir
    New-Item -ItemType Directory -Force -Path $PkgDir | Out-Null
    Copy-Item -Force (Join-Path $BuildDir "bin\*") $PkgDir
    Copy-Item -Force (Join-Path $RootDir "LICENSE.txt") $PkgDir
    Copy-Item -Force (Join-Path $RootDir "README.md") $PkgDir
    Copy-Item -Recurse -Force (Join-Path $RootDir "LICENSES") $PkgDir
    if (-not (Test-Path (Join-Path $PkgDir "eden.exe"))) {
        throw "eden.exe was not found in package directory: $PkgDir"
    }

    if ($UseBundledQt) {
        Write-Host "-- Skipping windeployqt because YUZU_USE_BUNDLED_QT is enabled"
    } else {
        $WinDeployQt = $env:WINDEPLOYQT
        if (-not $WinDeployQt) {
            $WinDeployQtCommand = Get-Command "windeployqt.exe" -ErrorAction SilentlyContinue
            if ($WinDeployQtCommand) {
                $WinDeployQt = $WinDeployQtCommand.Source
            }
        }
        if (-not $WinDeployQt) {
            $QtTools = Get-ChildItem -Path @($RootDir, "C:\Qt", "$env:ProgramFiles\Qt") -Filter "windeployqt.exe" -Recurse -ErrorAction SilentlyContinue |
                Select-Object -First 1
            if ($QtTools) {
                $WinDeployQt = $QtTools.FullName
            }
        }
        if (-not $WinDeployQt) {
            throw "WINDEPLOYQT is not set and windeployqt.exe was not found"
        }

        Invoke-Native $WinDeployQt "--release" "--no-compiler-runtime" "--no-opengl-sw" "--no-system-dxc-compiler" "--no-system-d3d-compiler" "--dir" $PkgDir (Join-Path $PkgDir "eden.exe")
    }

    Remove-Item -Force -ErrorAction SilentlyContinue $ZipPath
    Compress-Archive -Path (Join-Path $PkgDir "*") -DestinationPath $ZipPath
    if (-not (Test-Path $ZipPath)) {
        throw "Package archive was not created: $ZipPath"
    }
    $ZipItem = Get-Item $ZipPath
    if ($ZipItem.Length -le 0) {
        throw "Package archive is empty: $ZipPath"
    }

    Write-Host "Created $ZipPath ($($ZipItem.Length) bytes)"
} finally {
    Pop-Location
}
