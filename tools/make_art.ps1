# Draws the mod's own art, in the colors of the game's menus (cream f0e3cc / dark 40392b / orange f97b03):
#   mod/mod_image.jpg                                       the Mods menu / Workshop thumbnail: a grid of rifle
#                                                           silhouettes under header bars, one marked as equipped
#   mod/cursor/eras_cursor.cur                              the mouse cursor: a cream arrow with a dark outline and an
#                                                           orange tip, at 32/48/64/96 px (the plugin picks one by
#                                                           screen height)
#   mod/textures/gui/dk2ml_ps_open_normal.dds, _hover.dds   the portrait picker's "choose portrait" button (64x64,
#                                                           hover inverted): a 3x3 grid of head-and-shoulders
#                                                           silhouettes, uncompressed 24-bit DDS like the stock icons
# Nothing is taken from the game's assets.
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing

$cream = [Drawing.Color]::FromArgb(0xf0, 0xe3, 0xcc)
$dark = [Drawing.Color]::FromArgb(0x40, 0x39, 0x2b)
$tile = [Drawing.Color]::FromArgb(0x60, 0x5a, 0x4f)
$orange = [Drawing.Color]::FromArgb(0xf9, 0x7b, 0x03)
$modDir = Join-Path $PSScriptRoot '..\mod'

# a rifle facing right in a w x h box at (x, y): stock, body, magazine, grip, barrel
function Draw-Rifle($g, $brush, $x, $y, $w, $h) {
    $f = { param($fx, $fy, $fw, $fh) $g.FillRectangle($brush, [single]($x + $fx * $w), [single]($y + $fy * $h), [single]($fw * $w), [single]($fh * $h)) }
    & $f 0.02 0.30 0.20 0.30   # stock
    & $f 0.20 0.25 0.45 0.25   # receiver
    & $f 0.65 0.32 0.33 0.10   # barrel
    & $f 0.45 0.50 0.09 0.38   # magazine
    & $f 0.27 0.50 0.07 0.28   # grip
}

$size = 512
$scale = 4 # drawn large and scaled down, for smooth edges
$s = $size * $scale
$big = New-Object Drawing.Bitmap $s, $s
$g = [Drawing.Graphics]::FromImage($big)
$g.SmoothingMode = 'AntiAlias'
$g.Clear($dark)

$margin = $s * 0.07
$gap = $s * 0.035
$cols = 3; $rows = 5
$header = $s * 0.035
$cellW = ($s - 2 * $margin - ($cols - 1) * $gap) / $cols
$cellH = ($s - 2 * $margin - $rows * $gap - 2 * ($header + $gap)) / $rows
$tileBrush = New-Object Drawing.SolidBrush $tile
$creamBrush = New-Object Drawing.SolidBrush $cream
$orangeBrush = New-Object Drawing.SolidBrush $orange
$y = $margin
foreach ($group in @(3, 2)) { # two groups: a header bar, then rows of tiles
    $g.FillRectangle($orangeBrush, [single]$margin, [single]($y + $header * 0.6), [single]($s - 2 * $margin), [single]($header * 0.4))
    $y += $header + $gap
    for ($row = 0; $row -lt $group; $row++) {
        for ($col = 0; $col -lt $cols; $col++) {
            $x = $margin + $col * ($cellW + $gap)
            $g.FillRectangle($tileBrush, [single]$x, [single]$y, [single]$cellW, [single]$cellH)
            $equipped = ($group -eq 3 -and $row -eq 1 -and $col -eq 1)
            if ($equipped) {
                $pen = New-Object Drawing.Pen $orange, ([single]($s * 0.008))
                $g.DrawRectangle($pen, [single]$x, [single]$y, [single]$cellW, [single]$cellH)
            }
            Draw-Rifle $g $creamBrush ($x + $cellW * 0.1) ($y + $cellH * 0.15) ($cellW * 0.8) ($cellH * 0.7)
        }
        $y += $cellH + $gap
    }
}

$small = New-Object Drawing.Bitmap $size, $size
$gs = [Drawing.Graphics]::FromImage($small)
$gs.InterpolationMode = 'HighQualityBicubic'
$gs.PixelOffsetMode = 'HighQuality'
$gs.DrawImage($big, 0, 0, $size, $size)
$g.Dispose(); $gs.Dispose(); $big.Dispose()

New-Item -ItemType Directory -Force $modDir | Out-Null
$path = Join-Path $modDir 'mod_image.jpg'
$small.Save($path, [Drawing.Imaging.ImageFormat]::Jpeg)
$small.Dispose()
(Get-Item $path).FullName

# --- the portrait picker's button ---

