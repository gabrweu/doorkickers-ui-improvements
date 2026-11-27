# Draws the mod's own art, in the colors of the game's menus (cream f0e3cc / dark 40392b / orange f97b03):
#   mod/mod_image.jpg                                       the Mods menu / Workshop thumbnail: a grid of rifle
#                                                           silhouettes under header bars, one marked as equipped
#   mod/cursor/eras_cursor.cur                              the mouse cursor: a cream arrow with a dark outline and an
#                                                           orange tip, at 32/48/64/96 px (the plugin picks one by
#                                                           screen height)
#   mod/textures/gui/dk2ml_ps_open_normal.tga, _hover.tga   the portrait picker's "choose portrait" button (48x48,
#                                                           a ring and 2x2 head-and-shoulders figures: a clear cream
#                                                           (the UI's white) outline at rest, and the stock (i)'s
#                                                           hover, a black disc with them in cream),
#                                                           uncompressed 32-bit TGA with alpha like the stock icons
#   mod/textures/gui/dk2ml_lo_lock_open.tga, _closed.tga    the loadouts' slot padlocks (40x40): unlocked, a flat open
#                                                           padlock in cream (tinted faint in XML); locked, a closed
#                                                           orange padlock on a dark chip with a thin cream frame
#   mod/textures/gui/dk2ml_wm_door.tga                      the action wheel's door badge (48x48): the wheel's
#                                                           door-opening icon redrawn with an orange arrow, on a dark
#                                                           disc with a cream rim, marking the outer ring's buttons
#                                                           that use an item on the target
#   mod/textures/gui/dk2ml_wm_ring.tga                      a quarter of the wheel's outer ring (640x640: 320 GUI
#                                                           units at 2x): a dotted cream circle (4.5-unit dots ~12
#                                                           apart), flipped into four in XML
#   mod/textures/gui/dk2ml_wm_ring_inner.tga                the same for the inner ring (368x368), shown instead of
#                                                           the stock ring (radius 180, dotted)
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

# A disc filling the image in `disc` (Transparent: none), a ring along its edge and a glyph in `ink`.
# The glyph is a 2x2 grid of head-and-shoulders figures.
function Draw-PortraitButton($size, $ink, $disc) {
    $scale = 4 # drawn large and scaled down, for smooth edges
    $s = $size * $scale
    $big = New-Object Drawing.Bitmap $s, $s, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($big)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([Drawing.Color]::Transparent)
    if ($disc.A -gt 0) { $g.FillEllipse((New-Object Drawing.SolidBrush $disc), 0, 0, $s, $s) }
    $ring = $s * 4 / 48
    $pen = New-Object Drawing.Pen $ink, ([single]$ring)
    $g.DrawEllipse($pen, [single]($ring / 2), [single]($ring / 2), [single]($s - $ring), [single]($s - $ring))
    $fig = New-Object Drawing.SolidBrush $ink
    $box = $s * 0.6 # the 2x2 grid, centered inside the ring
    $gap = $s * 0.06
    $cell = ($box - $gap) / 2
    $x0 = ($s - $box) / 2; $y0 = ($s - $box) / 2
    for ($row = 0; $row -lt 2; $row++) {
        for ($col = 0; $col -lt 2; $col++) {
            $x = $x0 + $col * ($cell + $gap)
            $y = $y0 + $row * ($cell + $gap)
            # head and shoulders, the shoulders cut off at the cell's bottom
            $g.SetClip((New-Object Drawing.RectangleF ([single]$x), ([single]$y), ([single]$cell), ([single]$cell)))
            $head = $cell * 0.46
            $g.FillEllipse($fig, [single]($x + ($cell - $head) / 2), [single]$y, [single]$head, [single]$head)
            $g.FillEllipse($fig, [single]($x + $cell * 0.06), [single]($y + $cell * 0.56), [single]($cell * 0.88), [single]($cell * 0.9))
            $g.ResetClip()
        }
    }
    $small = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gs = [Drawing.Graphics]::FromImage($small)
    $gs.InterpolationMode = 'HighQualityBicubic'
    $gs.PixelOffsetMode = 'HighQuality'
    $gs.Clear([Drawing.Color]::Transparent)
    $gs.DrawImage($big, 0, 0, $size, $size)
    $g.Dispose(); $gs.Dispose(); $big.Dispose(); $pen.Dispose()
    return $small
}

