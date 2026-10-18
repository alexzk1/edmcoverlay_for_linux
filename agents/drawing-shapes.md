# Drawing Shapes on the EDMC Overlay (Python)

How to draw geometry — lines, polygons, circles, markers, labels — on the
EDMC overlay from a Python plugin.

This guide covers the **Linux `edmcoverlay`** backend (the one in this repo).
It is based on the real protocol understood by the compiled overlay binary and
on a working reference implementation (a radar plugin that draws nested circles
with markers and labels).

---

## 1. What the overlay is

The overlay is a separate window (X11) that sits on top of the game. Plugins
talk to it over a **local TCP connection on port 5010** (`127.0.0.1:5010`).

The Python side is a thin wrapper. You send it **JSON messages**; the binary
parses each one and renders it. Every shape/text/SVG you send is stored in the
binary under a **unique id**. Re-sending the same id **replaces** the old
drawing and refreshes its lifetime.

Everything the binary draws is ultimately converted to **SVG** and rendered with
`lunasvg`. So the shapes you can draw are whatever SVG supports (lines, circles,
rects, polygons, text).

---

## 2. Connecting

Import the overlay module. There are two accepted package names — import the
first that works and fall back:

```python
try:
    from EDMCOverlay import edmcoverlay
except ImportError:
    try:
        from edmcoverlay import edmcoverlay
    except ImportError:
        edmcoverlay = None
```

Create **one `Overlay` instance per plugin**. Each instance opens its own TCP
connection. If the overlay binary is not running, `Overlay()` still constructs
and connects lazily — the first `send_*` call (or a `connect()` call) attempts
the connection.

```python
overlay = edmcoverlay.Overlay()
```

If `overlay is None`, the overlay module is not installed — skip drawing.

### Checking support defensively

Not every overlay supports every feature — older overlays, for example, have
**no SVG support at all**. Never assume a feature exists. Instead, check
`is_available()` first, then probe for the feature with a `hasattr` / `callable`
guard. The real client caches each result on first use (`_has_svg`,
`_has_multiline`) so the check is cheap after the first call:

```python
def is_available(self) -> bool:
    return self._overlay is not None

def supports_svg(self) -> bool:
    return self._has_svg or (
        self._overlay is not None
        and hasattr(self._overlay, "is_svg_supported")
        and callable(self._overlay.is_svg_supported)
        and self._overlay.is_svg_supported()
    )

def supports_multiline(self) -> bool:
    return self._has_multiline or (
        self._overlay is not None
        and hasattr(self._overlay, "is_multiline_supported")
        and callable(self._overlay.is_multiline_supported)
        and self._overlay.is_multiline_supported()
    )
```

The pattern is the same for every feature: `self._has_<feature> or (overlay is
not None and hasattr(overlay, "is_<feature>_supported") and callable(...) and
overlay.is_<feature>_supported())`. Guard with `hasattr`/`callable` because an
older overlay simply won't have the method — calling it would raise
`AttributeError`.

---

## 3. Two ways to draw shapes

There are two entry points for shapes:

| Method | Use for |
| --- | --- |
| `send_shape(...)` | Simple fixed-size shapes (`rect`). |
| `send_raw({...})` | **Vector shapes** — polylines, polygons, circles, markers, labels. Use this for almost everything. |

### 3.1 `send_shape` — simple shapes

```python
overlay.send_shape(
    shapeid="my_rect",   # unique id for this shape
    shape="rect",        # "rect" is the only non-vector shape
    color="#00FF88",     # stroke color
    fill="#00FF8820",    # fill color, "" = none
    x=100, y=100,        # top-left corner (screen pixels)
    w=64, h=64,          # width / height in pixels
    ttl=4,               # lifetime in seconds (0 = remove immediately)
)
```

The message this produces:

```json
{"shape": "rect", "color": "#00FF88", "fill": "#00FF8820", "x": 100, "y": 100, "w": 64, "h": 64, "ttl": 4, "id": "my_rect"}
```

