# build_headless.ps1 - 本地 Windows 编译 headless 引擎回归（VM + Bridge + miniz + headless_main）
# MINIZ_USE_ACP: Windows 下 argv 路径为 ANSI(GBK)，miniz 按 ACP 转宽字符（iOS 分支不受影响）
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Set-Location -LiteralPath $root
& gcc -O2 -DMINIZ_USE_ACP -o tools\headless.exe `
    tools\headless_main.c `
    VM\vm.c VM\classfile.c VM\interp.c VM\heap.c VM\zip.c `
    Bridge\bridge_common.c Bridge\midi_synth.c `
    VM\miniz\miniz.c VM\miniz\miniz_zip.c VM\miniz\miniz_tinfl.c VM\miniz\miniz_tdef.c `
    -I VM -I Bridge
if ($LASTEXITCODE -ne 0) { throw 'gcc FAILED' }
"headless.exe OK: $((Get-Item tools\headless.exe).Length) bytes"