# Uncompressed 32-bit TGA like the stock icons: BGRA, straight alpha, bottom-up rows (descriptor 0x08: 8 alpha bits).
function Save-Tga($bitmap, $path) {
    $w = $bitmap.Width; $h = $bitmap.Height
    $ms = New-Object IO.MemoryStream
    $bw = New-Object IO.BinaryWriter $ms
    $bw.Write([byte]0); $bw.Write([byte]0); $bw.Write([byte]2) # no id, no color map, uncompressed true color
    $bw.Write((New-Object byte[] 9))                           # color map spec, x/y origin
    $bw.Write([uint16]$w); $bw.Write([uint16]$h)
    $bw.Write([byte]32); $bw.Write([byte]0x08)
    $data = $bitmap.LockBits((New-Object Drawing.Rectangle 0, 0, $w, $h), 'ReadOnly', [Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $row = New-Object byte[] ($w * 4)
    for ($y = $h - 1; $y -ge 0; $y--) {
        [Runtime.InteropServices.Marshal]::Copy([IntPtr]($data.Scan0.ToInt64() + $y * $data.Stride), $row, 0, $row.Length)
        $bw.Write($row)
    }
    $bitmap.UnlockBits($data)
    $bw.Flush()
    [IO.File]::WriteAllBytes($path, $ms.ToArray())
}

$texDir = Join-Path $modDir 'textures\gui'
New-Item -ItemType Directory -Force $texDir | Out-Null
# normal: a clear cream outline (90%), no disc; hover: the stock (i)'s hover, a black disc
$normal = Draw-PortraitButton 48 ([Drawing.Color]::FromArgb(230, $cream)) ([Drawing.Color]::Transparent)
Save-Tga $normal (Join-Path $texDir 'dk2ml_ps_open_normal.tga')
$hover = Draw-PortraitButton 48 $cream ([Drawing.Color]::Black)
Save-Tga $hover (Join-Path $texDir 'dk2ml_ps_open_hover.tga')
if ($env:ERAS_BUTTON_PREVIEW) { $normal.Save($env:ERAS_BUTTON_PREVIEW, [Drawing.Imaging.ImageFormat]::Png) } # dev preview
$normal.Dispose(); $hover.Dispose()

# --- the slot padlocks (loadouts) ---

# A flat padlock in a 40-unit grid, square corners like the game's icons: a body with a keyhole slot and a shackle
# (open: lifted, its left leg out of the body). chip: on a dark square with a thin cream frame (like the small NVG
# slot next to the helmet), the padlock smaller and orange; else cream on transparent (tinted in XML).
function Draw-Padlock($size, $open, $chip) {
    $scale = 4 # drawn large and scaled down, for smooth edges
    $s = $size * $scale
    $big = New-Object Drawing.Bitmap $s, $s, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($big)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([Drawing.Color]::Transparent)
    $u = $s / 40
    $k = 1.0
    $ink = $cream
    $hole = [Drawing.Color]::Transparent
    if ($chip) {
        $back = [Drawing.Color]::FromArgb(235, 0x0c, 0x0b, 0x0b)
        $g.FillRectangle((New-Object Drawing.SolidBrush $back), 0, 0, $s, $s)
        $frame = 2 * $u
        $g.DrawRectangle((New-Object Drawing.Pen $cream, ([single]$frame)), [single]($frame / 2), [single]($frame / 2), [single]($s - $frame), [single]($s - $frame))
        $k = 0.7
        $ink = $orange
        $hole = $back
    }
    $o = 20 * $u * (1 - $k) # scaled about the center
    $P = { param($v) [single]($o + $v * $u * $k) }
    $L = { param($v) [single]($v * $u * $k) }
    $brush = New-Object Drawing.SolidBrush $ink
    $pen = New-Object Drawing.Pen $ink, (& $L 3.6)
    # shackle: an arch over the body, legs at x 14 and 26
    $lift = if ($open) { 3 } else { 0 }
    $g.DrawArc($pen, (& $P 14), (& $P (7 - $lift)), (& $L 12), (& $L 12), 180, 180)
    $g.DrawLine($pen, (& $P 26), (& $P (13 - $lift)), (& $P 26), (& $P 19.5))
    $leftEnd = if ($open) { 12 } else { 19.5 }
    $g.DrawLine($pen, (& $P 14), (& $P (13 - $lift)), (& $P 14), (& $P $leftEnd))
    # body, then the keyhole slot cut out of it
    $g.FillRectangle($brush, (& $P 10), (& $P 19), (& $L 20), (& $L 15))
    $g.CompositingMode = 'SourceCopy'
    $g.FillRectangle((New-Object Drawing.SolidBrush $hole), (& $P 18.5), (& $P 23), (& $L 3), (& $L 6.5))
    $g.CompositingMode = 'SourceOver'

    $small = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gs = [Drawing.Graphics]::FromImage($small)
    $gs.InterpolationMode = 'HighQualityBicubic'
    $gs.PixelOffsetMode = 'HighQuality'
    $gs.Clear([Drawing.Color]::Transparent)
    $gs.DrawImage($big, 0, 0, $size, $size)
    $g.Dispose(); $gs.Dispose(); $big.Dispose(); $pen.Dispose(); $brush.Dispose()
    return $small
}

$lockOpen = Draw-Padlock 40 $true $false
Save-Tga $lockOpen (Join-Path $texDir 'dk2ml_lo_lock_open.tga')
$lockClosed = Draw-Padlock 40 $false $true
Save-Tga $lockClosed (Join-Path $texDir 'dk2ml_lo_lock_closed.tga')
$lockOpen.Dispose(); $lockClosed.Dispose()

# --- the action wheel's door badge ---

# The door-opening icon of the wheel's own buttons, redrawn as a badge in a 48-unit grid, for the outer ring's equipment
# buttons (the item used on the door / target): a dark disc with a thin cream rim; a cream door frame (a bracket on the
# left) with the door swung open towards you (a panel, its far edge lower, a dark knob); and an orange arrow sweeping
# clockwise over the top right, where the stock icon's is blue. Our own drawing: the game's icon can't be recoloured
# (no game art in the repo, and an XML tint only multiplies, so blue can't become orange).
function Draw-DoorBadge($size) {
    $scale = 4 # drawn large and scaled down, for smooth edges
    $s = $size * $scale
    $big = New-Object Drawing.Bitmap $s, $s, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($big)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([Drawing.Color]::Transparent)
    $u = $s / 48
    $P = { param($v) [single]($v * $u) }
    $Pt = { param($x, $y) New-Object Drawing.PointF (& $P $x), (& $P $y) }
    $g.FillEllipse((New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(245, $dark))), (& $P 1), (& $P 1), (& $P 46), (& $P 46))
    $g.DrawEllipse((New-Object Drawing.Pen $cream, (& $P 2.5)), (& $P 2.25), (& $P 2.25), (& $P 43.5), (& $P 43.5))

    # the frame: a bracket on the left
    $framePen = New-Object Drawing.Pen $cream, (& $P 2.6)
    $framePen.LineJoin = 'Miter'
    $g.DrawLines($framePen, [Drawing.PointF[]]@((& $Pt 17 15), (& $Pt 11.5 15), (& $Pt 11.5 37), (& $Pt 17 37)))
    # the door, hinged inside the frame (a dark gap keeps the bracket visible), swung open towards you: its far edge
    # drops and runs past the frame's bottom
    $door = [Drawing.PointF[]]@((& $Pt 16 15.5), (& $Pt 26 19), (& $Pt 26 40), (& $Pt 16 36.5))
    $g.FillPolygon((New-Object Drawing.SolidBrush $cream), $door)
    $g.FillEllipse((New-Object Drawing.SolidBrush $dark), (& $P 22), (& $P 27.5), (& $P 2.6), (& $P 2.6))

    # the arrow: clockwise over the top right (GDI angles run clockwise, y down), its head pointing down at the end
    $cx = 25; $cy = 23; $r = 12; $from = 238; $sweep = 108
    $arrowPen = New-Object Drawing.Pen $orange, (& $P 3.4)
    $g.DrawArc($arrowPen, (& $P ($cx - $r)), (& $P ($cy - $r)), (& $P (2 * $r)), (& $P (2 * $r)), [single]$from, [single]$sweep)
    $t = ($from + $sweep) * [Math]::PI / 180
    $ex = $cx + $r * [Math]::Cos($t); $ey = $cy + $r * [Math]::Sin($t)
    $dx = -[Math]::Sin($t); $dy = [Math]::Cos($t)   # the direction of travel (clockwise)
    $nx = [Math]::Cos($t); $ny = [Math]::Sin($t)    # outwards
    $head = [Drawing.PointF[]]@(
        (& $Pt ($ex + 5.5 * $dx) ($ey + 5.5 * $dy)),
        (& $Pt ($ex - 1 * $dx + 4.5 * $nx) ($ey - 1 * $dy + 4.5 * $ny)),
        (& $Pt ($ex - 1 * $dx - 4.5 * $nx) ($ey - 1 * $dy - 4.5 * $ny)))
    $g.FillPolygon((New-Object Drawing.SolidBrush $orange), $head)

    $small = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gs = [Drawing.Graphics]::FromImage($small)
    $gs.InterpolationMode = 'HighQualityBicubic'
    $gs.PixelOffsetMode = 'HighQuality'
    $gs.Clear([Drawing.Color]::Transparent)
    $gs.DrawImage($big, 0, 0, $size, $size)
    $g.Dispose(); $gs.Dispose(); $big.Dispose(); $framePen.Dispose(); $arrowPen.Dispose()
    return $small
}

$badge = Draw-DoorBadge 48
Save-Tga $badge (Join-Path $texDir 'dk2ml_wm_door.tga')
if ($env:ERAS_BADGE_PREVIEW) {
    $badge.Save($env:ERAS_BADGE_PREVIEW, [Drawing.Imaging.ImageFormat]::Png) # dev preview, and a 4x one to judge it by
    $zoom = New-Object Drawing.Bitmap 192, 192
    $gz = [Drawing.Graphics]::FromImage($zoom); $gz.InterpolationMode = 'NearestNeighbor'; $gz.PixelOffsetMode = 'Half'
    $gz.DrawImage($badge, 0, 0, 192, 192); $gz.Dispose()
    $zoom.Save(($env:ERAS_BADGE_PREVIEW -replace '\.png$', '_4x.png'), [Drawing.Imaging.ImageFormat]::Png); $zoom.Dispose()
}
$badge.Dispose()

# --- the action wheel's outer ring ---

# One quarter of the merged wheel's outer ring (the top right: the ring's centre is the image's bottom-left corner;
# gui/action_wheel.xml flips it into the other three), in GUI units drawn at 2 px each (shown at scale 0.5, sharp on
# high-resolution screens). One dotted line, in the stock ring's colour (ffe3ce) at its dashed line's place (7 inside
# the buttons): here at the outer buttons' radius 315 - 7. Round dots, $dot units across, spaced well apart (the stock
# dotted line has ~3 px dots ~2.5 apart). $count dots fill the quarter evenly, with half a spacing at each end, so the
# mirrored quarters meet with the same spacing.
function Draw-RingQuarter($units, $radius, $dot, $count) {
    $px = 2     # final px per GUI unit
    $scale = 4  # drawn large and scaled down, for smooth edges
    $s = $units * $px * $scale
    $big = New-Object Drawing.Bitmap $s, $s, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g = [Drawing.Graphics]::FromImage($big)
    $g.SmoothingMode = 'AntiAlias'
    $g.Clear([Drawing.Color]::Transparent)
    $u = $px * $scale
    $brush = New-Object Drawing.SolidBrush ([Drawing.Color]::FromArgb(0xff, 0xe3, 0xce))
    $r = $radius * $u
    $d = $dot * $u
    $step = ([Math]::PI / 2) / $count              # radians
    for ($i = 0; $i -lt $count; $i++) {
        $a = ($i + 0.5) * $step                    # counter-clockwise from +x (up is +y); the image's y runs down
        $x = $r * [Math]::Cos($a); $y = $s - $r * [Math]::Sin($a)
        $g.FillEllipse($brush, [single]($x - $d / 2), [single]($y - $d / 2), [single]$d, [single]$d)
    }

    $size = $units * $px
    $small = New-Object Drawing.Bitmap $size, $size, ([Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $gs = [Drawing.Graphics]::FromImage($small)
    $gs.InterpolationMode = 'HighQualityBicubic'
    $gs.PixelOffsetMode = 'HighQuality'
    $gs.Clear([Drawing.Color]::Transparent)
    $gs.DrawImage($big, 0, 0, $size, $size)
    $g.Dispose(); $gs.Dispose(); $big.Dispose(); $brush.Dispose()
    return $small
}

# 320 units: the ring (308 + 2.25) and a little margin; 40 dots a quarter, 4.5 across (circumference 1935: 12.1 apart)
$ring = Draw-RingQuarter 320 308 4.5 40
Save-Tga $ring (Join-Path $texDir 'dk2ml_wm_ring.tga')
if ($env:ERAS_RING_PREVIEW) { $ring.Save($env:ERAS_RING_PREVIEW, [Drawing.Imaging.ImageFormat]::Png) } # dev preview
$ring.Dispose()
# the inner ring, in place of the stock one: at the stock dashed line's radius 180, 184 units (368 px), 24 dots a
# quarter (circumference 1131: 11.8 apart)
$ringInner = Draw-RingQuarter 184 180 4.5 24
Save-Tga $ringInner (Join-Path $texDir 'dk2ml_wm_ring_inner.tga')
if ($env:ERAS_RING_INNER_PREVIEW) { $ringInner.Save($env:ERAS_RING_INNER_PREVIEW, [Drawing.Imaging.ImageFormat]::Png) } # dev preview
$ringInner.Dispose()

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