`send_shape` does **not** send a `vector` field, so it only supports the fixed
`rect` shape. For anything more complex — lines, polygons, circles, markers —
use `send_raw` with a **vector shape** (see below).

> `fill` fills the rectangle's interior; `color` draws its stroke (border).
> An empty `fill` means "no fill". This used to be a bug in the overlay binary
> (the `fill` field was parsed but ignored, so filled frames never rendered) and
> is now fixed -- pass a real color to get a filled rect.

### 3.2 Vector shapes via `send_raw` — the main method

A **vector shape** is a list of points. The binary connects consecutive points
with lines and, for points that carry a color, draws a **marker** at each of
them. This single mechanism draws lines, polygons, circles, and labeled
markers.

Build the message as a plain dict and pass it to `send_raw`:

```python
overlay.send_raw({
    "id": "my_shape",
    "shape": "vect",          # "vect" = vector shape
    "vector": [               # the points (see §4)
        {"x": 10, "y": 10},
        {"x": 50, "y": 10},
        {"x": 50, "y": 50},
    ],
    "color": "#00FF88",       # stroke color for the connecting lines
    "ttl": 4,
})
```

The binary reads these JSON keys (from `parseJsonString`):

| JSON key | Meaning |
| --- | --- |
| `id` | Unique id. Re-sending the same id replaces the drawing and refreshes TTL. |
| `shape` | `"vect"` selects the vector-shape path. Must be `"vect"`. |
| `vector` | **Array of points.** Its value becomes the shape's point list. |
| `color` | Stroke color for the lines connecting the points. |
| `fill` | Fill color (unused for open polylines). |
| `x`, `y` | Base corner (ignored for vector shapes — each point is absolute). |
| `w`, `h` | Size hints (ignored for vector shapes). |
| `ttl` | Lifetime in seconds. `0` removes it immediately. |
| `size` | `"normal"` or `"large"`; sets the marker/label font size (see §5). |
| `vector_font_size` | Optional explicit marker/label font size in px. Overrides `size`. |

> **One mode per message.** A message is either text, shape, or SVG. Do not mix
> text keys (`text`, `size`, `font_size`) with shape keys in the same dict — the
> binary ignores it.

---

## 4. Points and markers

Each entry in the `vector` array is a point:

```json
{"x": 12, "y": 34}
```

- `x`, `y` are **required** integers (screen pixels; see §6).
- A point **without** a `color` is just a **line vertex** — it connects to the
  next point with a line.
- A point **with** a `color` is drawn as a **marker** at its position, *in
  addition to* the connecting lines.

### 4.1 Markers

Add `color`, `marker`, and optionally `text` to a point to make it a marker:

| `marker` value | Renders as |
| --- | --- |
| `"circle"` | An outlined circle (radius ~4px, stroke = `color`). |
| `"cross"` | An X (two diagonal lines, stroke = `color`). |
| *(text only)* | A point with `color` and `text` but no `marker` type renders a text label. |

```python
overlay.send_raw({
    "id": "radar_center",
    "shape": "vect",
    "vector": [
        {"x": 640, "y": 360, "color": "#00FF88", "marker": "circle"},
    ],
    "ttl": 4,
})

overlay.send_raw({
    "id": "target_marker",
    "shape": "vect",
    "vector": [
        {"x": 800, "y": 200, "color": "#FF3344", "marker": "cross", "text": "Asteroid"},
    ],
    "ttl": 4,
})
```

A marker point:

```json
{"x": 800, "y": 200, "color": "#FF3344", "marker": "cross", "text": "Asteroid"}
```

- `color` — marker stroke/label color (required for a point to be a marker).
- `marker` — `"cross"` or `"circle"`.
- `text` — optional label drawn next to the marker.

### 4.2 How points connect

Points are connected **sequentially**: point 0→1, 1→2, 2→3, … Markers are
drawn at every point that has a color, **on top of** the connecting lines.

- **1 point** → just a marker, no lines.
- **2 points** → one line segment (useful for arrows, needles, indicators).
- **N points** → an open or closed polyline.

