# 打源码包：dist\A_or_B-src-v2.0.0.zip
# 内容：源码 + 头文件 + 脚本 + 一键编译 make.bat + json.hpp（可直接编译）
# 不含：bin / build / dist / data / logs / versions / .vscode

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
Set-Location $root

$name  = 'A_or_B-src-v2.0.0'
$stage = "build\$name"

if (Test-Path $stage) { Remove-Item $stage -Recurse -Force }
New-Item -ItemType Directory -Path $stage | Out-Null

Copy-Item 'make.bat'   $stage
Copy-Item 'README.md'  $stage
Copy-Item 'CHANGELOG.md' $stage
Copy-Item 'LICENSE.txt'  $stage
Copy-Item 'src'       $stage -Recurse
Copy-Item 'include'   $stage -Recurse
Copy-Item 'scripts'   $stage -Recurse
Copy-Item 'installer' $stage -Recurse
Copy-Item 'tests'     $stage -Recurse
New-Item -ItemType Directory -Path (Join-Path $stage 'third_party') | Out-Null
Copy-Item 'third_party\json.hpp' (Join-Path $stage 'third_party')

if (-not (Test-Path 'dist')) { New-Item -ItemType Directory -Path 'dist' | Out-Null }
$zip = Join-Path 'dist' "$name.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }

Compress-Archive -Path $stage -DestinationPath $zip -CompressionLevel Optimal -Force

$size = [math]::Round((Get-Item $zip).Length / 1KB, 1)
Write-Host "[pack] $zip  ($size KB)"
