#!/usr/bin/env python3
"""Generate the RSFX MPC screen skin: a rack-style panel drawn by MPC OS from TUI.json + PNG filmstrips.

Output: mpc/skin/RSFX - VST - RSFX/{version.xml, Plugin Skins/{TUI.json, Q-Links.json, Q-Links - 8by1.json, *.png}}
Format follows the MPC OS plugin-skin layout used by the JV-880 package (and the GlueBus skin): components bind to
"Parameter N" = VST parameter index, knobs are vertical filmstrips. All artwork is original.

  python3 tools/make_skin.py                    build the skin
  python3 tools/make_skin.py --preview s.json out.png   render a preview from a plugin state dump (tests/dump_state)
Requires Pillow.
"""
import json, math, os, random, shutil, sys
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SKIN_NAME = "RSFX - VST - RSFX"
SKIN_DIR = os.path.join(ROOT, "mpc", "skin", SKIN_NAME)
OUT = os.path.join(SKIN_DIR, "Plugin Skins")
W, H = 1280, 628          # plugin view area used by the JV-880 / GlueBus skins
FRAMES, NUMFRAMES = 128, 127
SS = 3                    # supersampling

# ---------------------------------------------------------------- parameter indices (must match src/algos.h)
P = dict(algo=0, mix=1, decay=2, damp=3, pre=4, delay=5, fb=6, rate=7, depth=8, drive=9, freq=10, reso=11,
         thresh=12, pitch=13, bal=14, out=15, bypass=16, input=17, chain=18, inl=19, inr=20, outl=21, outr=22,
         led_rev=23, led_dly=24, led_mod=25, led_chr=26)

# ---------------------------------------------------------------- palette
CHAR   = (24, 25, 27)       # charcoal
GRAPH  = (38, 40, 43)       # graphite
ALU    = (52, 54, 58)       # module face
ALU_HI = (78, 80, 85)
EDGE_D = (12, 12, 14)
WHITE  = (232, 230, 224)    # primary labels
GREY   = (138, 140, 145)    # secondary labels
AMBER  = (255, 168, 56)
GREEN  = (86, 206, 108)
RED    = (232, 62, 48)
ICE    = (150, 210, 255)

FONT_DIRS = ["/usr/share/fonts/truetype/dejavu", "/usr/share/fonts/dejavu", "/Library/Fonts", "C:/Windows/Fonts"]
def font(name, size):
    for d in FONT_DIRS:
        p = os.path.join(d, name)
        if os.path.exists(p): return ImageFont.truetype(p, size)
    return ImageFont.load_default()
BOLD, COND, BOOK = "DejaVuSans-Bold.ttf", "DejaVuSansCondensed-Bold.ttf", "DejaVuSansCondensed.ttf"
def hexa(c, a=255): return "%02x%02x%02x%02x" % (a, c[0], c[1], c[2])     # TUI colour = AARRGGBB
def rgba(c, a=255): return (c[0], c[1], c[2], a)
def mixc(a, b, t): return tuple(int(a[i] + (b[i] - a[i]) * t) for i in range(3))
def spaced(s, n=1): return (" " * n).join(s)
def text_c(d, cx, cy, s, f, fill):
    b = d.textbbox((0, 0), s, font=f); d.text((cx - (b[2] - b[0]) / 2 - b[0], cy - (b[3] - b[1]) / 2 - b[1]), s, font=f, fill=fill)

# ---------------------------------------------------------------- layout
# knob sizes: image px (includes value ring), component box
KS = {"XL": dict(img=108, box=(150, 172), name_h=20, val_h=22),
      "L":  dict(img=92,  box=(128, 156), name_h=20, val_h=22),
      "M":  dict(img=76,  box=(108, 138), name_h=18, val_h=20),
      "S":  dict(img=60,  box=(82, 120),  name_h=16, val_h=18)}
