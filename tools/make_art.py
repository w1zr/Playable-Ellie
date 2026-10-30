"""Draws the Playable Ellie (WORKING) artwork (original, generated in code;
fonts are the Windows system fonts Segoe UI / Segoe UI Semibold / Bold).

The emblem: a fern frond inside a ring of two arrows (swap), on a dark teal
tile with a soft mint accent, so it sits next to w1z's menu (dark tile,
amber) and the TLOU Outfit Changer (amber coin, hanger) without looking like
either. Other palettes are kept for comparison: --preview <png> draws them all.

Writes:
  art/logo.png                   512x512 emblem
  package/fluffy/screenshot.png  1280x720 Fluffy Mod Manager preview
  nexus/images/00-header.png     2600x744 Nexus header
  nexus/images/01-logo.png       512x512 (same as art/logo.png)

usage: python tools/make_art.py [--preview out.png]   (needs Pillow)
"""
import math
import os

from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS = os.path.join(os.environ.get("SystemRoot", r"C:\Windows"), "Fonts")

PALETTES = {
    # name: background, tile, tile edge, text, dim text, accent, glow, tag fill, tag text
    "teal": dict(BG=(9, 14, 16), PANEL=(14, 26, 28), LINE=(38, 62, 64), TEXT=(238, 242, 240), DIM=(138, 158, 158),
                 ACC=(110, 201, 186), GLOW=(40, 110, 104), TAG=(110, 201, 186), TAGT=(9, 14, 16)),
    "ember": dict(BG=(14, 11, 10), PANEL=(28, 22, 20), LINE=(66, 52, 46), TEXT=(242, 236, 230), DIM=(160, 146, 136),
                  ACC=(232, 138, 92), GLOW=(120, 60, 36), TAG=(232, 138, 92), TAGT=(14, 11, 10)),
    "dusk": dict(BG=(11, 12, 18), PANEL=(22, 24, 36), LINE=(52, 56, 80), TEXT=(240, 240, 246), DIM=(146, 148, 170),
                 ACC=(160, 150, 230), GLOW=(72, 62, 140), TAG=(160, 150, 230), TAGT=(11, 12, 18)),
}
PALETTE = os.environ.get("PE_PALETTE", "teal")


def use(name):
    global BG, PANEL, LINE, TEXT, DIM, GREEN, GREEN_D, AMBER, TAGT
    p = PALETTES[name]
    BG, PANEL, LINE, TEXT, DIM = p["BG"], p["PANEL"], p["LINE"], p["TEXT"], p["DIM"]
    GREEN, GREEN_D, AMBER, TAGT = p["ACC"], p["GLOW"], p["TAG"], p["TAGT"]


use(PALETTE)

S = 3  # supersampling


def font(name, size):
    return ImageFont.truetype(os.path.join(FONTS, name), int(size * S))


def bezier(p0, p1, p2, t):
    a = (1 - t) ** 2
    b = 2 * (1 - t) * t
    c = t * t
    return (a * p0[0] + b * p1[0] + c * p2[0], a * p0[1] + b * p1[1] + c * p2[1])


def bezier_tan(p0, p1, p2, t):
    return (2 * (1 - t) * (p1[0] - p0[0]) + 2 * t * (p2[0] - p1[0]),
            2 * (1 - t) * (p1[1] - p0[1]) + 2 * t * (p2[1] - p1[1]))


def leaf(d, base, angle, length, width, fill):
    """A pointed leaflet from `base`, pointing along `angle` (radians)."""
    ca, sa = math.cos(angle), math.sin(angle)
    pts = []
    n = 24
    for i in range(n + 1):  # one side
        t = i / n
        x = t * length
        y = math.sin(math.pi * t) ** 0.8 * width * (1 - 0.35 * t)
        pts.append((x, y))
    for i in range(n, -1, -1):  # other side
        t = i / n
        x = t * length
        y = -math.sin(math.pi * t) ** 0.8 * width * (1 - 0.35 * t)
        pts.append((x, y))
    d.polygon([(base[0] + x * ca - y * sa, base[1] + x * sa + y * ca) for x, y in pts], fill=fill)


