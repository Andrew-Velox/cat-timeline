# cat-timeline

A lightweight desktop todo widget for Linux. A glowing purple timeline runs
across a small borderless window: one dot per day, a Cairo-drawn cat running in
place on **today**, and little dashes above each dot for the tasks due that day.
Hover a dot to highlight it; click it to open a small panel for that day's tasks.

The timeline, line glow, dots and task dashes are all drawn with **Cairo** into a
single `GtkDrawingArea`. The running cat uses the iconic **RunCat** runner
(5 sprite frames), embedded in the binary and tinted to match the theme — see
[Credits](#credits).

![cat-timeline](docs/image.png)

---

## Features

- Borderless, always-on-top, no taskbar entry, compact **320 × 90** window with a
  fully transparent background (only the cat and timeline are drawn).
- Spawns in the bottom-right corner of the primary monitor.
- Horizontal timeline with a soft purple glow that fades out toward the edges.
- A handful of day dots (1 past, today, 3 future), today's dot at 25% from the left.
- The RunCat runner (5 sprite frames), tinted and animated on today's dot, with
  a fading paw-print trail.
- Per-day task dashes above each dot (max 8, then a `+N` overflow badge), colour
  coded by state (pending / today / past / done).
- Hover a dot to highlight it (it grows and glows); no popup, so nothing flickers.
- **Click a dot** to open a styled task panel — a small floating card with the
  date, a completion bar, and a scrollable list to add, check off and delete that
  day's tasks. Press `Esc` or the `×` to close.
- **Double-click** empty space to open a matching **settings window**: a month
  calendar (days with tasks are marked, and you can add/toggle/delete a day's
  tasks) and an **Appearance** panel with colour pickers for the timeline, cat,
  tasks and dots. Changes apply live and are saved. `Esc` / `×` closes it.
- Right-click anywhere for a **Quit** menu; click-and-drag empty space to move
  the window.
- The timeline scrolls with the clock: each day's dot sits on the cat at
  midnight and drifts left as the day elapses, so tomorrow's dot reaches the cat
  as today ends. Set `CAT_TIMELINE_DEMO=<seconds>` to compress a "day" into that
  many seconds and watch it scroll.
- Tasks stored as JSON in `~/.local/share/cat-timeline/tasks.json`; colours in
  `~/.local/share/cat-timeline/settings.json`. Both saved on every change.

---

## Dependencies

| Package | Purpose            |
|---------|--------------------|
| `gtk3`  | windowing / widgets |
| `cairo` | 2D drawing (pulled in by gtk3) |
| `meson` | build system       |
| `ninja` | build backend      |
| a C99 compiler (`gcc` or `clang`) | |

`cJSON` is bundled in `assets/` and compiled into the binary — no extra package
needed.

### Install on Arch

```sh
sudo pacman -S gtk3 meson ninja gcc
```

### Debian / Ubuntu

```sh
sudo apt install libgtk-3-dev meson ninja-build gcc
```

### Fedora

```sh
sudo dnf install gtk3-devel meson ninja-build gcc
```

---

## Install

**1. Install the dependencies** for your distro (see the commands above).

**2. Clone, build and run:**

```sh
git clone https://github.com/Andrew-Velox/cat-timeline.git
cd cat-timeline
meson setup build
ninja -C build
./build/cat-timeline
```

**3. (Optional) install it system-wide** so you can launch it as `cat-timeline`
from anywhere (`/usr/local/bin/cat-timeline` by default):

```sh
sudo ninja -C build install
```

---

## Autostart on Hyprland

Add the binary to your Hyprland config (`~/.config/hypr/hyprland.conf`, or a
sourced custom file — on **end-4 / illogical-impulse** put it in
`~/.config/hypr/custom/`). Wayland compositors place windows themselves, so use
window rules to pin the widget and strip the window decorations:

```ini
# Launch the widget at startup (use an absolute path or install it on $PATH)
exec-once = cat-timeline

# Target the main widget only (its window title is exactly "cat-timeline";
# the task/settings panels use other titles so they stay normal).
windowrulev2 = float,       title:^(cat-timeline)$
windowrulev2 = pin,         title:^(cat-timeline)$
windowrulev2 = nofocus,     title:^(cat-timeline)$
windowrulev2 = size 320 90, title:^(cat-timeline)$
windowrulev2 = move 100%-344 100%-114, title:^(cat-timeline)$

# Strip decorations so only the transparent widget shows (no card/box).
# Needed on setups like end-4 that round/border/shadow/blur every window.
windowrulev2 = noborder,    title:^(cat-timeline)$
windowrulev2 = rounding 0,  title:^(cat-timeline)$
windowrulev2 = noshadow,    title:^(cat-timeline)$
windowrulev2 = noblur,      title:^(cat-timeline)$
```

`move 100%-344 100%-114` anchors it 24px from the bottom-right corner
(320 + 24 = 344, 90 + 24 = 114). Reload with `hyprctl reload`.

> **Seeing a rounded box / border around the widget?** That is your compositor
> drawing window decorations, not the app (its background is fully transparent).
> The `noborder` / `rounding 0` / `noshadow` / `noblur` rules above remove it.

> For a true see-through widget you also need a compositor doing alpha blending.
> Hyprland does this out of the box; on other setups make sure a compositor
> (e.g. `picom` on X11) is running, or the transparent areas may render black.

On X11 desktops the app positions itself in the bottom-right corner on launch,
so no extra configuration is needed there.

---

## Data format

`~/.local/share/cat-timeline/tasks.json` (created on first save):

```json
{
  "2026-06-03": [
    { "id": "6a201b8e001", "text": "Fix bug",   "done": false },
    { "id": "6a201b8e002", "text": "Write docs", "done": true }
  ]
}
```

The whole file is loaded into memory on startup and rewritten on every change.

---

## Project layout

```
cat-timeline/
├── meson.build
├── README.md
├── src/
│   ├── main.c        entry point, GTK init
│   ├── app.h         shared state + geometry constants
│   ├── window.c/h    borderless always-on-top window, draw callback
│   ├── timeline.c/h  line, dots, date labels, task dashes
│   ├── cat.c/h       Cairo cat animation
│   ├── tasks.c/h     JSON load/save, task model, date helpers
│   ├── style.c/h     shared CSS theme for the task panel + settings window
│   ├── settings.c/h  colour palette load/save (settings.json)
│   ├── settings_window.c/h  double-click calendar + appearance window
│   └── input.c/h     mouse events, drag, context menu, task panel
└── assets/
    ├── cJSON.h
    ├── cJSON.c          bundled single-file JSON library (MIT)
    ├── runcat_frames.h  RunCat sprite frames embedded as byte arrays
    └── runcat/          original PNG frames + LICENSE (Apache-2.0)
```

The sprite frames are decoded once at startup with gdk-pixbuf and painted via
their alpha channel as a mask, so the cat is recoloured to the widget theme
(`#f0e0ff`).

---

## Performance notes

The window is **not** redrawn every frame. `gtk_widget_queue_draw()` is only
called when:

1. the 100ms cat-animation timer fires,
2. the pointer enters or leaves a dot's hover zone,
3. task data changes,
4. the window first draws.

A single drawing area, a single timer, no extra threads.

---

## Credits

The running cat sprites are from **RunCat** by **Takuto Nakamura (Kyome22)**:
<https://github.com/Kyome22/RunCat365> — `resources/runners/cat/`. They are
licensed under the **Apache License 2.0**; the full license text is kept at
[`assets/runcat/LICENSE`](assets/runcat/LICENSE). The frames are embedded in
this project unmodified (re-tinted at render time only).

## License

- `assets/cJSON.{c,h}` — © Dave Gamble and cJSON contributors (MIT).
- `assets/runcat/*.png` — © 2025 Takuto Nakamura (Apache-2.0, see above).
- The rest of this project is provided as-is.