# modules: key, title, x0, y0, x1, y1, led param
MODULES = [
    ("input",  "INPUT",       30,  58, 198, 290, None),
    ("display","ALGORITHM",  206,  58, 902, 290, None),
    ("output", "OUTPUT",     910,  58, 1250, 290, None),
    ("reverb", "REVERB",      30, 300, 382, 612, "led_rev"),
    ("delay",  "DELAY",      390, 300, 742, 612, "led_dly"),
    ("mod",    "MODULATION", 750, 300, 988, 612, "led_mod"),
    ("char",   "CHARACTER",  996, 300, 1250, 612, "led_chr"),
]
# knobs: param, label (MPC shows the parameter name; label is only for the preview), size, x, y
KNOBS = [
    ("input",  "M",  40,  98),
    ("algo",   "XL", 734, 92),
    ("mix",    "M",  922, 96),
    ("out",    "M",  1036, 96),
    ("decay",  "L",  44,  392), ("damp", "M", 178, 402), ("pre", "M", 274, 402),
    ("delay",  "L",  404, 392), ("fb",   "M", 538, 402), ("bal", "M", 634, 402),
    ("rate",   "M",  762, 402), ("depth","M", 868, 402),
    ("drive",  "S",  1004, 346), ("freq", "S", 1084, 346), ("reso", "S", 1164, 346),
    ("thresh", "S",  1044, 476), ("pitch","S", 1124, 476),
]
METERS = [("inl", 160, 98), ("inr", 176, 98), ("outl", 1178, 98), ("outr", 1194, 98)]
METER_W, METER_H = 12, 168
LCD = (226, 100, 714, 254)        # x0, y0, x1, y1
BYPASS = (950, 242, 200, 40)

# ---------------------------------------------------------------- knob filmstrip
A0, A1 = -135.0, 135.0
def ang_xy(cx, cy, r, deg):
    a = math.radians(deg - 90); return cx + r * math.cos(a), cy + r * math.sin(a)

