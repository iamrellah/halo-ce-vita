#!/usr/bin/env python3
"""Presses keys in the running Vita3K window: vita3k_keys.py WAIT "key:hold:pause ..." (a+b for chords).
keys: x c z v (cross circle square triangle), up/down/left/right, enter (start), select, w/a/s/d (left stick)."""
import os, sys, time
from Xlib import display as xdisplay
from pynput.keyboard import Controller as Keyboard, Key
from pynput.mouse import Button, Controller as Mouse
os.environ.setdefault("DISPLAY", ":0")
SPECIAL = {"up": Key.up, "down": Key.down, "left": Key.left, "right": Key.right, "enter": Key.enter, "select": Key.shift_r}
def render_window(disp):
    root = disp.screen().root
    for wid in root.get_full_property(disp.intern_atom("_NET_CLIENT_LIST"), 0).value:
        w = disp.create_resource_object("window", wid)
        try:
            cls = w.get_wm_class(); name = w.get_wm_name() or ""
        except Exception:
            continue
        if not cls or "vita3k" not in " ".join(cls).lower() or name.startswith("Vita3K v"):
            continue
        g = w.get_geometry(); o = w.translate_coords(root, 0, 0)
        return -o.x, -o.y, g.width, g.height
def main():
    time.sleep(float(sys.argv[1])); disp = xdisplay.Display(); found = None
    for _ in range(30):
        found = render_window(disp)
        if found: break
        time.sleep(1)
    if not found:
        print("no window"); return 1
    x, y, w, h = found; mouse, kb = Mouse(), Keyboard()
    mouse.position = (x + w // 2, y + h // 2); mouse.click(Button.left); time.sleep(0.5)
    for step in sys.argv[2].split():
        name, hold, pause = (step.split(":") + ["0.15", "1.5"])[:3]
        keys = [SPECIAL.get(p, p) for p in name.split("+")]
        for k in keys: kb.press(k)
        time.sleep(float(hold))
        for k in reversed(keys): kb.release(k)
        time.sleep(float(pause))
    return 0
sys.exit(main())
