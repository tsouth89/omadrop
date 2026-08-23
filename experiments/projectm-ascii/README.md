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
preset='/usr/share/projectM/presets/presets_stock/Rovastar - Fractopia (Fantic Dancing Lights Mix).milk'
./experiments/projectm-ascii/projectm-ascii "$preset" /tmp/native.ppm /tmp/ascii.ppm
```

`projectm-ascii` remains the deterministic offline comparison tool.
`projectm-ascii-live` is the live Omadrop renderer.

Replay a raw 44.1 kHz stereo f32 sink capture through the production analyzer
with `audio-feature-replay CAPTURE.raw`. It reports kick, snare, and hat rates,
impact range, BPM, and clock confidence for repeatable real-song tuning.

## Live GPU proof

`projectm-ascii-live` captures the default PipeWire sink, feeds PCM directly to
libprojectM, copies projectM's completed frame into a GPU texture, and applies
the ASCII dot composite in an OpenGL shader:

```sh
preset='/usr/share/projectM/presets/presets_bltc201/Aderrasi + Geiss - Airhandler (Kali Mix) - Painterly Kaleidoscope 2.milk'
./experiments/projectm-ascii/projectm-ascii-live "$preset"
```

Run the first curated rotating set with:

```sh
./experiments/projectm-ascii/run-curated.sh
```

After each profile's minimum dwell, scenes wait for a confident bar boundary
or bass onset and begin a one to two bar dual-renderer morph. Each preset keeps
its own musical reaction through the transition, with a deadline for quiet
passages. Press `n` to skip immediately and Escape to quit. `[` and `]` adjust audio/video
alignment by 10 ms while listening. `OMADROP_SYNC_MS` sets the initial delay;
Bluetooth outputs default to 180 ms and wired outputs to 35 ms. Manual changes
are saved per output in `$XDG_CONFIG_HOME/omadrop/sync-by-sink/`, so switching
between Bluetooth and wired devices preserves separate calibration values.

Press `p` to return to the previous preset while auditioning the curated set.
Automatic changes randomly choose within the track's slowly measured calm or
driving energy class. The current preset and four most recent presets are
excluded, preventing obvious repeats and short loops.

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

Album art comes from MPRIS, holds for five seconds, then dissolves over five
seconds in coordinated ribbons. Cover glyphs use peak luminance for dot
activation and a chroma-weighted representative color, preserving saturated
linework beside pale highlights.

## Current findings

- The system libprojectM 4 C API embeds cleanly in an SDL OpenGL context.
- The installed preset corpus contains more than 4,000 `.milk` files.
- Coherent MilkDrop structures survive a braille-style final pass.
- Point sampling loses 1px waves and outlines. Per-dot max pooling preserves
  them and should inform the production GPU composite shader.
- The old hardcoded procedural scenes are no longer the product path.
- A live, GPU-only projectM-to-ASCII handoff works in an SDL OpenGL window.
- Per-preset topology and reaction profiles are required. A universal overlay
  cannot make every MilkDrop composition feel musically intentional.
- Hit envelopes preserve transient strength, so harder detected hits produce
  stronger preset gestures instead of the same binary pulse.
- Album covers and preset frames need different color sampling. Brightest-pixel
  pooling preserves preset hairlines but can wash out cover palettes.

## Next

- Qualify more top-tier presets that remain coherent under ASCII conversion.
- Tune musical roles and reaction gain per qualified preset.
- Continue improving topology-preserving transitions.
- Add packaging and an end-user install path.