def knob_frame(px, t):
    s = px * SS; im = Image.new("RGBA", (s, s), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    c = s / 2
    ring_r = s * 0.485; ring_w = max(2, int(s * 0.045))
    R = s * 0.385                                 # skirt radius
    # value ring: dark track + amber arc
    box = [c - ring_r, c - ring_r, c + ring_r, c + ring_r]
    d.arc(box, start=A0 - 90, end=A1 - 90, fill=rgba((16, 16, 18)), width=ring_w)
    if t > 0.004:
        d.arc(box, start=A0 - 90, end=A0 - 90 + (A1 - A0) * t, fill=rgba(AMBER), width=ring_w)
    # shadow
    sh = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(sh).ellipse([c - R + s * .02, c - R + s * .045, c + R + s * .02, c + R + s * .045], fill=(0, 0, 0, 170))
    im.alpha_composite(sh.filter(ImageFilter.GaussianBlur(s * 0.03)))
    d = ImageDraw.Draw(im)
    # knurled aluminium skirt (vertical light gradient + rotating ridges)
    sk = Image.new("RGBA", (s, s), (0, 0, 0, 0)); sd = ImageDraw.Draw(sk)
    for yy in range(int(c - R), int(c + R) + 1):
        k = (yy - (c - R)) / (2 * R)
        col = mixc((196, 198, 202), (92, 94, 98), k)
        dx = math.sqrt(max(0.0, R * R - (yy - c) ** 2)); sd.line([c - dx, yy, c + dx, yy], fill=rgba(col))
    im.alpha_composite(sk); d = ImageDraw.Draw(im)
    rot = A0 + (A1 - A0) * t
    for i in range(48):
        deg = rot + i * 7.5
        x0, y0 = ang_xy(c, c, R * 0.86, deg); x1, y1 = ang_xy(c, c, R * 0.995, deg)
        d.line([x0, y0, x1, y1], fill=rgba((70, 72, 76), 200), width=max(1, int(SS * 0.9)))
    d.ellipse([c - R, c - R, c + R, c + R], outline=rgba((40, 40, 44)), width=max(1, SS))
    # machined cap: dark graphite with concentric brushing and a soft top highlight
    r2 = R * 0.80
    cap = Image.new("RGBA", (s, s), (0, 0, 0, 0)); cd = ImageDraw.Draw(cap)
    for i in range(int(r2), 0, -max(1, SS // 2)):
        k = i / r2; v = int(46 + 10 * math.sin(i * 0.9) * 0.3 + (1 - k) * 8)
        cd.ellipse([c - i, c - i, c + i, c + i], fill=rgba((v, v + 1, v + 3)))
    hl = Image.new("RGBA", (s, s), (0, 0, 0, 0))
    ImageDraw.Draw(hl).ellipse([c - r2 * 0.75, c - r2 * 0.95, c + r2 * 0.75, c - r2 * 0.05], fill=(255, 255, 255, 34))
    cap.alpha_composite(hl.filter(ImageFilter.GaussianBlur(s * 0.04)))
    mask = Image.new("L", (s, s), 0); ImageDraw.Draw(mask).ellipse([c - r2, c - r2, c + r2, c + r2], fill=255)
    capm = Image.new("RGBA", (s, s), (0, 0, 0, 0)); capm.paste(cap, (0, 0), mask)
    im.alpha_composite(capm); d = ImageDraw.Draw(im)
    d.ellipse([c - r2, c - r2, c + r2, c + r2], outline=rgba((22, 22, 24)), width=max(1, SS))
    # indicator line
    x0, y0 = ang_xy(c, c, r2 * 0.30, rot); x1, y1 = ang_xy(c, c, r2 * 0.92, rot)
    d.line([x0, y0, x1, y1], fill=rgba(AMBER), width=max(2, int(s * 0.035)))
    return im.resize((px, px), Image.LANCZOS)

def knob_strip(px):
    strip = Image.new("RGBA", (px, px * FRAMES), (0, 0, 0, 0))
    for f in range(FRAMES): strip.paste(knob_frame(px, f / (FRAMES - 1)), (0, f * px))
    return strip

# ---------------------------------------------------------------- meter filmstrip (24 segments, top = clip)
SEGS = 24
def meter_frame(t):
    s = SS; w, h = METER_W * s, METER_H * s
    im = Image.new("RGBA", (w, h), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, w - 1, h - 1], radius=2 * s, fill=rgba((8, 8, 10)))
    seg_h = (h - 4 * s) / SEGS
    lit = int(round(t / 0.97 * (SEGS - 1))) if t < 0.999 else SEGS - 1
    for i in range(SEGS):
        y1 = h - 2 * s - i * seg_h; y0 = y1 - seg_h + s
        base = RED if i == SEGS - 1 else (AMBER if i >= SEGS - 6 else GREEN)
        on = (i < lit) if i < SEGS - 1 else (t >= 0.999)
        col = base if on else mixc(base, (10, 10, 12), 0.82)
        d.rectangle([2 * s, y0, w - 2 * s, y1], fill=rgba(col))
    return im.resize((METER_W, METER_H), Image.LANCZOS)

def meter_strip():
    strip = Image.new("RGBA", (METER_W, METER_H * FRAMES), (0, 0, 0, 0))
    for f in range(FRAMES): strip.paste(meter_frame(f / (FRAMES - 1)), (0, f * METER_H))
    return strip

# ---------------------------------------------------------------- LEDs and bypass button
def led(on, px=16):
    s = SS; im = Image.new("RGBA", (px * s, px * s), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    c = px * s / 2
    d.ellipse([c - 7 * s, c - 7 * s, c + 7 * s, c + 7 * s], fill=rgba((10, 10, 12)))
    col = AMBER if on else (58, 44, 28)
    d.ellipse([c - 5 * s, c - 5 * s, c + 5 * s, c + 5 * s], fill=rgba(col))
    if on:
        d.ellipse([c - 3 * s, c - 4 * s, c + 1 * s, c - 1 * s], fill=(255, 232, 190, 220))
    return im.resize((px, px), Image.LANCZOS)

def bypass_img(on):
    w, h = BYPASS[2], BYPASS[3]; s = SS
    im = Image.new("RGBA", (w * s, h * s), (0, 0, 0, 0)); d = ImageDraw.Draw(im)
    d.rounded_rectangle([0, 0, w * s - 1, h * s - 1], radius=6 * s, fill=rgba((8, 8, 10)))
    face = (120, 34, 26) if on else (44, 46, 50)
    d.rounded_rectangle([3 * s, 3 * s, (w - 3) * s, (h - 3) * s], radius=4 * s, fill=rgba(face))
    lamp = RED if on else (70, 30, 26)
    d.ellipse([14 * s, (h / 2 - 5) * s, 24 * s, (h / 2 + 5) * s], fill=rgba(lamp))
    text_c(d, (w / 2 + 10) * s, h / 2 * s, "BYPASS", font(BOLD, 15 * s), rgba(WHITE if on else GREY))
    return im.resize((w, h), Image.LANCZOS)

# ---------------------------------------------------------------- background panel
def brushed(w, h, base, seed, amp=5):
    random.seed(seed)
    im = Image.new("RGB", (w, h), base); px = im.load()
    rows = [random.randint(-amp, amp) for _ in range(h)]
    for y in range(h):
        r = rows[y]
        for x in range(w):
            n = r + random.randint(-2, 2)
            px[x, y] = (base[0] + n, base[1] + n, base[2] + n + 1)
    return im

def screw(d, x, y, r=6):
    d.ellipse([x - r, y - r, x + r, y + r], fill=(110, 112, 116), outline=(18, 18, 20))
    d.ellipse([x - r + 2, y - r + 2, x + r - 2, y + r - 2], outline=(150, 152, 156))
    d.line([x - r + 3, y, x + r - 3, y], fill=(30, 30, 32), width=2)

def recess(im, x0, y0, x1, y1, radius=8):
    """Recessed module plate: brushed face, dark top-left inner edge, light bottom-right edge."""
    face = brushed(x1 - x0, y1 - y0, ALU, x0 * 7 + y0, amp=3)
    mask = Image.new("L", face.size, 0); ImageDraw.Draw(mask).rounded_rectangle([0, 0, face.size[0] - 1, face.size[1] - 1], radius=radius, fill=255)
    im.paste(face, (x0, y0), mask)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([x0, y0, x1, y1], radius=radius, outline=EDGE_D, width=2)
    d.line([x0 + radius, y1 + 1, x1 - radius, y1 + 1], fill=ALU_HI, width=1)
    d.line([x1 + 1, y0 + radius, x1 + 1, y1 - radius], fill=ALU_HI, width=1)

def background():
    im = brushed(W, H, GRAPH, 11, amp=4)
    d = ImageDraw.Draw(im)
    # rack ears with mounting slots
    for ex in (0, W - 22):
        d.rectangle([ex, 0, ex + 21, H], fill=CHAR)
        d.line([ex + (21 if ex == 0 else 0), 0, ex + (21 if ex == 0 else 0), H], fill=EDGE_D, width=2)
        for sy in (70, H - 90):
            d.rounded_rectangle([ex + 5, sy, ex + 16, sy + 30], radius=5, fill=(8, 8, 9), outline=(70, 72, 76))
    # header strip
    d.rectangle([22, 0, W - 22, 48], fill=CHAR)
    d.line([22, 48, W - 22, 48], fill=EDGE_D, width=2)
    d.line([22, 50, W - 22, 50], fill=(60, 62, 66), width=1)
    d.text((40, 9), "RSFX", font=font(BOLD, 26), fill=WHITE)
    d.text((126, 18), spaced("ASR  SERIES  EFFECTS  RACK"), font=font(COND, 12), fill=GREY)
    d.text((W - 372, 18), spaced("ASR-10  ·  ASR-X  ·  98  ALGORITHMS"), font=font(COND, 11), fill=GREY)
    # modules
    for key, title, x0, y0, x1, y1, ledp in MODULES:
        recess(im, x0, y0, x1, y1)
        d = ImageDraw.Draw(im)
        tx = x0 + (34 if ledp else 14)
        d.text((tx, y0 + 9), spaced(title), font=font(COND, 13), fill=WHITE)
        d.line([x0 + 12, y0 + 30, x1 - 12, y0 + 30], fill=EDGE_D, width=1)
        d.line([x0 + 12, y0 + 31, x1 - 12, y0 + 31], fill=ALU_HI, width=1)
    # LCD window
    x0, y0, x1, y1 = LCD
    d.rounded_rectangle([x0 - 4, y0 - 4, x1 + 4, y1 + 4], radius=8, fill=EDGE_D)
    lcd = Image.new("RGB", (x1 - x0, y1 - y0), (10, 13, 15)); ld = ImageDraw.Draw(lcd)
    for yy in range(0, y1 - y0, 3): ld.line([0, yy, x1 - x0, yy], fill=(13, 17, 20))       # fine pixel rows
    for yy in range(0, 40):                                                                    # glass reflection
        a = int(10 * (1 - yy / 40)); ld.line([0, yy, x1 - x0, yy], fill=(10 + a, 13 + a, 15 + a))
    im.paste(lcd, (x0, y0)); d = ImageDraw.Draw(im)
    d.text((x0 + 16, y0 + 12), spaced("PROGRAM"), font=font(COND, 11), fill=mixc(ICE, (10, 13, 15), 0.35))
    d.text((x0 + 16, y0 + 104), spaced("SIGNAL"), font=font(COND, 11), fill=mixc(ICE, (10, 13, 15), 0.35))
    d.text((x0 + 16, y0 + 124), "IN ▸", font=font(COND, 13), fill=mixc(ICE, (10, 13, 15), 0.2))
    d.text((x1 - 104, y0 + 124), "▸ MIX ▸ OUT", font=font(COND, 13), fill=mixc(ICE, (10, 13, 15), 0.2))
    d.line([x0 + 14, y0 + 96, x1 - 14, y0 + 96], fill=(30, 40, 46), width=1)
    # meter scale + captions
    f = font(COND, 9)
    for mx in (160, 1178):
        for db, lab in ((3, "CLIP"), (-12, "12"), (-24, "24"), (-36, "36")):
            n = (db + 48) / 51.0
            yy = 98 + METER_H - 2 - n * (METER_H - 4)
            if lab: d.text((mx + 30 if mx > 600 else mx - 26, yy - 6), lab, font=f, fill=GREY)
        text_c(d, mx + 14, 98 + METER_H + 12, "L  R", font(COND, 10), GREY)
    d.text((1005, 230), "", font=f, fill=GREY)
    # signal-flow strip between rows: shows the fixed bus order of the rack
    return im

# ---------------------------------------------------------------- TUI.json helpers (same schema as the JV-880/GlueBus skins)
def bounds(b, focus="No", show="Show", visible="Always"):
    return {"version": 2, "acceptsHWFocus": focus, "showWhenDataModelInvalid": show, "whenVisible": visible,
            "boundsType": "Absolute", "bounds": " ".join(str(int(round(v))) for v in b), "additionalInvalidatingHandles": []}
NOREMAP = {"version": 1, "map": []}
def remap(p): return {"version": 1, "map": [{"key": "Data", "value": f"Parameter {P[p]}"}]}
def comp(name, typ, data, b, rm=None):
    return {"version": 2, "componentData": {"version": 1, "name": name, "type": typ, "data": data},
            "handle remapping": rm or NOREMAP, "bounds": b}
def action(on, handler, extra=""):
    return {"version": 2, "onAction": on, "handler": handler, "handleName": "" if handler == "Show Overlay" else "Data",
            "additionalData": extra, "handle remapping": NOREMAP}
def bgdata(col="0"): return {"version": 1, "focussed": {"version": 1, "colour": col, "image": ""}, "unfocussed": {"version": 1, "colour": col, "image": ""}}
def definition(actions, parts, bgcol="0", ignore=False):
    return {"version": 4, "actions": actions, "backgroundData": bgdata(bgcol), "ignoreMousePresses": ignore,
            "disableCoarseDataWheel": False, "repeats": 1, "hideQLinkBounds": True, "componentsData": parts}
def label(name, kind, h, colour, b, case="Original", just="horizontallyCentred verticallyCentred", style="SemiBold"):
    return comp(name, "Label", {"version": 1, "textStyle": {"version": 1, "font": {"version": 1, "name": "Titillium Web", "style": style, "height": float(h)},
                "colour": colour, "justification": just, "case": case}, "type": kind, "handleName": "Data"}, bounds(b))
def focus(b):
    return comp("Focus", "Focus", {"version": 1, "backgroundColour": "00000000", "outlineColour": hexa(ICE), "backgroundInset": 2.0,
                "outlineThickness": 2.0}, bounds(b, visible="WhenFocussed"))

def knob_def(size, with_value=True):
    k = KS[size]; bw, bh = k["box"]; px = k["img"]
    parts = [focus((0, 0, bw, bh)),
             comp("Knob", "Knob", {"version": 5, "knobType": "FilmStrip", "filmStrip": f"rs_knob_{size}.png", "numFrames": NUMFRAMES,
                   "invert": False, "dragOrientation": "Vertical", "handleName": "Data"}, bounds(((bw - px) / 2, 4, px, px))),
             label("Name", "Name", 14 if size == "S" else 15, hexa(WHITE), (0, px + 8, bw, k["name_h"]), case="Upper Case"),
             label("Value", "Value", 14 if size == "S" else 17, hexa(AMBER), (0, px + 8 + k["name_h"], bw, k["val_h"]))]
    if not with_value: parts = parts[:3]
    return definition([action("Mouse Down", "Q-Link"), action("Double Click", "Show Overlay", "knob overlay"),
                       action("Enter Pressed", "Show Overlay", "knob overlay")], parts)

def build():
    if os.path.isdir(SKIN_DIR): shutil.rmtree(SKIN_DIR)
    os.makedirs(OUT)
    defs, page = [], []
    background().save(os.path.join(OUT, "rs_bg.png"), optimize=True)
    page.append(comp("Background", "Image", {"version": 2, "imageType": "Regular", "colour": "0", "image": "rs_bg.png"}, bounds((0, 0, W, H))))

    for size in ("XL", "L", "M", "S"):
        knob_strip(KS[size]["img"]).save(os.path.join(OUT, f"rs_knob_{size}.png"), optimize=True)
        defs.append({"key": f"rsKnob{size}", "value": knob_def(size, with_value=(size != "XL"))})
    for p, size, x, y in KNOBS:
        bw, bh = KS[size]["box"]
        page.append(comp(p, f"rsKnob{size}", {"version": 1, "handleName": "Data"}, bounds((x, y, bw, bh), focus="Yes", show="Hide"), remap(p)))

    # LCD: algorithm name and the signal chain are the Algorithm / Chain parameters' display text
    x0, y0, x1, y1 = LCD
    page.append(label("Program", "Value", 34, hexa(AMBER), (x0 + 14, y0 + 30, x1 - x0 - 28, 54), just="left verticallyCentred", style="Bold"))
    page[-1]["handle remapping"] = remap("algo")
    page.append(label("Chain", "Value", 17, hexa(ICE), (x0 + 56, y0 + 118, x1 - x0 - 170, 26), just="left verticallyCentred"))
    page[-1]["handle remapping"] = remap("chain")

    # meters: display-only filmstrips bound to the read-only meter parameters
    meter_strip().save(os.path.join(OUT, "rs_meter.png"), optimize=True)
    defs.append({"key": "rsMeter", "value": definition([], [
        comp("Knob", "Knob", {"version": 5, "knobType": "FilmStrip", "filmStrip": "rs_meter.png", "numFrames": NUMFRAMES,
             "invert": False, "dragOrientation": "Vertical", "handleName": "Data"}, bounds((0, 0, METER_W, METER_H)))], ignore=True)})
    for p, x, y in METERS:
        page.append(comp(p, "rsMeter", {"version": 1, "handleName": "Data"}, bounds((x, y, METER_W, METER_H)), remap(p)))

    # module lights: display-only buttons bound to the read-only "module in use" parameters
    led(True).save(os.path.join(OUT, "rs_led_on.png")); led(False).save(os.path.join(OUT, "rs_led_off.png"))
    defs.append({"key": "rsLed", "value": definition([], [
        comp("Button", "Button", {"version": 2, "onImage": "rs_led_on.png", "offImage": "rs_led_off.png", "buttonId": 1,
             "numButtonsInGroup": 1, "handleName": "Data", "gestureBehaviour": "Instant"}, bounds((0, 0, 16, 16)))], ignore=True)})
    for key, title, mx0, my0, mx1, my1, ledp in MODULES:
        if ledp:
            page.append(comp(ledp, "rsLed", {"version": 1, "handleName": "Data"}, bounds((mx0 + 12, my0 + 9, 16, 16)), remap(ledp)))

    # bypass: illuminated push button
    bypass_img(True).save(os.path.join(OUT, "rs_bypass_on.png")); bypass_img(False).save(os.path.join(OUT, "rs_bypass_off.png"))
    bx, by, bw, bh = BYPASS
    defs.append({"key": "rsBypass", "value": definition([action("Mouse Down", "Q-Link"), action("Enter Pressed", "Toggle Switch")], [
        focus((0, 0, bw, bh)),
        comp("Button", "Button", {"version": 2, "onImage": "rs_bypass_on.png", "offImage": "rs_bypass_off.png", "buttonId": 1,
             "numButtonsInGroup": 1, "handleName": "Data", "gestureBehaviour": "Instant"}, bounds((0, 0, bw, bh)))])})
    page.append(comp("Bypass", "rsBypass", {"version": 1, "handleName": "Data"}, bounds((bx, by, bw, bh), focus="Yes", show="Hide"), remap("bypass")))

    defs.append({"key": "MAIN|Main", "value": definition([], page, hexa(GRAPH))})
    tui = {"pageData": {"version": 1,
        "componentDefinitions": {"version": 2, "importFiles": ["/usr/share/Akai/Content/Synths/Generic/Generic Knob Overlay.json",
                                                                "/usr/share/Akai/Content/Synths/Generic/Generic Menu Overlay.json"],
                                 "localComponentDefinitions": defs},
        "info": {"version": 1, "type": "CompleteDescription"},
        "tabs": [{"version": 3, "tabName": "Rack", "fnKeyIndex": 0, "fnKeySubIndex": 0, "qlinkBoundsData": ["0 0 0 0"],
                  "componentName": "MAIN|Main", "initialSize": f"0 0 {W} {H}", "scale": 1.0}]}}
    json.dump(tui, open(os.path.join(OUT, "TUI.json"), "w"), indent=4)

    # Q-Links 1-16 = parameters 0-15 (unchanged from v1.0, documented in README)
    order = ["algo", "mix", "decay", "damp", "pre", "delay", "fb", "rate", "depth", "drive", "freq", "reso", "thresh", "pitch", "bal", "out"]
    ql = {f"Q-Link {i + 1}": P[k] for i, k in enumerate(order)}
    q = {"version": 4, "info": {"version": 1, "type": "CompleteDescription"},
         "Screen Mode Q-Links": {"version": 4, "map": [{"Tab": 1, "SubTab": 1, "Bank Direction": "Column", "Q-Links": ql}]},
         "Program Mode Q-Links": ql}
    for fn in ("Q-Links.json", "Q-Links - 8by1.json"):
        json.dump(q, open(os.path.join(OUT, fn), "w"), indent=4)
    open(os.path.join(SKIN_DIR, "version.xml"), "w").write(
        "<?xml version='1.0' encoding='utf-8'?>\n<plugincontent version=\"1.0\">\n\t<identifier>rsfx.vst.rsfx</identifier>\n"
        "\t<version>1.1.0.0</version>\n</plugincontent>\n")
    print("skin written to", SKIN_DIR)

# ---------------------------------------------------------------- preview (approximates how MPC OS draws the skin)
def preview(state_path, out_path):
    st = json.load(open(state_path))
    vals = {int(k): v for k, v in st["values"].items()}; disp = {int(k): v for k, v in st["display"].items()}
    names = {int(k): v for k, v in st["names"].items()}
    im = Image.open(os.path.join(OUT, "rs_bg.png")).convert("RGBA"); d = ImageDraw.Draw(im)
    for p, size, x, y in KNOBS:
        k = KS[size]; bw, bh = k["box"]; px = k["img"]; i = P[p]
        strip = Image.open(os.path.join(OUT, f"rs_knob_{size}.png"))
        fr = int(round(vals[i] * (FRAMES - 1)))
        im.alpha_composite(strip.crop((0, fr * px, px, (fr + 1) * px)), (int(x + (bw - px) / 2), y + 4))
        text_c(d, x + bw / 2, y + px + 8 + k["name_h"] / 2, names[i].upper(), font(COND, 13 if size == "S" else 14), WHITE)
        if size != "XL":
            text_c(d, x + bw / 2, y + px + 8 + k["name_h"] + k["val_h"] / 2, disp[i], font(COND, 13 if size == "S" else 15), AMBER)
    x0, y0, x1, y1 = LCD
    d.text((x0 + 16, y0 + 36), disp[P["algo"]], font=font(COND, 34), fill=AMBER)
    d.text((x0 + 58, y0 + 122), disp[P["chain"]], font=font(COND, 16), fill=ICE)
    ms = Image.open(os.path.join(OUT, "rs_meter.png"))
    for p, x, y in METERS:
        fr = int(round(vals[P[p]] * (FRAMES - 1)))
        im.alpha_composite(ms.crop((0, fr * METER_H, METER_W, (fr + 1) * METER_H)), (x, y))
    for key, title, mx0, my0, mx1, my1, ledp in MODULES:
        if ledp: im.alpha_composite(Image.open(os.path.join(OUT, "rs_led_on.png" if vals[P[ledp]] > 0.5 else "rs_led_off.png")), (mx0 + 12, my0 + 9))
    im.alpha_composite(Image.open(os.path.join(OUT, "rs_bypass_on.png" if vals[P["bypass"]] > 0.5 else "rs_bypass_off.png")), (BYPASS[0], BYPASS[1]))
    im.convert("RGB").save(out_path)

if __name__ == "__main__":
    if len(sys.argv) > 1 and sys.argv[1] == "--preview": preview(sys.argv[2], sys.argv[3])
    else: build()
