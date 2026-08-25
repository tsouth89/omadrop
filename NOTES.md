# Omadrop — working notes

Read this before changing anything. It exists so a future session does not
re-derive findings that cost real debugging time. `README.md` is the manual;
this is the engineering log and the roadmap.

---

## Current implementation snapshot

The production path is now `experiments/projectm-ascii/projectm-ascii-live`,
an SDL/OpenGL application using libprojectM. `bin/omadrop` launches it on every
connected Hyprland monitor by default and treats those windows as one paired
session. Quickshell remains only as the explicit `OMADROP_LEGACY=1` recovery
path.

Implemented in the native path:

- PipeWire sink capture and six-role audio analysis
- bounded audio buffering that preserves normal packet bursts and drops only
  excess backlog after render stalls
- output-specific audio/video calibration and raw-audio analyzer replay
- native beat-confidence hysteresis, four-bar phrases, and sustained spectral
  novelty detection
- native section fingerprints that recall a topology family with a fresh preset
- authored per-preset kick, snare, and hat geometry deformation
- restrained structural scene scheduling with independently reacting dual renderers
- optional full-song timelines synchronized to MPRIS playback position
- stable visual motif recall when a section identity returns
- randomized curated selection with recent-preset exclusion
- MPRIS cover hold, ribbon dissolve, and album-derived scene palette
- color-aware cover glyph sampling that separates dot activation from color
  selection, preventing neutral highlights from erasing saturated artwork
- instant ASCII/original MilkDrop toggle
- paired multi-monitor launch, close, and secondary-display toggle
- Astro landing page deployed from `site/` through Cloudflare Pages

Current curated rotation:

1. Aderrasi, Contortion
2. Martin, Wire Dance
3. Aderrasi, Halls of Centrifuge
4. Martin, Night Cathedral
5. Aderrasi, Bitterfeld
6. Aderrasi + Geiss, Airhandler (Painterly Kaleidoscope 2)
7. Tokyo Corridor, shifter tumbling cubes remix
8. Unchained + Rovastar, Wormhole Pillars (Hall of Shadows mix)
9. Rovastar, VooV's Organic Light
10. fiShbRaiN, Crystal Glasses
11. shifter, Mandala
12. Geiss, Myriad Mosaics
13. EoS + Phat, Cubetrace v2
14. Krash + Rovastar, Cerebral Demons (Phat + EoS Moire remix)
15. The NG + Geiss + Flexi, The Waterfowl in the Rain
16. Phat + fiShbRaiN + EoS, Mandala Chasers remix

The next product work is broader top-tier preset qualification, per-preset
reaction tuning, transition polish, packaging, and a documented public install
path. The sections below preserve research and prototype lessons. Anything
describing `shell.qml`, `field.frag`, Genesis, or eight procedural scenes is
historical context rather than the active renderer.

The live product path deliberately has no AI runtime. Classical DSP provides
the rhythm hierarchy and online arrangement novelty, so Spotify and any other
system audio work without uploads, accounts, model files, or preprocessing.
Optional JSON timelines remain useful as an authoring oracle and regression
fixture, not as an end-user requirement.

Tempo estimation uses a sixty-second spectral-flux memory and slow consensus
across overlapping windows. This avoids letting a sparse intro, breakdown, or
outro pull the beat clock onto a temporary subdivision. Structural events have
a twenty-four-second minimum spacing in addition to the bar hierarchy, so fast
music does not cause frantic scene replacement.

---

## Where this is going (and how far off it is)

### Architecture decision: libprojectM engine, Omadrop material

The hardcoded scene library is no longer the product direction. A successful
spike in `experiments/projectm-ascii` renders real `.milk` presets through
libprojectM and converts the finished frame into braille-style dots. Both thin
feedback lines and large tonal custom shapes remain coherent after conversion.

Production direction:

    libprojectM preset + PCM -> continuous OpenGL frame
                            -> Omadrop glyph/material composite
                            -> Wayland layer surface