def fern(d, cx, cy, size, fill, vein):
    """A fern frond: a gently curved stem with paired, separated leaflets that
    shrink towards the tip."""
    p0 = (cx - size * 0.06, cy + size * 0.50)
    p1 = (cx - size * 0.20, cy - size * 0.02)
    p2 = (cx + size * 0.10, cy - size * 0.48)
    pts = [bezier(p0, p1, p2, i / 60) for i in range(61)]
    d.line(pts, fill=fill, width=max(2, int(size * 0.03)), joint="curve")
    pairs = 8
    for i in range(pairs):
        t = 0.16 + 0.76 * i / (pairs - 1)
        bx, by = bezier(p0, p1, p2, t)
        tx, ty = bezier_tan(p0, p1, p2, t)
        a = math.atan2(ty, tx)
        k = i / (pairs - 1)
        L = size * (0.30 - 0.20 * k)
        W = L * 0.24
        leaf(d, (bx, by), a - 0.95, L, W, fill)
        leaf(d, (bx, by), a + 0.95, L, W, fill)
    tx, ty = bezier_tan(p0, p1, p2, 1.0)
    leaf(d, p2, math.atan2(ty, tx), size * 0.11, size * 0.03, fill)


def arrow_ring(d, cx, cy, r, width, col):
    """Two arcs chasing each other, each ending in an arrowhead: the swap."""
    for start in (200, 20):
        span = 125
        box = [cx - r, cy - r, cx + r, cy + r]
        d.arc(box, start, start + span, fill=col, width=width)
        a = math.radians(start + span)
        hx, hy = cx + r * math.cos(a), cy + r * math.sin(a)
        tang = a + math.pi / 2  # direction of travel (clockwise in screen space)
        hl = width * 2.4
        tip = (hx + math.cos(tang) * hl * 0.9, hy + math.sin(tang) * hl * 0.9)
        nx, ny = math.cos(a), math.sin(a)
        left = (hx + nx * hl * 0.75, hy + ny * hl * 0.75)
        right = (hx - nx * hl * 0.75, hy - ny * hl * 0.75)
        d.polygon([tip, left, right], fill=col)


