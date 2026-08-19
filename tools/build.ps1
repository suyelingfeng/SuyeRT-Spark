<#
.SYNOPSIS
    构建 STM32F407 + RT-Thread + LVGL 固件。
.DESCRIPTION
    本脚本只生成构建产物，不会修改 STM32CubeMX 生成的时钟配置。
#>
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot "build\codex-gcc"
$cmake = "D:\ST\STM32CubeCLT_1.18.0\CMake\bin\cmake.exe"
$ninja = "D:\ST\STM32CubeCLT_1.18.0\Ninja\bin\ninja.exe"

if (-not (Test-Path -LiteralPath $cmake)) {
    throw "未找到 STM32CubeCLT CMake：$cmake"
}
if (-not (Test-Path -LiteralPath $ninja)) {
    throw "未找到 STM32CubeCLT Ninja：$ninja"
}

& $cmake -S $projectRoot -B $buildDir -G Ninja `
    -DCMAKE_TOOLCHAIN_FILE="$projectRoot\cmake\gcc-arm-none-eabi.cmake" `
    -DCMAKE_BUILD_TYPE=Debug
if ($LASTEXITCODE -ne 0) { throw "CMake 配置失败。" }

& $ninja -C $buildDir
if ($LASTEXITCODE -ne 0) { throw "固件编译失败。" }

Write-Host "构建完成：$buildDir\STM32F407_RTT.elf" -ForegroundColor Green
