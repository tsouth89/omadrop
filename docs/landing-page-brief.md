# omadrop.com implementation brief

## The decision

Build a single, fast product page around captured Omadrop output. Do not
recreate the visualizer in JavaScript and do not use generated psychedelic
art. The product is already the strongest art direction available.

There is no site application or deploy configuration in this repository yet.
Start the web build only after the hero master and poster below exist. Use a
static Astro site with plain CSS and a small amount of client JavaScript. It
keeps the page easy to host anywhere, permits componentized sections, and does
not require a permanent browser runtime for decoration.

Suggested location when media is ready:

```text
site/
  src/pages/index.astro
  src/styles/global.css
  src/components/Hero.astro
  src/components/DemoRail.astro
  src/components/Install.astro
  public/media/
  media-source/              # lossless masters, not deployed
```

## Creative direction

The page should feel like Omarchy opened a portal, not like a SaaS launch
template.

- Near-black background, warm off-white type, and one album-derived accent at
  a time. Let the footage supply the color.
- Monospace throughout. Use the visitor's system monospace first so it feels
  native on Omarchy.
- Braille dots and terminal rules form the interface material. Large body copy
  remains normal text for legibility.
- No fake terminal windows, glass cards, gradients behind every section, stock
  waveforms, neon rings, or generic generated art.
- Motion has two speeds: slow ambient drift and precise beat accents. Scroll
  must never scrub music footage or fight its rhythm.
- The wordmark is lowercase `omadrop` in a restrained monospace treatment. A
  compact braille mark can be derived from a real captured frame after the
  frame is selected.