# grid of n x n portrait tiles filling a size x size image; tile `highlight` (or -1) gets the accent color
function Draw-Grid($size, $n, $background, $tile, $figure, $accent, $highlight) {
    $scale = 4 # drawn large and scaled down, for smooth edges
    $s = $size * $scale
    $big = New-Object Drawing.Bitmap $s, $s
    $g = [Drawing.Graphics]::FromImage($big)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear($background)
    $margin = $s * 0.08
    $gap = $s * 0.05
    $cell = ($s - 2 * $margin - ($n - 1) * $gap) / $n
    for ($row = 0; $row -lt $n; $row++) {
        for ($col = 0; $col -lt $n; $col++) {
            $x = $margin + $col * ($cell + $gap)
            $y = $margin + $row * ($cell + $gap)
            $i = $row * $n + $col
            $tileBrush = New-Object Drawing.SolidBrush ($(if ($i -eq $highlight) { $accent } else { $tile }))
            $g.FillRectangle($tileBrush, [single]$x, [single]$y, [single]$cell, [single]$cell)
            # silhouette: head and shoulders, clipped to the tile
            $g.SetClip((New-Object Drawing.RectangleF ([single]$x), ([single]$y), ([single]$cell), ([single]$cell)))
            $fig = New-Object Drawing.SolidBrush $figure
            $head = $cell * 0.36
            $g.FillEllipse($fig, [single]($x + ($cell - $head) / 2), [single]($y + $cell * 0.18), [single]$head, [single]$head)
            $g.FillEllipse($fig, [single]($x + $cell * 0.14), [single]($y + $cell * 0.6), [single]($cell * 0.72), [single]($cell * 0.7))
            $g.ResetClip()
        }
    }
    $small = New-Object Drawing.Bitmap $size, $size
    $gs = [Drawing.Graphics]::FromImage($small)
    $gs.InterpolationMode = 'HighQualityBicubic'
    $gs.PixelOffsetMode = 'HighQuality'
    $gs.DrawImage($big, 0, 0, $size, $size)
    $g.Dispose(); $gs.Dispose(); $big.Dispose()
    return $small
}

# Uncompressed 24-bit RGB DDS, top-down rows, BGR byte order (masks R=0xFF0000 G=0xFF00 B=0xFF), no mipmaps.
function Save-Dds($bitmap, $path) {
    $w = $bitmap.Width; $h = $bitmap.Height
    $ms = New-Object IO.MemoryStream
    $bw = New-Object IO.BinaryWriter $ms
    $bw.Write([Text.Encoding]::ASCII.GetBytes('DDS '))
    $bw.Write([uint32]124)                  # header size
    $bw.Write([uint32]0x100F)               # CAPS | HEIGHT | WIDTH | PITCH | PIXELFORMAT
    $bw.Write([uint32]$h); $bw.Write([uint32]$w)
    $bw.Write([uint32]($w * 3))             # pitch
    $bw.Write([uint32]0); $bw.Write([uint32]0) # depth, mipmap count
    for ($i = 0; $i -lt 11; $i++) { $bw.Write([uint32]0) } # reserved
    $bw.Write([uint32]32); $bw.Write([uint32]0x40) # pixel format: size, DDPF_RGB
    $bw.Write([uint32]0); $bw.Write([uint32]24)    # fourcc, bit count
    $bw.Write([uint32]0x00FF0000); $bw.Write([uint32]0x0000FF00); $bw.Write([uint32]0x000000FF); $bw.Write([uint32]0)
    $bw.Write([uint32]0x1000)               # caps: texture
    for ($i = 0; $i -lt 4; $i++) { $bw.Write([uint32]0) }
    for ($y = 0; $y -lt $h; $y++) {
        for ($x = 0; $x -lt $w; $x++) {
            $c = $bitmap.GetPixel($x, $y)
            $bw.Write([byte]$c.B); $bw.Write([byte]$c.G); $bw.Write([byte]$c.R)
        }
    }
    $bw.Flush()
    [IO.File]::WriteAllBytes($path, $ms.ToArray())
}

$texDir = Join-Path $modDir 'textures\gui'
New-Item -ItemType Directory -Force $texDir | Out-Null
$normal = Draw-Grid 64 3 $cream $dark $cream $dark -1
Save-Dds $normal (Join-Path $texDir 'dk2ml_ps_open_normal.dds')
$hover = Draw-Grid 64 3 $dark $cream $dark $cream -1
Save-Dds $hover (Join-Path $texDir 'dk2ml_ps_open_hover.dds')
$normal.Dispose(); $hover.Dispose()
(Get-ChildItem $texDir).FullName

# --- the mouse cursor (mod/cursor/eras_cursor.cur) ---

# A classic arrow, tip at the top left, in a 32-unit grid: cream fill, dark outline, an orange tip.
$arrow = @(@(1, 1), @(1, 24), @(7, 18.5), @(11, 28), @(15, 26.3), @(11.2, 17), @(19, 17))
$tipAccent = @(@(1, 1), @(1, 9), @(6.5, 6.5))
$dark2 = [Drawing.Color]::FromArgb(0x21, 0x1e, 0x1d)

