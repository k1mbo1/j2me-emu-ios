# build_midp.ps1 - 编译 MIDP API 为 midp.jar（javac @file 用 ANSI 编码，避免中文路径乱码）
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
if (Test-Path .\MIDP\classes) { Remove-Item -Recurse -Force .\MIDP\classes }
New-Item -ItemType Directory -Path .\MIDP\classes | Out-Null
$files = Get-ChildItem -LiteralPath .\MIDP\src -Recurse -Filter *.java | ForEach-Object { $_.FullName }
# javac 17 在 Windows 按 ANSI(GBK) 读取 @file；中文路径必须 ANSI 编码写入
[System.IO.File]::WriteAllLines("$root\MIDP\sources.txt", $files,
    [System.Text.Encoding]::GetEncoding(936))
& javac -source 8 -target 8 -encoding UTF-8 -nowarn -d .\MIDP\classes "@MIDP\sources.txt"
if ($LASTEXITCODE -ne 0) { throw 'javac FAILED' }
& jar cf .\MIDP\midp.jar -C .\MIDP\classes .
"midp.jar OK: $((Get-Item .\MIDP\midp.jar).Length) bytes"