The installed corpus provides more than 4,000 presets immediately. Omadrop
will own PipeWire capture, MPRIS album transitions, preset curation, ASCII
materials, live controls, and eventual native presets. libprojectM supplies the
composition engine and `.milk` compatibility.

ASCII is a presentation mode, not a permanent restriction. The production UI
must allow instant switching between Omadrop's ASCII material and the original
MilkDrop composite. The live spike uses `a` for this toggle.

The native renderer now adds an Omadrop conductor after projectM. Six spectral
roles produce separate kick, snare, and hat envelopes. A preset profile maps
those envelopes into topology-specific deformation of the preset's own
objects, with a measured per-preset gain. This is intentionally not a ring,
flash, or camera-shake overlay.

The automatic director uses the same profiles for energy fit, topology family,
motion direction, ASCII density, dwell range, and transition duration. Recent
presets are excluded before a randomized choice among the best candidates.
Transitions begin on a confident bar boundary or a low-confidence bass onset,
with a deadline for quiet material.

The spike exposed a critical display rule: one point sample per braille dot
misses physical 1px waves and outlines. Each dot must pool its full source
subcell, preferably with a highlight-preserving max or weighted-max filter.

**The target:** MilkDrop for Omarchy — rendered in braille/ASCII. Not a
spectrum readout with effects layered on. A field that is *alive*: things
drifting, trailing, forming and dissolving on their own, that you can follow
with your eye, with the music shaping them rather than driving every frame.

**Historical state:** this describes the superseded Quickshell procedural
renderer. Its findings informed the native libprojectM path above.

### Historical composition architecture

A preset owns a complete visual world: its focal geometry, feedback behavior,
camera intent, musical mappings, album-art lineage, lifecycle, and compatible
successors. A scene function is only a reusable primitive inside that world.

The director must not choose arbitrary effects. Successors form a compatibility
graph, with musical state selecting between authored options. Presets run long
enough to develop, and transitions preserve a shared axis, topology, or motion
direction.

The first preset is **Genesis**. It samples the cover luminance and edges along
the same radial current that forms its abstract geometry. The cover therefore
becomes persistent strands instead of fading out while an unrelated field fades
in. This is the pattern every future preset family should follow.

Current transitional state:

- Genesis replaces the old generic rings scene at index 0.
- The shuffle bag is gone; a compatibility graph selects successors.
- Feedback zoom, rotation, and warp strength now follow the active preset.
- Preset dwell is 18-38 seconds with a four-second transition.
- The remaining seven fields are still provisional primitives, not finished
  premium presets.

Genesis now assigns musical streams to separate parts of one composition:

| input | visual role |
|---|---|
| bass ratio | radial depth pull |
| mid ratio | torque through the strand field |
| treble ratio | fragments shed from cover edges |
| HPSS percussive | travelling ridge inside the radial subject |
| HPSS harmonic | sustained body of the current |

Its lifecycle moves from recognizable cover structure through developing
current and increasing detail, then reduces new material during the transition
so feedback carries the existing world into its successor. Generic burst and
filigree layers are now weighted per preset instead of being stamped over every
composition.

### Frame feedback — IMPLEMENTED

This was the single biggest gap and it is now built. Kept here because the
reasoning explains the whole render architecture.

Every frame is currently rendered **from scratch** as a pure function of
(time, audio). There is no memory. That is exactly why nothing trails, nothing
persists, and nothing can be *followed* — there is no object, only a formula
evaluated fresh 165 times a second.

MilkDrop's defining trick is a **feedback framebuffer**: each frame samples the
*previous* frame through a slight warp, decays it, and draws new material on
top. Everything characteristic of it — trails, tunnels, smoke, structures that
form and drift and dissolve — falls out of that one mechanism. Per-frame
equations only steer it.

Implementing it here:

- Render the field into an offscreen `ShaderEffectSource` instead of straight
  to screen.
- Next frame, sample that texture through a warp (zoom/rotate/translate that
  varies slowly), multiply by a decay (~0.92-0.97), and add the new field.
- Braille-quantise **at the end**, as a display pass over the accumulated
  buffer, not per-scene.

