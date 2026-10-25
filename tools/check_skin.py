#!/usr/bin/env python3
"""Static checks for the generated skin: every image exists and has the right filmstrip height, every component type
is defined, every bound parameter exists in the plugin, and Q-Links point at real parameters."""
import json, os, re, sys
from PIL import Image
ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
D = os.path.join(ROOT, "mpc", "skin", "RSFX - VST - RSFX", "Plugin Skins")
NPARAMS = int(sys.argv[1]) if len(sys.argv) > 1 else 27
t = json.load(open(os.path.join(D, "TUI.json")))["pageData"]
defs = {x["key"]: x["value"] for x in t["componentDefinitions"]["localComponentDefinitions"]}
builtin = {"Image", "Knob", "Label", "Focus", "Button"}
errs = []
def walk(o, f):
    if isinstance(o, dict):
        f(o); [walk(v, f) for v in o.values()]
    elif isinstance(o, list): [walk(v, f) for v in o]
imgs, types, params = set(), set(), set()
def visit(o):
    for k in ("image", "filmStrip", "onImage", "offImage"):
        if o.get(k): imgs.add(o[k])
    if "componentData" in o: types.add(o["componentData"]["type"])
    for m in o.get("map", []) if isinstance(o.get("map"), list) else []:
        if isinstance(m, dict) and str(m.get("value", "")).startswith("Parameter "): params.add(int(m["value"].split()[1]))
walk(t, visit)
for i in imgs:
    if not os.path.exists(os.path.join(D, i)): errs.append(f"missing image {i}")
for x in types:
    if x not in defs and x not in builtin: errs.append(f"undefined component type {x}")
if t["tabs"][0]["componentName"] not in defs: errs.append("tab component missing")
for p in params:
    if not 0 <= p < NPARAMS: errs.append(f"bound to nonexistent parameter {p}")
def strips(o):
    if o.get("knobType") == "FilmStrip":
        im = Image.open(os.path.join(D, o["filmStrip"])); w, h = im.size
        if h % (o["numFrames"] + 1): errs.append(f"{o['filmStrip']}: height {h} not a multiple of {o['numFrames'] + 1} frames")
walk(t, strips)
q = json.load(open(os.path.join(D, "Q-Links.json")))
for k, v in q["Program Mode Q-Links"].items():
    if not 0 <= v < 16: errs.append(f"{k} -> parameter {v}")
print(f"{len(imgs)} images, {len(types)} component types, parameters bound: {sorted(params)}")
print("\n".join(errs) if errs else "SKIN OK")
sys.exit(1 if errs else 0)