To draw a **closed polygon**, repeat the first point at the end so the last
segment closes the shape.

---

## 5. Drawing circles

There is no native "circle" primitive on this backend. Draw a circle as a
**vector polygon**: compute `N` points around the circumference and connect them.

```python
import math

def circle_points(cx, cy, radius, segments=48):
    """Return `segments + 1` points around a circle; the last equals the first
    to close the polygon."""
    pts = []
    for i in range(segments + 1):
        angle = math.radians(360.0 / segments * i)
        pts.append({
            "x": int(cx + radius * math.cos(angle)),
            "y": int(cy + radius * math.sin(angle)),
        })
    return pts

overlay.send_raw({
    "id": "radar_ring",
    "shape": "vect",
    "vector": circle_points(640, 360, 200, segments=48),
    "color": "#4488FF",
    "ttl": 4,
})
```

`segments` controls smoothness. `360 / segments` is the step in degrees; use at
least ~24 for a coarse circle, ~48 for a smooth one. Each point has **no
`color`**, so they form a clean outline with no markers.

### Nested circles (radar)

Draw each ring as its own message with its **own id**:

```python
for i, radius in enumerate([60, 120, 180, 240]):
    overlay.send_raw({
        "id": f"radar_ring_{i}",
        "shape": "vect",
        "vector": circle_points(640, 360, radius, segments=48),
        "color": "#4488FF",
        "ttl": 4,
    })
```

Give every ring a **distinct id** (`radar_ring_0`, `radar_ring_1`, …) so each
can be replaced or cleared independently.

---

## 6. Coordinate system

Coordinates are **screen-space pixels**:

- `x` grows left → right.
- `y` grows top → bottom.
- `(0, 0)` is the top-left of the overlay/screen area.
- Each vector point's `x`/`y` is an **absolute** screen position (the `x`/`y`
  and `w`/`h` fields are ignored for vector shapes).

Keep your shapes inside the overlay's configured area (set in EDMC overlay
settings). Going off-screen simply clips the drawing.

---

## 7. Colors

Accepted formats:

```
red, yellow, green, blue, ...   # named colors
#rrggbb                          # hex, e.g. #00FF88
```

- The top-level `color` is the **stroke** for the connecting **lines** and for
  markers/labels.
- Each **marker point** may carry its own `color`, which overrides the line
  color for that marker and its label.
- For a **`rect`** drawn via `send_shape`, `color` is the border stroke and
  `fill` is the interior. An empty `fill` means "no fill".

---

## 8. IDs, replacement, TTL, and clearing

- **Replace:** send a new message with the **same `id`**. The binary replaces
  the old drawing and refreshes its TTL.
- **TTL:** lifetime in **seconds**. After it expires the drawing disappears.
  Re-send before it expires to keep it alive (the radar re-sends every few
  seconds).
- **Clear / remove:** send the same `id` with `ttl: 0`. It is removed
  immediately.

```python
# Remove a previously drawn ring.
overlay.send_raw({"id": "radar_ring_0", "ttl": 0})
```

> The `id` you pass is namespaced: the wrapper prefixes it with a per-instance
> token and the caller's plugin path, so two plugins can reuse the same `id`
> without colliding. Always use **stable, unique ids** per logical shape so
> replacement and clearing behave predictably.

---

## 9. Marker / label font size

For vector shapes that contain labels, the binary needs a font size. If you do
not send `vector_font_size`, it is derived from `size`:

- `size: "normal"` → 16 px
- `size: "large"` → 20 px

```python
overlay.send_raw({
    "id": "labeled_marker",
    "shape": "vect",
    "vector": [
        {"x": 800, "y": 200, "color": "#FF3344", "marker": "cross", "text": "Target"},
    ],
    "size": "large",          # or set "vector_font_size": 20
    "ttl": 4,
})
```

---

## 10. Full example — a radar

A radar = a set of concentric circles (rings) + a center marker + a north
indicator + distance markers with labels. The pieces compose from the building
blocks above:

