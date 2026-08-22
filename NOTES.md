# Omadrop — working notes

Read this before changing anything. It exists so a future session does not
re-derive findings that cost real debugging time. `README.md` is the manual;
this is the engineering log and the roadmap.

---

## Where this is going (and how far off it is)

**The target:** MilkDrop for Omarchy — rendered in braille/ASCII. Not a
spectrum readout with effects layered on. A field that is *alive*: things
drifting, trailing, forming and dissolving on their own, that you can follow
with your eye, with the music shaping them rather than driving every frame.

**Current state: roughly 80% of a good visualiser, but not MilkDrop.** Cover
reveal, eight abstract scenes, beat tracking, HPSS, layered time scales — all
working. What it still is *not* is autonomous. The user's words: "animations
that are actually animated and doing stuff in the background, not tied to the
beat, that you can follow."

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

### Next, in order

1. **Waveform rendering.** MilkDrop draws the actual oscilloscope trace and it
   is a huge part of the identity. We currently keep NO time-domain data --
   the pipeline discards raw samples after the FFT. Needs a downsampled
   waveform on the wire and a curve drawn into the field.
2. **Song-structure awareness.** Beat tracking gives position in the bar, not
   position in the song; verse and chorus look identical. Novelty groundwork
   exists in `analysis.py`.
3. **Preset system.** MilkDrop's depth is its preset library, not its renderer.
4. **Autonomous agents.** A few hundred particles with their own velocities and
   lifetimes, advected by a slowly-evolving flow field, leaving trails in the
   accumulator. These are the things the eye follows. Position state can live
   in a small ping-ponged texture, or in QML JS if the count stays low.
2. **Multiple composited layers.** Background field + midground agents +
   foreground accents, each on its own time scale.
3. **Preset system.** MilkDrop's real depth is its preset library. A preset
   here = a named parameter set (scene weights, flow constants, palette rules,
   feedback warp). Makes the thing extensible without new shader code, and
   shareable — which matters for distribution.
4. **Per-band time constants** for the 32 bands (bass slow, treble snappy).
   Currently one shared attack/release curve across the spectrum.
5. **Block/shade glyphs mixed into the atlas.** The texture still reads faintly
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

## Distribution (not started)

The user wants this shareable. It is a standalone Quickshell app, so options
are a PKGBUILD/AUR package, or an Omarchy plugin that acts purely as a
launcher and settings surface. It cannot be a normal in-shell plugin because
`QSG_RENDER_LOOP` is process-wide and a heavy visualiser must not be able to
stutter the bar. That reasoning is in README under Framing/Architecture.