function Draw-Cursor($size) {
    $scale = 4
    $s = $size * $scale
    $k = $s / 32.0
    $big = New-Object Drawing.Bitmap $s, $s, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($big)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([Drawing.Color]::Transparent)
    $pts = $arrow | ForEach-Object { New-Object Drawing.PointF ([single]($_[0] * $k)), ([single]($_[1] * $k)) }
    $tip = $tipAccent | ForEach-Object { New-Object Drawing.PointF ([single]($_[0] * $k)), ([single]($_[1] * $k)) }
    $g.FillPolygon((New-Object Drawing.SolidBrush $cream), [Drawing.PointF[]]$pts)
    $g.FillPolygon((New-Object Drawing.SolidBrush $orange), [Drawing.PointF[]]$tip)
    $pen = New-Object Drawing.Pen $dark2, ([single](1.6 * $k))
    $pen.LineJoin = 'Round'
    $g.DrawPolygon($pen, [Drawing.PointF[]]$pts)
    $small = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gs = [Drawing.Graphics]::FromImage($small)
    $gs.InterpolationMode = 'HighQualityBicubic'
    $gs.PixelOffsetMode = 'HighQuality'
    $gs.Clear([Drawing.Color]::Transparent)
    $gs.DrawImage($big, 0, 0, $size, $size)
    $g.Dispose(); $gs.Dispose(); $big.Dispose(); $pen.Dispose()
    return $small
}

# Multi-size .cur: one 32-bit BGRA DIB per size (BITMAPINFOHEADER with double height, bottom-up rows, straight alpha,
# then an all-zero 1-bpp AND mask); the hotspot is the arrow tip.
function Save-Cur($bitmaps, $path) {
    $images = foreach ($bmp in $bitmaps) {
        $w = $bmp.Width; $h = $bmp.Height
        $maskStride = [int]([Math]::Ceiling($w / 32.0) * 4)
        $ms = New-Object IO.MemoryStream
        $bw = New-Object IO.BinaryWriter $ms
        $bw.Write([uint32]40); $bw.Write([int32]$w); $bw.Write([int32]($h * 2))
        $bw.Write([uint16]1); $bw.Write([uint16]32); $bw.Write([uint32]0)
        $bw.Write([uint32]($w * $h * 4 + $maskStride * $h))
        $bw.Write([int32]0); $bw.Write([int32]0); $bw.Write([uint32]0); $bw.Write([uint32]0)
        for ($y = $h - 1; $y -ge 0; $y--) {
            for ($x = 0; $x -lt $w; $x++) {
                $c = $bmp.GetPixel($x, $y)
                $bw.Write([byte]$c.B); $bw.Write([byte]$c.G); $bw.Write([byte]$c.R); $bw.Write([byte]$c.A)
            }
        }
        $bw.Write((New-Object byte[] ($maskStride * $h)))
        $bw.Flush()
        , $ms.ToArray()
    }
    $out = New-Object IO.MemoryStream
    $bw = New-Object IO.BinaryWriter $out
    $bw.Write([uint16]0); $bw.Write([uint16]2); $bw.Write([uint16]$bitmaps.Count)
    $offset = 6 + 16 * $bitmaps.Count
    for ($i = 0; $i -lt $bitmaps.Count; $i++) {
        $w = $bitmaps[$i].Width
        $hot = [uint16][Math]::Round($w / 32.0)
        $bw.Write([byte]($w % 256)); $bw.Write([byte]($w % 256)); $bw.Write([byte]0); $bw.Write([byte]0)
        $bw.Write([uint16]$hot); $bw.Write([uint16]$hot)
        $bw.Write([uint32]$images[$i].Length); $bw.Write([uint32]$offset)
        $offset += $images[$i].Length
    }
    foreach ($img in $images) { $bw.Write([byte[]]$img) }
    $bw.Flush()
    [IO.File]::WriteAllBytes($path, $out.ToArray())
}

$cursorDir = Join-Path $modDir 'cursor'
New-Item -ItemType Directory -Force $cursorDir | Out-Null
$cursors = @(32, 48, 64, 96 | ForEach-Object { Draw-Cursor $_ })
Save-Cur $cursors (Join-Path $cursorDir 'eras_cursor.cur')
if ($env:ERAS_CURSOR_PREVIEW) { $cursors[3].Save($env:ERAS_CURSOR_PREVIEW, [Drawing.Imaging.ImageFormat]::Png) } # dev preview
$cursors | ForEach-Object { $_.Dispose() }
(Get-Item (Join-Path $cursorDir 'eras_cursor.cur')).FullName

