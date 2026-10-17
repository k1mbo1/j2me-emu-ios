# Generates 1024x1024 app icon (abstract blue ring) and base64 text
Add-Type -AssemblyName System.Drawing
$size = 1024
$bmp = New-Object System.Drawing.Bitmap($size, $size)
$g = [System.Drawing.Graphics]::FromImage($bmp)
$g.SmoothingMode = [System.Drawing.Drawing2D.SmoothingMode]::AntiAlias

# Background: dark vertical gradient
$bgRect = New-Object System.Drawing.Rectangle(0, 0, $size, $size)
$bg = New-Object System.Drawing.Drawing2D.LinearGradientBrush($bgRect, [System.Drawing.Color]::FromArgb(255, 16, 24, 40), [System.Drawing.Color]::FromArgb(255, 8, 12, 22), 90.0)
$g.FillRectangle($bg, $bgRect)

$cx = 512
$cy = 512

# Outer ring: radial blue gradient disc
$outerR = 370
$path = New-Object System.Drawing.Drawing2D.GraphicsPath
$path.AddEllipse(($cx - $outerR), ($cy - $outerR), ($outerR * 2), ($outerR * 2))
$pgb = New-Object System.Drawing.Drawing2D.PathGradientBrush($path)
$pgb.CenterPoint = New-Object System.Drawing.PointF(($cx - 120), ($cy - 120))
$pgb.CenterColor = [System.Drawing.Color]::FromArgb(255, 128, 202, 255)
$pgb.SurroundColors = @([System.Drawing.Color]::FromArgb(255, 34, 90, 220))
$g.FillEllipse($pgb, ($cx - $outerR), ($cy - $outerR), ($outerR * 2), ($outerR * 2))

# Inner disc: dark center -> creates the ring look
$innerR = 250
$path2 = New-Object System.Drawing.Drawing2D.GraphicsPath
$path2.AddEllipse(($cx - $innerR), ($cy - $innerR), ($innerR * 2), ($innerR * 2))
$pgb2 = New-Object System.Drawing.Drawing2D.PathGradientBrush($path2)
$pgb2.CenterColor = [System.Drawing.Color]::FromArgb(255, 22, 32, 54)
$pgb2.SurroundColors = @([System.Drawing.Color]::FromArgb(255, 10, 16, 28))
$g.FillEllipse($pgb2, ($cx - $innerR), ($cy - $innerR), ($innerR * 2), ($innerR * 2))

# Small accent dot in center
$dotR = 46
$path3 = New-Object System.Drawing.Drawing2D.GraphicsPath
$path3.AddEllipse(($cx - $dotR), ($cy - $dotR), ($dotR * 2), ($dotR * 2))
$pgb3 = New-Object System.Drawing.Drawing2D.PathGradientBrush($path3)
$pgb3.CenterColor = [System.Drawing.Color]::FromArgb(255, 110, 190, 255)
$pgb3.SurroundColors = @([System.Drawing.Color]::FromArgb(255, 40, 100, 225))
$g.FillEllipse($pgb3, ($cx - $dotR), ($cy - $dotR), ($dotR * 2), ($dotR * 2))

$g.Dispose()
$out = $PSScriptRoot + "\icon1024.png"
$bmp.Save($out, [System.Drawing.Imaging.ImageFormat]::Png)
$bmp.Dispose()
[Convert]::ToBase64String([IO.File]::ReadAllBytes($out)) | Set-Content -Encoding ASCII ($PSScriptRoot + "\icon1024.b64")
Write-Output ("icon saved: " + $out)