That last point matters: the accumulator should hold a smooth continuous
field, and the braille grid should be a *view* of it. Quantising before
accumulating would stair-step the trails.

**How it is actually built.** A single `ShaderEffectSource` with
`recursive: true` — Qt double-buffers internally, so no manual ping-pong is
needed. `shaders/field.frag` renders the scene *and* samples the accumulator
through the warp; `shaders/display.frag` does the braille quantisation as a
view over it. The field pass is sized to exactly one texel per braille dot
(`cols*2 x rows*4`), so the display pass is a 1:1 lookup.

Tuning notes, all learned the hard way:

- **Decay is the trail length and it saturates fast.** 0.90 filled the frame
  with grey mush within seconds. ~0.80-0.82 holds structure without filling.
- **The additive term must be well under 1** (0.45). It is applied every frame
  on top of a decayed copy of itself; small numbers compound.
- **Zoom compounds too.** 0.978/frame is already strong outward flow — a few
  percent per frame is a lot after 60 frames.
- **Mask the edges** (`smoothstep` on distance to border) or the border smears
  inward forever.
- **Re-apply contrast in the display pass.** The accumulator saturates toward
  1, so without `smoothstep(0.17, 0.72, level)` every dot lights and the
  uniform-grey-haze problem returns.
- **Gate feedback by `dissolve`.** The cover reveal must stay crisp; trails
  belong to the abstract phase. Feedback ramps in as the cover devolves.

### Warp field and musical layers — IMPLEMENTED

**The warp must be per-pixel, not a global transform.** The first feedback
build used a single zoom+rotate for the whole frame, which slides the picture
as a rigid body. MilkDrop's warp is a per-vertex mesh: different regions flow
in different directions, and that is what shears structure into vortices and
filaments. `warpSample()` in `field.frag` evaluates it per pixel.

Two details that matter more than the constants:

- **Rotation must vary with radius.** Differential rotation winds structure
  into spirals; a constant angle just turns the frame.
- **Each spatial scale is driven by a different band** (bass -> large flow,
  mid -> medium lattice, treble -> fine detail), so the motion itself carries
  the arrangement.

**Layers bound to musical streams.** `burstLayer` (HPSS percussive -> expanding
rings from drum hits) and `filigreeLayer` (treble -> fine sparkle) composite
alongside the scene body. Everything answering one aggregate signal is what
made earlier builds read as a single thump no matter how well onsets were
detected.

### Waveform — IMPLEMENTED

`pw-spectrum.py` now emits a 128-point downsampled waveform per frame on a
`^`-prefixed line (0..1000, 500 = zero crossing). QML writes it into a 128x1
`Canvas` via `fillRect` (putImageData still does nothing) and `field.frag`
draws it as a **radial** trace, which sits better with the radial scenes than
a flat line and gets smeared into ribbons by the feedback warp.

**But do not DRAW it.** A drawn trace reads as an oscilloscope pasted over the
art -- a UI element sitting on top of abstract work, not part of it. It was
tried and removed. The waveform is instead used as **displacement in the warp
field**: the medium ripples with the actual signal, present everywhere and
legible nowhere as a line. Same "this is literally the audio" property,
without the decal.

General lesson: importing a MilkDrop *feature* is not the same as importing
the *principle* behind it. Ask what the feature is for before copying it.

**Layer weights compound.** Every layer is added on top of a decayed copy of
itself every frame, so the useful range is far smaller than it looks. Body
0.34, bursts 0.42, filigree 0.24, wave 0.52, decay ~0.75. Doubling any of
these fills the frame with haze within a couple of seconds.

### Packaging (historical Quickshell path)

The retired Quickshell prototype was made relocatable and followed the system
monospace, but its `.qsb`, Qt, and Python packaging notes no longer apply. The
current installer builds the native renderer, installs its Arch dependencies,
copies the bundled presets, and manages Omarchy shortcuts with backup and
rollback validation. A PKGBUILD remains future work.

### Next, in order

1. **Finish Genesis as one complete premium lifecycle.** Cover, structural
   handoff, development, peak, and release must read as one continuous event.
