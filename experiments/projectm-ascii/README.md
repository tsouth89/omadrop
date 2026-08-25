# Native projectM to ASCII renderer

This directory contains Omadrop's active rendering architecture:

1. libprojectM renders a real `.milk` preset into an OpenGL framebuffer.
2. The frame is sampled as 2x4 braille subcells.
3. Each dot pools its full source area so thin MilkDrop geometry survives.
4. Native and ASCII-style frames are exported for direct comparison.

Build:

```sh
./experiments/projectm-ascii/build.sh
```

Run:

```sh
preset="presets/curated/Aderrasi - Contortion (Escher's Tunnel Mix).milk"
./experiments/projectm-ascii/projectm-ascii "$preset" /tmp/native.ppm /tmp/ascii.ppm
```

`projectm-ascii` remains the deterministic offline comparison tool.
`projectm-ascii-live` is the live Omadrop renderer.

Replay a raw 44.1 kHz stereo f32 sink capture through the production analyzer
with `audio-feature-replay CAPTURE.raw`. It reports percussion, tempo, clock
confidence, bar novelty, phrases, and structural boundaries for repeatable
real-song tuning.

## Live GPU proof

`projectm-ascii-live` captures the default PipeWire sink, feeds PCM directly to
libprojectM, copies projectM's completed frame into a GPU texture, and applies
the ASCII dot composite in an OpenGL shader:

```sh
preset='presets/curated/Aderrasi + Geiss - Airhandler (Kali Mix) - Painterly Kaleidoscope 2.milk'
./experiments/projectm-ascii/projectm-ascii-live "$preset"
```

Run the first curated rotating set with:

```sh
./experiments/projectm-ascii/run-curated.sh
```

Once the native beat clock is reliable, scenes breathe for at least six bars.
Sustained arrangement changes can then start a one to two bar dual-renderer
morph. If no strong change arrives, a phrase boundary after twelve bars advances
the scene. Low-confidence material keeps the bass-onset and deadline fallback.
Strong boundaries are spaced by at least twenty-four seconds. Similar spectral
entrances recall the same visual family while choosing a fresh compatible
preset, creating theme and variation instead of exact replay. Each preset
continues reacting to individual hits throughout. Press `n` to skip immediately
and Escape to quit. `[` and `]` adjust audio/video
alignment by 10 ms while listening. `OMADROP_SYNC_MS` sets the initial delay;
Bluetooth outputs default to 180 ms and wired outputs to 35 ms. Manual changes
are saved per output in `$XDG_CONFIG_HOME/omadrop/sync-by-sink/`, so switching
between Bluetooth and wired devices preserves separate calibration values.

Press `p` to return to the previous preset while auditioning the curated set.
Automatic changes choose within the track's slowly measured calm or driving
energy class, with a small variation among the strongest compatible candidates.
The current preset and seven most recent presets are excluded. If a recalled
family has no fresh variant, selection widens to its related visual group before
reusing a recent scene.

Press `a` to switch between Omadrop ASCII and the original MilkDrop rendering.
Press F11 to toggle fullscreen.

Press Escape to exit. The process prints `audio: PipeWire` when nonzero sink
samples arrive. `OMADROP_SYNTHETIC_AUDIO=1` forces a deterministic repeating
kick, snare, and hat fixture. `OMADROP_DISABLE_ART=1` skips the cover sequence,
`OMADROP_COVER_PATH` forces a local cover for visual comparison, and
`OMADROP_AUTO_NEXT_MS` requests one preset transition after the given number of
milliseconds. `bin/motion-capture` uses these controls to inspect the native SDL
renderer without manual input. `OMADROP_REACTION_SCALE=0` disables the authored
post-process movement for deterministic native-versus-authored A/Bs.

## Structure timelines

Timelines are an optional authoring and test override. Normal Spotify, browser,
and local playback uses the native live detector and needs no timeline or AI
runtime.

`OMADROP_TIMELINE_PATH` loads an optional full-song JSON timeline. While it is
active, section boundaries replace randomized dwell scheduling. Each section
identity receives a stable preset, so later occurrences of the same identity
return to the same visual family. MPRIS position keeps the timeline aligned
through pause and seek, and a seek across sections restores the target preset
immediately.

```sh
OMADROP_TIMELINE_PATH=./experiments/projectm-ascii/fixtures/manual-song.json \
  ./experiments/projectm-ascii/run-curated.sh
```

The optional `track.identity` must exactly match the player's MPRIS
`xesam:url`. Omit it only for a deliberately unbound test fixture. For a
frozen visual check, `OMADROP_TIMELINE_POSITION` overrides the player position
with a non-negative number of seconds. Invalid timelines fail at startup. If
no timeline is configured, or its identity does not match, the existing live
audio director remains active.

The minimum schema is:

```json
{
  "duration": 100.0,
  "track": {"identity": "file:///music/example.mp3"},
  "sections": [
    {"start": 0.0, "end": 20.0, "identity": "A", "label": "intro"},
    {"start": 20.0, "end": 40.0, "identity": "B", "label": "verse"}
  ]
}
```

Album art comes from MPRIS, holds for five seconds, then dissolves over five
seconds in coordinated ribbons. Each braille cell averages the cover's RGB and
luminance, applies a restrained cell-local tone curve, and caps dot activation
below a solid field. A dim original-color underlay preserves faces, typography,
and fine texture while the glyphs remain dominant. The underlay fades early in
the dissolve, and cover reactions stay restrained so hard hits do not distort
the artwork.

## Current findings

- The system libprojectM 4 C API embeds cleanly in an SDL OpenGL context.
- The installed preset corpus contains more than 4,000 `.milk` files.
- Coherent MilkDrop structures survive a braille-style final pass.
- Point sampling loses 1px waves and outlines. Per-dot max pooling preserves
  them and should inform the production GPU composite shader.
- The old hardcoded procedural scenes are no longer the product path.
- A live, GPU-only projectM-to-ASCII handoff works in an SDL OpenGL window.
- Per-preset family and reaction profiles are required. A universal overlay
  cannot make every MilkDrop composition feel musically intentional.
- Hit envelopes preserve transient strength, so harder detected hits produce
  stronger preset gestures instead of the same binary pulse.
- Album covers and preset frames need different color sampling. Brightest-pixel
  pooling preserves preset hairlines but can wash out cover palettes.

## Preset qualification

The review harness now uses the same 12x24 braille cell geometry, level
quantization, and authored exposure values as the live renderer. Build it, then
create a deterministic metrics table and labeled contact sheet:

```bash
./bin/preset-audit cache/preset-audit presets/curated/*.milk
```

This tooling is for maintainers. It adds no runtime service, account, model, or
AI dependency to Omadrop.
