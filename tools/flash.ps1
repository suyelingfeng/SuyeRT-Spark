<#
.SYNOPSIS
    Flash and reset the board through the on-board ST-LINK/SWD interface.
#>
$ErrorActionPreference = "Stop"

$projectRoot = Split-Path -Parent $PSScriptRoot
$firmware = Join-Path $projectRoot "build\codex-gcc\STM32F407_RTT.elf"
$programmer = "D:\ST\STM32CubeCLT_1.18.0\STM32CubeProgrammer\bin\STM32_Programmer_CLI.exe"

if (-not (Test-Path -LiteralPath $firmware)) {
    throw "Firmware not found. Run tools\build.ps1 first."
}
if (-not (Test-Path -LiteralPath $programmer)) {
    throw "STM32CubeProgrammer was not found: $programmer"
}

Write-Host "Connecting to ST-LINK and flashing firmware..." -ForegroundColor Cyan
& $programmer -c port=SWD freq=4000 -d $firmware -v -rst
if ($LASTEXITCODE -ne 0) { throw "Flash or verification failed. Check ST-LINK and board power." }

Write-Host "Flash complete. ST-LINK VCP: 115200, 8N1, no flow control." -ForegroundColor Green