2. **Preset parameter layer.** Move authored camera, feedback, layer, palette,
   and response values out of scattered shader conditionals.
3. **Musical-state director.** Classify calm, building, driving, suspended,
   and releasing from novelty, HPSS, energy trend, and spectral shape.
4. **Topology-preserving transitions.** Morph rings into tunnel depth, tunnel
   spokes into spiral arms, streams into contours, and contours into lattice.
5. **Multiple composited layers.** Background medium, focal subject, and
   foreground accents, each selected by the preset rather than always enabled.
6. **Autonomous agents where composition calls for them.** Their flow and
   lifetime must reinforce the preset's dominant motion.
7. **Block/shade glyphs mixed into the atlas.** The texture still reads faintly
   vertical-striped because braille dots are 2 wide and 4 tall. Mixing
   `░▒▓█▀▄` gives smoother tonal steps. Needs the glyph index to exceed 256,
   so the cell texture needs a second channel.

---

## Motion review — `bin/motion-capture`

    ./bin/motion-capture [frames] [region]        env: SETTLE=<sec>

Every motion complaint this project has had was invisible in a still. This
bursts ~18fps of `grim` grabs from a small crop (grim is far faster on a
crop), montages them into a filmstrip, and prints the frame-to-frame
difference series. Flat line = steady motion; spikes = lurching. Use
`SETTLE=22` or more to capture past the cover reveal into the scenes.

**Build this before changing any visual.** Tuning motion from stills is what
made several earlier rounds go in circles.

## Testing methodology — use this, it is the only thing that worked

Screenshots of a moving target lie. Every real finding here came from one of
three techniques.

### 1. Deterministic A/B with the debug env vars

    OMADROP_SCENE=<0-7>   force one fully-devolved scene
    OMADROP_BEAT=1        pin the beat high
    OMADROP_FREEZE=1      freeze clocks AND audio
    OMADROP_TIME=<sec>    set the frozen musical time

`FREEZE` is what makes an A/B mean anything: without it the scene animates and
run-to-run drift (~5% of pixels) swamps whatever you are measuring.

### 2. Validate DSP offline against synthetic ground truth

The beat tracker was validated against generated click tracks at known BPM
(90/100/120/128/140/174 — all within 0.9 BPM, phase error 1-16 ms). The onset
and pulse detectors were tuned by replaying captured spectrum files. This
found bugs that were invisible on screen:

- `pulse` was firing **0.14x/sec** — once every seven seconds. Its only
  gesture in three scenes, and completely dead.
- Raw onset flux thresholding fires ~14x/sec and reads as jitter; the fix is a
  **refractory period**, not a higher threshold.

Capture a spectrum file with:

    timeout 15 ./bin/omadrop-source 128 60 > /tmp/spec.txt

### 3. Look at the picture

Pixel-diff metrics lie on sparse dot scenes — thin lines moving change few
pixels, so `AE` counts stayed at 0.05x while a gesture was obvious by eye. I
wasted a long stretch tuning against a number I had not validated. **When a
metric and your eyes disagree, believe your eyes and fix the metric.**

Also: `grim` will happily capture the desktop if the layer surface has not
mapped yet. Always wait:

    for _ in $(seq 1 40); do
      hyprctl layers | grep -q omadrop && break; sleep 1
    done

---

## Traps that cost real time

### Qt / Quickshell

- **`putImageData` does nothing.** Silently. With `Canvas.Image`, with
  `Canvas.FramebufferObject`, and via `getImageData`→modify→`putImageData`. No
  error, just a black texture. `fillRect` works. This is why the per-frame
  data-texture approach was abandoned for uniforms.
- **`hideSource: true` stops a `Canvas` from painting.** A plain `Item` (the
  glyph atlas `Grid`) is unaffected.
- **`QSG_RENDER_LOOP` defaults to a ~63fps cap** regardless of workload.
  `threaded` unlocks the display refresh (131fps+ measured). `bin/omadrop`
  sets it.
- **Bake shaders with GLSL ES targets.** Qt asks for 320/310/300/100es here;
  baking only `150,440` gives "No GLSL shader code found" and a black screen.
  `build.sh` has the right invocation.
