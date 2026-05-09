# SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
# SPDX-License-Identifier: GPL-3.0-or-later

$ErrorActionPreference = "Stop"

$ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$RootDir = Resolve-Path (Join-Path $ScriptDir "..\..")
$BuildDir = Join-Path $RootDir "build-onexplayer-1195g7-001"
$PkgDir = Join-Path $BuildDir "pkg"
$ArtifactsDir = Join-Path $RootDir "artifacts"
$ZipPath = Join-Path $ArtifactsDir "Eden-Windows-onexplayer-1195g7-001.zip"

Push-Location $RootDir
try {
    & (Join-Path $ScriptDir "load-msvc-env.ps1")

    foreach ($tool in @("cmake", "ninja")) {
        if (-not (Get-Command $tool -ErrorAction SilentlyContinue)) {
            throw "$tool was not found in PATH"
        }
    }

    New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
    New-Item -ItemType Directory -Force -Path $PkgDir | Out-Null
    New-Item -ItemType Directory -Force -Path $ArtifactsDir | Out-Null

    $compilerArgs = @()
    if (Get-Command clang-cl -ErrorAction SilentlyContinue) {
        $compilerArgs = @(
            "-DCMAKE_C_COMPILER=clang-cl",
            "-DCMAKE_CXX_COMPILER=clang-cl",
            "-DCMAKE_C_FLAGS=/O2",
            "-DCMAKE_CXX_FLAGS=/O2"
        )
    }

    cmake -S . -B $BuildDir -G Ninja `
        -DCMAKE_BUILD_TYPE=Release `
        -DENABLE_QT_TRANSLATION=ON `
        -DUSE_DISCORD_PRESENCE=ON `
        -DYUZU_USE_BUNDLED_SDL2=ON `
        -DBUILD_TESTING=OFF `
        -DYUZU_TESTS=OFF `
        -DDYNARMIC_TESTS=OFF `
        -DYUZU_CMD=OFF `
        -DYUZU_ROOM_STANDALONE=OFF `
        -DYUZU_USE_QT_MULTIMEDIA=OFF `
        -DYUZU_USE_QT_WEB_ENGINE=OFF `
        -DYUZU_USE_BUNDLED_QT=ON `
        -DENABLE_LTO=ON `
        @compilerArgs

    cmake --build $BuildDir --config Release

    Remove-Item -Force -ErrorAction SilentlyContinue (Join-Path $BuildDir "bin\*.pdb")
    Copy-Item -Force (Join-Path $BuildDir "bin\*") $PkgDir
    Copy-Item -Force (Join-Path $RootDir "LICENSE.txt") $PkgDir
    Copy-Item -Force (Join-Path $RootDir "README.md") $PkgDir
    Copy-Item -Recurse -Force (Join-Path $RootDir "LICENSES") $PkgDir

    $WinDeployQt = $env:WINDEPLOYQT
    if (-not $WinDeployQt) {
        $QtTools = Get-ChildItem -Path $RootDir -Filter "windeployqt.exe" -Recurse -ErrorAction SilentlyContinue |
            Select-Object -First 1
        if ($QtTools) {
            $WinDeployQt = $QtTools.FullName
        }
    }
    if (-not $WinDeployQt) {
        throw "WINDEPLOYQT is not set and windeployqt.exe was not found under the repository"
    }

    & $WinDeployQt --release --no-compiler-runtime --no-opengl-sw --no-system-dxc-compiler --no-system-d3d-compiler --dir $PkgDir (Join-Path $PkgDir "eden.exe")

    Remove-Item -Force -ErrorAction SilentlyContinue $ZipPath
    Compress-Archive -Path (Join-Path $PkgDir "*") -DestinationPath $ZipPath

    Write-Host "Created $ZipPath"
} finally {
    Pop-Location
}
