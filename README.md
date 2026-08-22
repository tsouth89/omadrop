# ASCIIscope

A braille-grid music visualiser for Omarchy. Starts from the album cover and
devolves into abstract scenes, synced to what is playing.

> **Working on this? Read [`NOTES.md`](NOTES.md) first.** It holds the roadmap,
> the testing methodology, and every trap that has already cost a debugging
> session. Most of them fail *silently* — a black screen, a stale shader, a
> detector that never fires — so they are expensive to rediscover.

**Goal:** MilkDrop for Omarchy, in ASCII. Currently about 80% of a good
visualiser but not yet MilkDrop — see "Where this is going" in NOTES.md. The
key missing piece is **frame feedback**; nothing persists between frames, so
nothing trails or can be followed.

## Requirements

    qt6-shadertools   (qsb, for baking shaders)
    quickshell        (qs)
    python + numpy    (analysis engine)
    imagemagick       (album art prep)
    pipewire          (pw-record)

Run `./bin/asciiscope-doctor` to check them.

## Running

    ./bin/asciiscope

Keys, all live:

    esc / q   quit                     space  restart the sequence
    [ / ]     grid density             - / =  zoom (0.56 fits height, 1.0 full-bleed)
    ; / '     gain                     e      edge structure
    v         vignette                 t      tint
    n         next scene               1-4    jump to one scene
    , / .     scene pace
    d         pause the sequencer      p      print current look values

    ./build.sh              # rebake shaders after editing shaders/*.frag|vert

## How it renders

Everything visible is one fragment shader. The CPU's per-frame job is updating
a handful of audio uniforms -- there is no per-cell upload.

- 256 braille glyphs (U+2800..U+28FF) are baked once into an atlas texture.
- The shader evaluates the field at each cell's eight braille dot positions,
  ordered-dithers them, and packs the results into a glyph index 0-255.
- That index selects the **real font glyph** from the atlas, so it keeps the
  terminal aesthetic while running entirely on the GPU.

Each braille cell is a 2x4 dot matrix, so a 266x96 grid addresses 532x384
dots. That is what makes it read as art rather than as text.

## Hard-won notes

Four things cost real debugging time. All are silent failures.

1. **`putImageData` does nothing** in this Qt/Quickshell build -- with
   `Canvas.Image`, with `Canvas.FramebufferObject`, and via
   `getImageData`->modify->`putImageData`. No error, just a black texture.
   `fillRect` works. This is why the data-texture approach was abandoned.
2. **`hideSource: true` stops a `Canvas` from painting.** A plain `Item`
   (e.g. the atlas `Grid`) is unaffected, so the atlas can use it.
3. **`QSG_RENDER_LOOP` defaults to a ~63fps cap** regardless of workload.
   `threaded` unlocks the display refresh (131fps+ measured). `bin/asciiscope`
   sets it.
4. **Bake shaders with GLSL ES targets.** Qt asks for 320/310/300/100 es here;
   baking only `150,440` yields "No GLSL shader code found" and a black
   screen. `build.sh` has the right invocation.

Also: the vertex and fragment uniform blocks must be **identical**. Adding a
uniform to only one stage silently corrupts the values.

## Album art

`bin/art-prep <in> <out> [target-mean]` normalises a cover for dot rendering.
Covers vary enormously -- "Only One" has a mean luminance of 0.05 and insets
the photo in a black square. The script trims flat borders first (otherwise
normalisation spends its range on the border and amplifies grain into noise),
then solves iteratively for the gamma that lands the mean on a target.

**Gamma brightening destroys saturation.** Lifting a dark cover pushes colour
toward white -- measured, it roughly halves it (0.114 -> 0.051 on one cover).
Restore with `-modulate 100,265,100` after the gamma, and lean past the
original: many covers are muted and this is a visualiser, not a print proof.

**Let the gamma go low enough.** A bright cover (mean 0.78) needs gamma ~0.11
to come down to the target. A floor of 0.25 leaves it washed out; the range is
0.10 - 8.0.

Design decision: **stay faithful to the cover.** Tinting monochrome art toward
the theme accent is available (`t`) but off by default -- it reads as a filter
over someone else's artwork.

## Cover art follows the track