```python
import math

def circle_points(cx, cy, radius, segments=48):
    return [
        {"x": int(cx + radius * math.cos(math.radians(360.0 / segments * i))),
         "y": int(cy + radius * math.sin(math.radians(360.0 / segments * i)))}
        for i in range(segments + 1)
    ]

def draw_radar(overlay, cx, cy, max_radius, rings, north_deg):
    # Concentric rings, one id each.
    for i, r in enumerate(rings):
        overlay.send_raw({
            "id": f"radar_ring_{i}",
            "shape": "vect",
            "vector": circle_points(cx, cy, r, segments=48),
            "color": "#4488FF",
            "ttl": 4,
        })

    # Center marker.
    overlay.send_raw({
        "id": "radar_center",
        "shape": "vect",
        "vector": [{"x": cx, "y": cy, "color": "#00FF88", "marker": "circle"}],
        "ttl": 4,
    })

    # North indicator: a short line from the center toward `north_deg`.
    r0 = rings[0] if rings else max_radius
    x1 = cx + (r0 - 5) * math.cos(math.radians(north_deg))
    y1 = cy + (r0 - 5) * math.sin(math.radians(north_deg))
    x2 = cx + (r0 + 5) * math.cos(math.radians(north_deg))
    y2 = cy + (r0 + 5) * math.sin(math.radians(north_deg))
    overlay.send_raw({
        "id": "radar_north",
        "shape": "vect",
        "vector": [
            {"x": int(x1), "y": int(y1)},
            {"x": int(x2), "y": int(y2)},
        ],
        "color": "#00FF88",
        "ttl": 4,
    })

def clear_radar(overlay, ids):
    for shape_id in ids:
        overlay.send_raw({"id": shape_id, "ttl": 0})
```

---

## 11. Checklist for an agent

- Import the module with the two-name try/except fallback; treat `None` as
  "overlay unavailable".
- Prefer `send_raw` with a **vector shape** (`shape: "vect"`) for any geometry.
- Use `send_shape(shape="rect", …)` only for a plain rectangle.
- A point with a `color` becomes a **marker**; without one it is a **line
  vertex**.
- Draw circles as a **vector polygon** (48 segments is smooth).
- Give every distinct shape a **unique, stable id**; reuse the id to update,
  send `ttl: 0` to remove.
- One mode per message — never mix text and shape keys.
- Coordinates are absolute screen pixels; keep shapes inside the overlay area.

---

## 12. Shape protocol reference (from the C++ binary)

This is the authoritative description of the shape protocol, taken straight from
the overlay's C++ (`svgbuilder.cpp`, `drawitem_t`). It exists because the shape
path is old, undocumented, and easy to get wrong.

### 12.1 Recognised shape values

Only two values of the `shape` field are drawn:

| `shape` value | Renders as | Fields used |
| --- | --- | --- |
| `"rect"` | `<rect>` at `x,y` with `w`×`h`. | `x`, `y`, `w`, `h`, `color` (stroke), `fill` (interior) |
| `"vect"` | Vector shape: points joined by lines, markers at colored points. | `vector`, `color`, `vector_font_size` |

Any other `shape` value draws **nothing**. `isShapeVector()` is true only when
`drawmode == shape` **and** `shape == "vect"`.

> Gotcha: you must send **both** `shape: "vect"` **and** `vector: [...]`. Sending
> `vector` alone sets the draw mode to shape but leaves `shape` empty, so
> `isShapeVector()` is false and nothing is drawn.

### 12.2 Shape message fields (what the binary parses)

From `parseJsonString`, these JSON keys map onto the shape:

| JSON key | C++ field | Notes |
| --- | --- | --- |
| `shape` | `drawshape_t.shape` | `"rect"` or `"vect"`. |
| `vector` | `drawshape_t.vect` | Array of point objects. |
| `color` | `drawitem.color` | Stroke for lines and markers. |
| `fill` | `drawshape_t.fill` | Interior fill of a `rect`. Empty → `none`. |
| `x`, `y` | `drawitem.x`, `.y` | Base corner; **ignored for `vect`** (each point is absolute). |
| `w`, `h` | `drawshape_t.w`, `.h` | Only used by `rect`. |
| `vector_font_size` | `drawshape_t.vector_font_size` | Marker/label font px. Defaults to 16. |
| `ttl` | `ttl` | Lifetime in seconds. |
| `id` / `shapeid` / `msgid` / `svgid` | `id` | Any of these set the id. |

