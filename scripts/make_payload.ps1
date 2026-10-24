# 打包安装程序用的 payload.zip
# 由 scripts\package.bat 调用，不单独使用。

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$payload = 'build\payload'
if (Test-Path $payload) { Remove-Item $payload -Recurse -Force }
New-Item -ItemType Directory -Path $payload | Out-Null

Copy-Item 'bin\A_or_B.exe'      $payload
Copy-Item 'bin\aorb_core.dll'   $payload
Copy-Item 'installer\Readme.txt' (Join-Path $payload '游戏介绍.txt')

if (Test-Path 'build\payload.zip') { Remove-Item 'build\payload.zip' -Force }
Compress-Archive -Path (Join-Path $payload '*') -DestinationPath 'build\payload.zip' -Force

$size = (Get-Item 'build\payload.zip').Length
Write-Host "[package] payload.zip = $size bytes"
