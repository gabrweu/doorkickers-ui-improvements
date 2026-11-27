# Makes mod/textures/gui/dk2ml_tt_bg.tga: Trooper Toolbox's main-page background (customization_menu_bg.tga, art by
# Sly, shipped with permission) with its baked-in slot frames painted out. With TT's pages merged, the plugin
# (slots/ttbg.cpp) shows it in place of TT's and frames each visible slot itself, so a hidden slot leaves no empty box.
# Re-run it if Trooper Toolbox changes its background.
#
#   python -I tools\clean_tt_bg.py [TT's customization_menu_bg.tga] [output .tga]
#
# A frame edge is a thin straight line: a run of one kind of pixel (cream on black or on the figure; black on the
# cream plate carrier) at least MINL long and, for most of its length, at most MAXT thick across. The whole run,
# corners and soft edges included, is filled from the pixels just outside it, each half from its own side, so the
# figure continues through where a line crossed it. Horizontal lines first, then vertical ones (on the transposed
# image); then the few soft-edge specks left at corners inside the cream are filled from the cream around them.
# The output has TT's own format (uncompressed 32-bit, rows bottom-up), which the game already loads for TT.
# Python 3, standard library only.
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DEFAULT_SRC = r"C:\Program Files (x86)\Steam\steamapps\workshop\content\1239080\3299377664\textures\gui\customization_menu_bg.tga"
DEFAULT_OUT = os.path.join(HERE, "..", "mod", "textures", "gui", "dk2ml_tt_bg.tga")

MAXT, MINL, PAD, THIN = 10, 80, 3, 0.8


def read_tga(path):
    """Returns w, h, bytes per pixel, rows top-down (BGR(A) bytearrays). Uncompressed or RLE, either origin."""
    d = open(path, "rb").read()
    kind = d[2]
    w, h = struct.unpack("<HH", d[12:16])
    bp = d[16] // 8
    off = 18 + d[0]
    n = w * h * bp
    if kind in (2, 3):
        px = d[off:off + n]
    elif kind in (10, 11):
        out, i = bytearray(), off
        while len(out) < n:
            c = d[i]
            i += 1
            count = (c & 0x7F) + 1
            if c & 0x80:
                out += d[i:i + bp] * count
                i += bp
            else:
                out += d[i:i + bp * count]
                i += bp * count
        px = bytes(out[:n])
    else:
        sys.exit("unsupported TGA type %d" % kind)
    rows = [bytearray(px[r * w * bp:(r + 1) * w * bp]) for r in range(h)]
    if not d[17] & 0x20:  # bottom-left origin
        rows.reverse()
    return w, h, bp, rows


def write_tga(path, w, h, bp, rows):
    """Uncompressed, rows bottom-up (descriptor: alpha bits, bottom-left origin), as TT's own file."""
    header = struct.pack("<BBBHHBHHHHBB", 0, 0, 2, 0, 0, 0, 0, 0, w, h, bp * 8, 8 if bp == 4 else 0)
    with open(path, "wb") as f:
        f.write(header)
        for r in reversed(rows):
            f.write(r)


def transpose(g, gw, gh, bp):
    t = [bytearray(gh * bp) for _ in range(gw)]
    for y in range(gh):
        r = g[y]
        for x in range(gw):
            t[x][y * bp:(y + 1) * bp] = r[x * bp:(x + 1) * bp]
    return t


def hlines(g, gw, gh, bp, test, sides):
    """Paints out the horizontal lines of pixels passing test; with sides, only lines with such pixels on both sides."""
    c = [bytearray(1 if test(g[y], x) else 0 for x in range(gw)) for y in range(gh)]
    vtop = [[0] * gw for _ in range(gh)]  # where each pixel's vertical run starts, and its length
    vlen = [[0] * gw for _ in range(gh)]
    for x in range(gw):
        y = 0
        while y < gh:
            if c[y][x]:
                e = y
                while e < gh and c[e][x]:
                    e += 1
                for i in range(y, e):
                    vtop[i][x], vlen[i][x] = y, e - y
                y = e
            else:
                y += 1
    done = [bytearray(gw) for _ in range(gh)]
    lines = 0
    for y in range(gh):
        row, x = c[y], 0
        while x < gw:
            if not row[x]:
                x += 1
                continue
            e = x
            while e < gw and row[e]:
                e += 1
            x0, x1, x = x, e, e
            if x1 - x0 < MINL or all(done[y][i] for i in range(x0, x1)):
                continue
            thin = [i for i in range(x0, x1) if vlen[y][i] <= MAXT]
            if len(thin) < THIN * (x1 - x0):
                continue  # a filled area (the plate carrier, the helmet), not a line
            tops = sorted(vtop[y][i] for i in thin)
            bottoms = sorted(vtop[y][i] + vlen[y][i] - 1 for i in thin)
            ty0, ty1 = tops[len(tops) // 2], bottoms[len(bottoms) // 2]  # the line's usual extent, for its corners
            a, b = max(0, ty0 - PAD - 1), min(gh - 1, ty1 + PAD + 1)
            if sides and not (sides(g[a], (x0 + x1) // 2) and sides(g[b], (x0 + x1) // 2)):
                continue
            for i in range(max(0, x0 - PAD), min(gw, x1 + PAD)):
                if x0 <= i < x1 and vlen[y][i] <= MAXT:
                    y0, y1 = vtop[y][i], vtop[y][i] + vlen[y][i] - 1
                else:
                    y0, y1 = ty0, ty1
                aa, bb = max(0, y0 - PAD - 1), min(gh - 1, y1 + PAD + 1)
                top, bottom = bytes(g[aa][i * bp:(i + 1) * bp]), bytes(g[bb][i * bp:(i + 1) * bp])
                mid = (aa + bb) / 2
                for yy in range(aa + 1, bb):
                    g[yy][i * bp:(i + 1) * bp] = top if yy < mid else bottom
                for yy in range(y0, y1 + 1):
                    done[yy][i] = 1
            lines += 1
    return lines


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    out = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_OUT
    w, h, bp, px = read_tga(src)
    if bp < 3:
        sys.exit("expected a colour TGA")

    def cream(r, x):
        return r[x * bp + 2] > 170 and r[x * bp + 1] > 150  # red and green high (BGR order)

    def dark(r, x):
        return max(r[x * bp:x * bp + 3]) < 70

    def both(g, test, sides=None):
        n = hlines(g, w, h, bp, test, sides)
        t = transpose(g, w, h, bp)
        n += hlines(t, h, w, bp, test, sides)
        return transpose(t, h, w, bp), n

    for i in range(2):
        px, n = both(px, cream)
        print("cream lines, pass %d: %d" % (i + 1, n))
    for i in range(2):
        px, n = both(px, dark, sides=cream)  # the frame drawn in black on the plate carrier
        print("dark lines on cream, pass %d: %d" % (i + 1, n))

    # specks (a corner's soft edge, at most 4 px across) inside cream: where the pixels just left and right of a short
    # run are the same cream, the run takes their colour
    specks = 0
    for y in range(h):
        r = px[y]
        for x in range(1, w - 6):
            left = bytes(r[(x - 1) * bp:x * bp])
            if not cream(r, x - 1) or bytes(r[x * bp:(x + 1) * bp]) == left:
                continue
            for e in range(x + 1, x + 5):
                if bytes(r[e * bp:(e + 1) * bp]) == left:
                    for i in range(x, e):
                        r[i * bp:(i + 1) * bp] = left
                    specks += 1
                    break
    print("specks: %d" % specks)

    write_tga(out, w, h, bp, px)
    print(os.path.abspath(out))


if __name__ == "__main__":
    main()