def emblem(size):
    W = size * S
    img = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    r = int(W * 0.18)
    d.rounded_rectangle([0, 0, W - 1, W - 1], radius=r, fill=PANEL + (255,), outline=LINE + (255,), width=max(2, W // 150))
    # soft green glow behind the frond
    glow = Image.new("RGBA", (W, W), (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([W * 0.22, W * 0.22, W * 0.78, W * 0.78], fill=GREEN_D + (70,))
    glow = glow.filter(ImageFilter.GaussianBlur(W * 0.08))
    img.alpha_composite(glow)
    d = ImageDraw.Draw(img)
    arrow_ring(d, W / 2, W / 2, W * 0.335, int(W * 0.038), GREEN + (255,))
    fern(d, W / 2, W / 2 + W * 0.01, W * 0.46, TEXT + (255,), PANEL + (255,))
    return img.resize((size, size), Image.LANCZOS)


def spaced(d, xy, text, f, fill, spacing, anchor_center=False):
    widths = [d.textlength(ch, font=f) for ch in text]
    total = sum(widths) + spacing * (len(text) - 1)
    x, y = xy
    if anchor_center:
        x -= total / 2
    for ch, w in zip(text, widths):
        d.text((x, y), ch, font=f, fill=fill)
        x += w + spacing
    return total


def backdrop(w, h):
    img = Image.new("RGBA", (w * S, h * S), BG + (255,))
    glow = Image.new("RGBA", img.size, (0, 0, 0, 0))
    gd = ImageDraw.Draw(glow)
    gd.ellipse([-w * S * 0.1, -h * S * 0.4, w * S * 0.55, h * S * 1.3], fill=GREEN_D + (38,))
    glow = glow.filter(ImageFilter.GaussianBlur(h * S * 0.25))
    img.alpha_composite(glow)
    return img


def header(w, h):
    img = backdrop(w, h)
    d = ImageDraw.Draw(img)
    e = h * 0.62
    em = emblem(int(e)).resize((int(e * S), int(e * S)), Image.LANCZOS)
    ex, ey = int(w * S * 0.09), int((h * S - e * S) / 2)
    img.alpha_composite(em, (ex, ey))
    tx = ex + e * S + w * S * 0.045
    f1 = font("seguisb.ttf", h * 0.17)
    f2 = font("segoeui.ttf", h * 0.055)
    f3 = font("seguibl.ttf" if os.path.exists(os.path.join(FONTS, "seguibl.ttf")) else "segoeuib.ttf", h * 0.07)
    y1 = h * S * 0.24
    tw = spaced(d, (tx, y1), "PLAYABLE ELLIE", f1, TEXT, h * S * 0.012)
    # "WORKING" tag
    tag = "WORKING"
    tagw = spaced(ImageDraw.Draw(Image.new("RGBA", (1, 1))), (0, 0), tag, f3, TEXT, h * S * 0.012)
    pad = h * S * 0.025
    ty = y1 + h * S * 0.235
    d.rounded_rectangle([tx, ty, tx + tagw + pad * 2, ty + h * S * 0.11], radius=h * S * 0.018, fill=AMBER)
    spaced(d, (tx + pad, ty + h * S * 0.008), tag, f3, TAGT, h * S * 0.012)
    d.line([(tx, ty + h * S * 0.17), (tx + tw, ty + h * S * 0.17)], fill=GREEN, width=int(h * S * 0.006))
    spaced(d, (tx, ty + h * S * 0.205), "PLAY JOEL'S CHAPTERS AS ELLIE  \u00b7  THE LAST OF US PART I", f2, DIM, h * S * 0.012)
    return img.resize((w, h), Image.LANCZOS)


def screenshot():
    """1280x720 Fluffy preview: header on top, what the mod does below."""
    w, h = 1280, 720
    img = backdrop(w, h)
    d = ImageDraw.Draw(img)
    e = 230
    em = emblem(e).resize((e * S, e * S), Image.LANCZOS)
    ex, ey = 70 * S, 60 * S
    img.alpha_composite(em, (ex, ey))
    tx = ex + e * S + 50 * S
    f1 = font("seguisb.ttf", 66)
    f3 = font("segoeuib.ttf", 26)
    f2 = font("segoeui.ttf", 22)
    tw = spaced(d, (tx, 78 * S), "PLAYABLE ELLIE", f1, TEXT, 5 * S)
    tag = "WORKING"
    tagw = spaced(ImageDraw.Draw(Image.new("RGBA", (1, 1))), (0, 0), tag, f3, TEXT, 4 * S)
    ty = 178 * S
    d.rounded_rectangle([tx, ty, tx + tagw + 28 * S, ty + 42 * S], radius=6 * S, fill=AMBER)
    spaced(d, (tx + 14 * S, ty + 3 * S), tag, f3, TAGT, 4 * S)
    d.line([(tx, ty + 66 * S), (tx + tw, ty + 66 * S)], fill=GREEN, width=3 * S)
    spaced(d, (tx, ty + 80 * S), "THE LAST OF US PART I  \u00b7  PC", f2, DIM, 4 * S)

    items = [
        ("Ellie in Joel's chapters", "In the outfit for the chapter"),
        ("Moves, aims, shoots, reloads", "Her own animations, streamed in"),
        ("Crafting in every chapter", "With every recipe"),
        ("Joel's 9mm held straight", "With proper recoil"),
        ("Joel as her companion", "Optional, on by default"),
        ("F10 to switch in game", "On-screen loading and swapped messages"),
    ]
    fb = font("seguisb.ttf", 25)
    fs = font("segoeui.ttf", 19)
    x0, y0, colw, rowh = 90 * S, 380 * S, 560 * S, 92 * S
    for i, (a, b) in enumerate(items):
        x = x0 + (i % 2) * colw
        y = y0 + (i // 2) * rowh
        d.rounded_rectangle([x, y + 6 * S, x + 5 * S, y + 62 * S], radius=2 * S, fill=GREEN)
        d.text((x + 24 * S, y), a, font=fb, fill=TEXT)
        d.text((x + 24 * S, y + 36 * S), b, font=fs, fill=DIM)
    return img.resize((w, h), Image.LANCZOS)


def preview(path):
    """All palettes side by side (logo + header strip), to choose from."""
    rows = []
    for name in PALETTES:
        use(name)
        h = header(1300, 372)
        rows.append(h)
    out = Image.new("RGB", (1300, 372 * len(rows) + 30 * (len(rows) - 1)), (255, 255, 255))
    for i, r in enumerate(rows):
        out.paste(r.convert("RGB"), (0, i * (372 + 30)))
    out.save(path)
    use(PALETTE)


def main():
    import sys
    if len(sys.argv) > 2 and sys.argv[1] == "--preview":
        preview(sys.argv[2])
        print("preview written")
        return
    for sub in ("art", "nexus/images", "package/fluffy"):
        os.makedirs(os.path.join(ROOT, sub), exist_ok=True)
    logo = emblem(512)
    logo.save(os.path.join(ROOT, "art", "logo.png"))
    logo.save(os.path.join(ROOT, "nexus", "images", "01-logo.png"))
    screenshot().convert("RGB").save(os.path.join(ROOT, "package", "fluffy", "screenshot.png"))
    header(2600, 744).convert("RGB").save(os.path.join(ROOT, "nexus", "images", "00-header.png"))
    print("art written")


if __name__ == "__main__":
    main()