A message carries **one** draw mode. Mixing shape keys with text keys (`text`,
`size`, `font_size`) in one dict makes the binary reject it.

### 12.3 Points and markers (`TMarkerInVectorInShape`)

Each entry in `vector` is a point:

```json
{"x": 800, "y": 200, "color": "#FF3344", "marker": "cross", "text": "Target"}
```

- `x`, `y` — **required** integers.
- `color` — **required to make a marker**. A point without `color` is only a
  line vertex (`IsSet()` = `!color.empty()`).
- `marker` — the point's `type`: `"circle"` or `"cross"`.
- `text` — optional label (`HasText()` = `!text.empty()`).

Marker rendering:

| Marker | Renders as |
| --- | --- |
| `"circle"` | Outline `<circle cx cy r="4" fill="none" stroke=color stroke-width="1">`. |
| `"cross"` | An X: two diagonal 4px lines, `stroke=color`. |
| `text` only | A text label at `x+5, y`, `color=color`, font = `vector_font_size`. |

### 12.4 How points connect (`ForEachVectorPointsPair`)

Points are processed in order:

1. If a point has a `color`, draw its marker **at that point** (on top of lines).
2. Connect consecutive points with a line: 0→1, 1→2, 2→3, …

Consequences:

- **1 point** → marker only, no lines.
- **2 points** → one line segment.
- **N points** → an open/closed polyline.
- To **close** a polygon, repeat the first point at the end.

A **single-point** vector gets a special bounding box so its label has room:
`width = windowWidth / 4`, `height = 2·4 + 1 + font_size`.

### 12.5 Coordinate system

- Incoming coordinates are **screen-space**; each `vect` point is an **absolute**
  screen position (`x`, `y`, `w`, `h` are ignored for `vect`).
- `BuildSvgTask` finds the bounding box (`minX/minY`) of the points, translates
  so the corner becomes `0,0`, and wraps the output in
  `<g transform='translate(-minX, -minY)'>`. Non-`vect` `rect` uses `drawitem.x/y`
  as that corner.
- The overlay renders a small local SVG and shifts it back to screen coords —
  this is why you always send absolute screen pixels.

### 12.6 Constants and defaults

| Symbol | Value | Meaning |
| --- | --- | --- |
| `kMarkerHalfSize` | `4` | Marker radius / half-length of a cross. |
| `kStrokeWidth` | `1` | Stroke width for lines and markers. |
| `kTextOffsetX` / `kTextOffsetY` | `1` / `0` | Text label offset from a marker. |
| `kNormalFontSize` | `16` | Default marker/label font size. |
| `kDeltaFontDifference` | `4` | `large` = `16 + 4 = 20`. |

`getFinalFontSize()` returns `vector_font_size` if set, else `16` (or `20` if
`size == "large"`).

### 12.7 Ids, TTL, dedup

- Shapes are stored in `draw_items_t = map<id, drawitem_t>` — keyed by id.
- **Replace:** send the same id again (refreshes TTL).
- **Clear:** send `ttl: 0`.
- **Auto-id:** a message with no id gets `"AUTOID:" + counter`; if it also has a
  negative ttl it is forced to `60` s, because an id-less message can never be
  cleared.
- **Dedup:** `isEqualStoredData` compares `drawmode, color, text, shape, svg, x,
  y` — two identical shapes collapse into one.

### 12.8 The `fill` bug (fixed)

`makeSvgShape` used to hardcode `fill='none'` for `rect`, so the parsed `fill`
field was never applied — filled background frames never rendered. The binary
now uses `drawshape_t.fill` (empty → `none`), so passing a real fill color fills
the rectangle. This doc assumes the fixed binary.
