---
name: desktop-control-x11
description: How to see and drive the user's desktop from a Claude Code session (X11, no sudo): ImageMagick import for screenshots, pynput in a scratchpad venv for mouse/keyboard
metadata:
  type: reference
---

The user's Manjaro box runs X11 (DISPLAY=:0). No xdotool, sudo needs a password.
What worked (Sept 18 2026): `import -window root shot.png` for full-screen captures, and a venv with
`pynput` + `python-xlib` for clicks, typing, drag and scroll. A small driver script (click/type/key/
drag/scroll/msgfile/shot) was built in the session scratchpad, so it must be recreated each session.

**Why:** The user asked Claude to "take control of my desktop" to build the Xita Discord server in the
desktop app; this was the only root-free path.

**How to apply:** Screen is 1920x1080. Discord app sits at x 1060-1920; crop screenshots to that
region to save context. Discord drags: channel drag in the sidebar works; role reorder works only via the hover drag handle
on the Roles list page; category reorder did not respond. Type multi-line messages with shift+enter between lines; avoid ":word" (emoji popup)
and "#name" (channel popup) inside typed text. See [[xita-discord-server]].