`Quickshell.Services.Mpris` supplies `trackArtUrl`; a change kicks off
`bin/art-fetch`, which caches by a hash of the URL (so a repeat track is
instant and QML's Image cache can never serve a stale cover) and pipes through
`art-prep`. Two cover textures are bound at once and `anim.w` crossfades
between them, so a track change dissolves rather than pops.

Note: there is one Image pair *per screen*, so every monitor reports the same
cover ready. `revealArt()` is guarded by path -- without that the toggles
cancel out on a multi-monitor setup and the fade never happens.

## Scenes

Four abstract fields the cover devolves into, all procedural in the fragment
shader:

| # | Key | Scene | Form |
|---|-----|-------|------|
| 0 | `1` | rings | concentric ripples, wavefront released on a kick |
| 1 | `2` | tunnel | perspective rings in depth plus angular spokes |
| 2 | `3` | waves | two drifting sources; their interference draws it |
| 3 | `4` | rain | drops develop the album cover as they fall |
| 4 | `5` | spiral | logarithmic arms; a beat kicks the winding |
| 5 | `6` | lattice | rectilinear grid, glowing nodes; cells snap on a beat |
| 6 | `7` | contour | topographic loops; a beat shifts the level set |
| 7 | `8` | streams | flow-field streaks; beats surge them along the flow |

Scene colour comes from the album's **palette**, not its picture. `art-fetch`
writes the five dominant colours alongside each cover as a `.pal` file and the
shader blends four of them by angle and radius.

This matters: sampling the cover directly drags its imagery into the middle of
every scene and buries whatever the scene is drawing (the tunnel's vanishing
point disappears entirely). Averaging many samples to hide the structure just
converges to grey. A palette keeps the record's colour with none of its form. `n` forces the next scene; see the director below. Scene order is a **shuffle
bag** -- a shuffled permutation of all eight, reshuffled when exhausted -- so
every scene plays before any repeats. Picking at random each time gives a
~1-in-7 chance of returning to the scene you just watched.

## Scene director

A scene runs for at least 8s and at most 18s (`,` / `.` adjust live). Inside that window, structural
novelty decides *when* -- and once a change is armed it is **held until the
next onset**, so the cut lands on a beat rather than mid-bar. That beat-snap
is the single biggest perceptual win: a change landing on a hit reads as
intentional even when the timing choice was arbitrary. Crossfade is 2.5s.

Novelty is the cosine distance between a ~0.45s and a ~5s EMA of the spectrum,
aggregated to **16 log-compressed bands**. Per-bin shape is far too noisy to
tell a section change from a cymbal. A second smoothing pass supplies
persistence -- a section change stays different, a transient does not.

`ASCIISCOPE_SCENE=<0-3>` jumps straight to one fully-devolved scene.

**Drive scene motion from a musical clock, not wall time.** `fx.z` accumulates
at `0.35 + energy*1.9 + onset*2.6`, so every scene's motion speeds up with the
track and lurches on transients. Scene motion on wall time can never look
synced no matter how many amplitudes you modulate -- the *rate* has to move.
Rain in particular was static: its fall speed, the most visible property it
has, ignored the audio entirely.

**Never blend a palette by angle alone.** `fract(angle/2pi + ...) * 4` makes
four colour wedges converge to a point at the centre of the frame. Combined
with a dark palette entry that is a black blotch pinned to the middle of every
scene. Lead the blend with radius instead, so there is no singularity.

**A cover palette needs a luminance floor.** Dominant-colour extraction happily
returns near-blacks (`#544A4A` on one cover). Any scene that paints with one
gets a dead region. Entries below 0.42 luminance are lifted, hue preserved.

**Layer the time scales, or it can only ever jitter.** The real cause of
"janky" was architectural: every visual parameter was a direct function of
instantaneous audio, so everything oscillated at beat rate and nothing
evolved. There was no long-form motion for a beat to *accent* -- the beat was
carrying 100% of the movement.

Three layers now:

| layer | period | source |
|-------|--------|--------|
| flow | 25-140s | `flowWarp()` + per-scene `lfo()` morphing shape parameters |
| section | 8-18s | scene changes on bar lines |
| accent | per beat | `beatWarp()`, amplitude cut from 0.42 to 0.17 |

`flowWarp` rotates, scales and drifts every scene continuously on mutually
irrational periods so the field never visibly repeats, and each scene morphs
its own shape constants (ring density, spoke count, spiral arm count, lattice
cell size, contour scale, stream band density) over 47-81s. Measured: 20-31%
of lit content changes over 25s from long-form motion alone, against a beat
accent of 0.17 -- evolution now outweighs the beat rather than the reverse.

The clock driving it runs on **slow** energy (~4s smoothing), not instantaneous
energy. A rate that tracks the audio frame-to-frame wobbles, and a wobbling
rate is itself a source of jank.

**Smoothness is envelope shape, not amplitude.** The beat impact decayed at
`exp(-phase * 5.5)`, which is spent by 40% through the beat -- that reads as a
twitch. `exp(-phase * 2.6)` carries the energy across the whole beat as a
swell. Likewise, one EMA gives an exponential jerk; two in series give an
S-curve, which is what actually reads as smooth.

**Not every gesture belongs on `onset`.** Onsets fire ~4x/sec and vocals trip
them, so an expanding ring on onset reads as a constant pulsing circle that
pulls attention off everything else. Gestures that big need a rarer, stronger
beat: `pulse` is bass-weighted with a ~0.7s refractory, and only it drives the
ring wavefronts.

**Every scene needs a beat gesture, and it should be the most visible thing
that scene can do.** Spectral mapping alone gives texture, not rhythm. Pick
the gesture from the scene's own form: a grid snaps its cell size, a spiral
kicks its winding, a contour map shifts its level set, streaks surge along the
flow. A brightness flash is the lazy answer and reads as a thump.

**Hairlines vanish at this dot pitch.** The lattice used
`smoothstep(0.90, 1.0, ...)` and rendered at a mean of 0.004 -- effectively
invisible. Bands need to be wide enough to cover several braille dots.

### Beat response: four traps, all measured

1. **`pulse` was firing 0.14x/sec** -- once every seven seconds. It tested a
   raw per-frame bass delta against 0.10, but the measured p95 frame-to-frame
   delta on real material is **0.051**. A raw delta threshold cannot work at
   86fps. Rebuilt on flux-above-running-average like the onset detector:
   1.36/sec, beat level.
2. **Phase shifts wrap.** Shifting `fract(levels)` by 2.1 is really a shift of
   0.1 -- 95% of the gesture thrown away. Half a cycle is the maximum change.
3. **Low-frequency fields need large warps.** A 20% zone scale on contour was
   *visually indistinguishable* in an A/B. These fields are smooth; they need
   ~50% scale and ~0.75 rad rotation before anything reads.
4. **Pixel-diff metrics lie on sparse dot scenes.** Thin lines moving change
   few pixels, so AE counts stayed at 0.05x even when the gesture was obvious
   by eye. Use `ASCIISCOPE_FREEZE=1` + `ASCIISCOPE_BEAT=1` and *look* at the
   A/B; the number is only a hint.

Debug env vars: `ASCIISCOPE_SCENE=<0-7>` forces one devolved scene,
`ASCIISCOPE_BEAT=1` pins the beat high, `ASCIISCOPE_FREEZE=1` freezes both the
clocks and the audio so an A/B differs only by the beat.

**Beats push different elements in opposite directions.** `beatWarp()` splits
the frame into concentric zones that scale and counter-rotate against each
other, seeded differently per scene. A uniform push in one direction is the
twitch; opposing motion is what reads as the field answering the music.

**Alternate direction with a sine, never a parity step.** The first version
used `mod(floor(x), 2) * 2 - 1`, which is discontinuous: adjacent zones
scaled by +-48% with a hard jump between them, and every one of those
boundaries burned a fixed circle into the middle of the scene. `sin()`
alternates direction just as well with no seam. This applies anywhere a
continuous field is being shifted -- rings, tunnel depth, spiral arms, lattice
bands, contour levels, stream bands all had it.

Two scene lessons worth keeping:

- **Rain needs churn.** One drop per column reads as a uniform curtain, not
  rain. It needs per-column period/phase/tail, columns that rest between
  drops, a white-hot leading cell, and per-cell glyph scramble as the drop
  passes -- that scramble is the signature of the effect.
- **Watch for singularities.** The tunnel's `0.34 / r` blows up at the centre;
  past a small radius the ring frequency outruns the cell grid and aliases
  into grey mush exactly where the eye goes. Fade the rings out near the
  centre and draw a bright vanishing point instead.

## Framing

A 1:1 cover on a 16:9 screen cannot both fill the frame and stay whole.
`zoom = 1/aspect` (~0.56) shows all of it, and outside the cover the artwork is
**mirrored outward at low intensity** -- light spilling off the record rather
than black bars. Cropping to full-bleed makes covers unreadable; this keeps
the whole piece and still fills the screen.

## Sequence

Every cover runs the same arc, restarting on each track change: 2s reveal,
8s hold, then a 9s smoothstep dissolve into the abstract field.

## Analysis engine (`bin/analysis.py`)

Two things a three-band spectrum cannot give you.

**`BeatTracker`** estimates tempo and beat phase from the onset envelope.
Tempo comes from the autocorrelation of that envelope weighted by a log-normal
prior around 120 BPM -- without the prior the peak lands on half or double
time about as often as on the truth. Phase comes from correlating a pulse
train against the recent envelope, then runs through an accumulator so it
stays smooth between estimates (which re-run every 16 frames).

Validated against synthesised click tracks with known tempo:

| true BPM | estimated | phase error |
|----------|-----------|-------------|
| 90 / 100 / 120 / 128 / 140 / 174 | all within 0.9 BPM | 1-16 ms |

174 matters -- fast tempi are where octave errors normally show up.

**Why this is the difference between reacting and being composed.** With exact
phase, the beat response is a *function of position within the beat*, not a
detection of one that already happened. So it can never fire late:

    impact = exp(-phase * 5.5) * confidence     peaks ON the beat
    swell  = ramp from phase 0.55 to 1.0        builds INTO the next one

`beatWarp` uses `beat() - swell() * 0.55`, so the field winds up against the
coming beat and releases on it. Anticipation is what makes a gesture look
intended rather than like a flinch after the fact. Both fall back to the flux
`pulse` when confidence is low, so non-rhythmic material still works.

**`HPSS`** splits the spectrum into percussive and harmonic by median
filtering: sustained tones are stable over time, so a median across time
isolates them; percussive hits are broadband, so a median across frequency
isolates those. Soft Wiener masks split the frame. Drums drive the snap,
sustained tones drive the level -- they stop competing for one channel.

Real source separation (Demucs et al.) is neural, wants its own GPU and has
latency in seconds. Useless here. HPSS gets the part that matters for visuals
at a few thousand ops per frame.

Feature lines are emitted on the same stream as the bands, prefixed `~`:

    ~bpm;phase;conf;perc;harm;barPhase;secondsToNextBeat

Scene cuts land on a **bar line** when the tracker is locked, falling back to
any strong onset when it is not. A change that lands on the "1" reads as
composed; one landing on an arbitrary onset reads as a glitch.

## Audio

Audio motion must NOT be gated behind the dissolve. It was, and the entire
10s reveal-and-hold phase sat motionless while the spectrum was arriving
perfectly. Bass breathing, the standing ripple, onset punch and treble
threshold sparkle all run at every dissolve level.

**Map frequency onto position, or it thumps.** Driving everything from three
band maxima plus a global onset makes the whole frame flash and lurch as one
body -- it reads as thumping, not as music, no matter how well the onsets are
detected. The shader receives a **32-band spectrum** (`sp0`-`sp7`, four bands
per vec4, each band normalised against its own running peak) and every scene
maps it spatially:

| scene | mapping |
|-------|---------|
| rings | radius -> frequency; each ring answers to its own band |
| tunnel | depth -> frequency; energy travels along the tunnel |
| spiral | radius -> frequency along the arms |
| lattice | distance from each axis -> frequency |
| contour | field level -> frequency |
| streams | across-flow position -> frequency |
| waves | each of the three sources driven by a different band |
| rain | column -> frequency; columns fall and brighten by their band |

Bands use fast attack / slow release **per band**, so they decay independently
instead of moving as a block. Global multipliers (onset on brightness, onset
on the musical clock, whole-frame zoom) were the other half of the thump and
are now small.

**Motion beats brightness.** Bass read clearly because it moved *geometry*
(zoom, ripple displacement) while mids and highs only modulated brightness and
the dither threshold. Displacement is far more salient than luminance, so the
low end appeared "synced" and nothing else did. Each band now displaces at its
own spatial scale and rate: bass long and slow, mid medium and lateral, treble
short-wavelength fast chatter. Measured first -- adding per-band transients
barely moved the numbers (high/low movement ratio 0.83 -> 0.90), which is what
pointed at salience rather than signal.

**Sync on the axis the motion travels.** The tunnel mapped frequency to
*angle* while its movement runs along *depth*, so the response was
perpendicular to the thing being watched. Frequency now lies along the depth
axis -- vanishing point is the top of the spectrum, mouth is the bottom.

**Give each band its own gesture.** Bass owns the zoom punch and radial
ripple, mid a lateral undulation on a different axis and frequency, treble the
per-dot shimmer. When several bands drive the same parameter only the loudest
(bass) is legible and the visualiser looks like it only hears kick drums.

`onset` decays per second, not per frame -- a 0.88 per-frame decay vanishes in
about 20 frames at 165Hz.

**Drive visuals from attack, not level.** Measured on real material, the bands
peak near 1.0 but their means barely move:

    bass mean=0.27 peak=0.95 | mid mean=0.56 peak=0.87 | treble mean=0.53 peak=1.00

Modern masters are compressed, so absolute loudness is near-constant and
level-driven visuals sit still. Onsets come from **spectral flux** (sum of
positive bin-to-bin change), each band normalised against its own decaying
running peak.

Onset detection needs a **refractory period**, not a higher threshold. Tuned
against captured audio:

| threshold ratio | refractory | onsets/sec |
|---|---|---|
| 1.35 | none | 12.6 |
| 1.9 | none | 7.8 |
| 1.5 | 10 frames | ~3.7 |

Roughly 3-4/sec is beats plus some subdivision. Raw flux thresholding fires
~14x/sec and reads as jitter.

## Status

Done: GPU braille renderer, adaptive art prep, live audio uniforms
(bass/mid/treble/energy/onset), dissolve warp, MPRIS art following with
crossfade.

Next: musical feature extraction (onset/novelty/centroid) for scene changes,
the abstract scene library, and the scene director.
