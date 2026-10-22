# build_tests.ps1 - 编译测试 jar（snake.jar 对齐 CI 的 test/SnakeGame.java；MediaTest 音频回归用）
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath "$root\tools"
if (Test-Path .\classes) { Remove-Item -Recurse -Force .\classes }
New-Item -ItemType Directory -Path .\classes | Out-Null
& javac -source 8 -target 8 -encoding UTF-8 -nowarn -cp ..\MIDP\midp.jar -d .\classes test\SnakeGame.java test\MediaTest.java test\PropTest.java test\CanvasTest.java
if ($LASTEXITCODE -ne 0) { throw 'javac FAILED' }
& jar cfm snake.jar manifest.mf -C .\classes .
& jar cf mediatest.jar -C .\classes .
"snake.jar: $((Get-Item snake.jar).Length) bytes / mediatest.jar: $((Get-Item mediatest.jar).Length) bytes"