Omarchy itself supports braille ASCII branding, so this is a direct extension
of its visual language rather than a decorative filter. Reference the official
[Omarchy branding guide](https://github.com/basecamp/omarchy/blob/quattro/manual/41-branding.md),
but do not copy the Omarchy logo or imply this is an official bundled feature
until that is true.

## Page choreography

### 1. Hero: the cover becomes the world

Full viewport, no browser or desktop chrome. The real Omadrop intro plays
behind the copy: recognizable album cover, braille conversion, dissolve, then
one coherent preset takes over. The first frame is nearly still so the headline
can land before the image develops.

Copy:

```text
omadrop
see the music

MilkDrop, reborn for Omarchy.
[ Get Omadrop ]  [ View source ]
```

Keep the release call to action truthful. Before a public package exists, use
`Follow development` instead of `Get Omadrop`.

### 2. The proof rail

Three edge-to-edge clips, not feature cards:

1. `THE RECORD BECOMES THE VISUAL` shows the album-art handoff.
2. `EVERY PART HEARS SOMETHING DIFFERENT` shows a scene with visible kick,
   snare, and high-frequency responses.
3. `MILKDROP / OMADROP` toggles the identical moment between original and ASCII
   rendering.

Each label behaves like a terminal annotation at the image edge. Clips play
muted when visible, pause offscreen, and expose a sound button. Never autoplay
audible music.

### 3. The engine, in one sentence

Use one diagram-like line rather than a grid of technical claims:

```text
ALBUM ART -> PIPEWIRE AUDIO -> MILKDROP FEEDBACK -> OMADROP ASCII MATERIAL
```

Below it, one short paragraph explains that six spectral roles conduct the
preset's existing structures and that scene changes wait for musical
boundaries. Link to the source for details.

### 4. A living preset wall

Use six real stills from the qualified lineup as an asymmetric contact sheet.
Hovering a still reveals its preset name and a two-second silent motion loop.
The layout should have black space and unequal image sizes. It must not become
a uniform portfolio grid.

### 5. Install and close

End on a dark terminal-scale panel with the real install command once one
exists, requirements, source link, and the `Super + Shift + V` shortcut. The
background reprises the opening cover as a sparse braille ghost. Final line:

```text
YOUR MUSIC. YOUR RECORDS. YOUR DESKTOP, ALIVE.
```

## Required real media

Capture these before UI implementation. Use artwork and music that are owned,
licensed, or explicitly cleared for the public site.

The first hero master has now been reviewed. Its clean sequence is
`00:06.00-00:21.30`: the cover remains readable until the dissolve begins near
`00:12.50`, then a coherent preset world develops through the end. It does not
return to a loop-compatible frame, so the site plays it once and holds the
final frame. The derived public encodes are silent. Replace the source before a
public launch unless the displayed album artwork is cleared for promotional
use.

| Deliverable | Deployed path | Source master | Exact content |
|---|---|---|---|
| Hero loop | `site/public/media/hero.webm` and `hero.mp4` | `site/media-source/hero-master.mkv` | 14 to 18 seconds, 2560x1440 at 60 fps. Cover held, ASCII becomes legible, dissolve completes, one strong scene develops. End and start must share a dark visual state for a clean loop. |
| Hero poster | `site/public/media/hero-poster.avif` | frame from hero master | The last fully legible album-art frame before dissolution. |
| Music-response proof | `site/public/media/conductor.webm` and `conductor.mp4` | `site/media-source/conductor-master.mkv` | 8 to 10 seconds from the clearest scene. Include two strong kicks, a snare passage, and audible highs. No transition. |
| Render toggle | `site/public/media/toggle.webm` and `toggle.mp4` | `site/media-source/toggle-master.mkv` | 6 to 8 seconds. Same preset and musical phrase, toggle original to ASCII once and back once. |
| Seamless transition | `site/public/media/transition.webm` and `transition.mp4` | `site/media-source/transition-master.mkv` | One complete topology-aware transition with two seconds of stable footage on both sides. |
| Preset stills | `site/public/media/presets/01.avif` through `06.avif` | frames from clean masters | One compositionally distinct still from each currently qualified preset. Avoid similar center-weighted frames. |
| Social crop | `site/public/media/social.mp4` | crop from hero master | 1920x1080, 8 to 12 seconds, readable without sound. |
| Share image | `site/public/og.avif` | composed from hero poster | 1200x630, actual Omadrop output with a small `omadrop` wordmark and safe margins. |

Record lossless masters first. Derive web assets afterward so compression can
be retuned without repeating a performance. Capture the application directly,
with no cursor, notifications, Waybar, terminal flash, or desktop edge visible.
The official Omarchy recorder is documented in its
[screenshots and recording guide](https://github.com/basecamp/omarchy/blob/quattro/manual/12-screenshots-recording.md).

Final encodes should target visually clean braille dots rather than an
arbitrary bitrate. Fine dot grids are unusually hard on video compression.
Inspect the hero at desktop width and on a real phone before accepting it.

## Interaction and quality gates

- The hero starts within 2.5 seconds on a normal mobile connection and always
  has a useful poster while video loads.
- `prefers-reduced-motion` shows the poster and disables ambient transforms.
- All meaning and calls to action remain available without video or JavaScript.
- Video pauses outside the viewport. Mobile receives a smaller encode, not a
  CSS-scaled desktop download.
- No layout shift when footage loads. Hero and proof media have fixed aspect
  ratios.
- Text contrast meets WCAG AA. Decorative braille is hidden from screen
  readers.
- Test Chromium and Firefox on Omarchy, plus Safari on iPhone.
- Lighthouse targets: performance 90+, accessibility 100, best practices 100,
  SEO 100 on the production build.
- The page must still look authored when every animation is paused. If it only
  works in motion, the composition is not finished.

## Build order

1. Capture and approve the hero master and poster.
2. Scaffold the static site and finish the hero at desktop and mobile sizes.
3. Capture the three proof clips and six lineup stills.
4. Build the proof rail, engine line, preset wall, and install close.
5. Add responsive encodes, reduced-motion behavior, metadata, and performance
   budgets.
6. Deploy a preview, review it beside the actual visualizer, then connect the
   domain.

The hero is the design gate. If it does not immediately communicate album art
becoming a music-driven ASCII world, typography and extra sections will not
rescue the page.