- **Vertex and fragment uniform blocks must be identical.** Adding a uniform to
  one stage silently corrupts values in the other.
- **GLSL needs declaration before use.** Adding a helper below its first caller
  fails the bake — and the app then silently runs the *previously* baked
  shader, so captures look stale rather than broken.
- **GLSL ES will not index uniforms dynamically.** Hence the `bandAt()` chain.
- **Duplicate `Component.onCompleted` is a hard error** ("Property value set
  multiple times") and the config fails to load entirely.
- Careful with scripted edits: a `replace()` on a common line (`root.sceneBlend
  = 0`) matched in two places and duplicated a debug block into an animation's
  `onFinished`, referencing an out-of-scope variable.

### Rendering

- **Braille has a legibility floor.** Four dot rows need ~3 physical px each.
  Past ~112 rows on a 1440p screen the atlas glyph is minified and adjacent
  cells bleed into vertical stripes. Density is not the lever; framing is.
- Render atlas glyphs at ~1:1 with their on-screen size. Minifying a large
  atlas blurs the dots together.
- Inset the atlas UV (`mix(0.02, 0.98, frac)`) or linear filtering samples
  across cell borders.
- **An S-curve before dithering is what creates negative space.** Without it
  dark regions still light one or two dots per cell and the frame is a uniform
  grey haze.
- **Interleaved-gradient noise, not Bayer.** A Bayer matrix leaves a visible
  crosshatch at this dot pitch.
- **Hairlines vanish.** `smoothstep(0.90, 1.0, ...)` rendered at a mean of
  0.004. Bands must cover several dots.
- **Watch for singularities.** `0.34/r` at the tunnel centre aliases into grey
  mush exactly where the eye goes. Fade it and draw a vanishing point instead.
- **Alternate direction with `sin()`, never a parity step.** `mod(floor(x),2)*2-1`
  is discontinuous; every zone boundary burns a fixed circle into the frame.
- **Never blend a palette by angle alone.** Wedges converge to a point at the
  centre; with a dark palette entry that is a black blotch in every scene.
- **Phase shifts wrap.** Shifting `fract(x)` by 2.1 is really 0.1. Half a cycle
  is the maximum change.

### Audio

- **Drive visuals from attack, not level.** Modern masters are compressed;
  measured band means barely move (bass 0.27, mid 0.56, treble 0.53) while
  peaks reach 1.0. Level-driven visuals sit still.
- **Motion beats brightness.** Bass read as "synced" only because it moved
  geometry while mids/highs modulated brightness. Give every band a
  *displacement* at its own spatial scale.
- **Map frequency onto position.** Three band maxima cannot carry counterpoint;
  the whole frame pumps as one body and reads as thumping.
- **Not every gesture belongs on `onset`** (~4/sec, vocals trip it). Big
  gestures need the rarer bass `pulse` (~1.4/sec).
- **Tempo priors prevent octave errors.** Without the log-normal prior around
  120 BPM the autocorrelation peak lands on half/double time constantly.
- **Layer the time scales.** See README. This was the fix for "janky".

### Album art

- **Gamma brightening destroys saturation** (0.114 → 0.051 measured). Restore
  after with `-modulate`.
- **Let gamma go low enough.** A bright cover (mean 0.78) needs ~0.11; a floor
  of 0.25 leaves it washed out.
- **Trim flat borders first.** Covers inset in a black square ("Only One") make
  normalisation spend its range on the border and amplify grain into noise.
- **A cover palette needs a luminance floor.** Extraction returns near-blacks.
- `omarchy-transcode-ascii` is a *logo* tool — 1-bit threshold. Useless for
  photographs.

---

## Distribution

Version 0.1.0 ships with a user-local Omarchy installer. It installs Arch
dependencies through `omarchy pkg add`, builds the native renderer, and manages
user-owned Hyprland shortcuts with backup and rollback validation. A future AUR
package can build on this path. The renderer remains out of the shell process so
a heavy preset cannot stutter the desktop shell.
