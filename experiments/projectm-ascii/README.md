# projectM to ASCII spike

This experiment proves the replacement rendering architecture:

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

The current program feeds deterministic synthetic stereo audio and renders
eight seconds as fast as possible. It is an architectural test, not the live
Omadrop application.

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

After 13 to 15 seconds, scenes wait for the next detected bass onset and begin
a 6.5 second dual-renderer morph on that boundary. Both presets remain live
through the dissolve. A six-second deadline handles
quiet passages. Press `n` to skip immediately and Escape to quit. `[` and `]` adjust audio/video
alignment by 10 ms while listening. `OMADROP_SYNC_MS` sets the initial delay;
Bluetooth outputs default to 180 ms and wired outputs to 35 ms. Manual changes
are saved in `$XDG_CONFIG_HOME/omadrop/sync-ms`.

Press `p` to return to the previous preset while auditioning the curated set.
Automatic changes randomly choose within the track's slowly measured calm or
driving energy class. The current preset and four most recent presets are
excluded, preventing obvious repeats and short loops.

Press `a` to switch between Omadrop ASCII and the original MilkDrop rendering.
Press F11 to toggle fullscreen.

Press Escape to exit. The process prints `audio: PipeWire` when nonzero sink
samples arrive. During silence it uses a synthetic development signal and
prints `audio: synthetic fallback`.

## Findings

- The system libprojectM 4 C API embeds cleanly in an SDL OpenGL context.
- The installed preset corpus contains more than 4,000 `.milk` files.
- Coherent MilkDrop structures survive a braille-style final pass.
- Point sampling loses 1px waves and outlines. Per-dot max pooling preserves
  them and should inform the production GPU composite shader.
- The old hardcoded procedural scenes are no longer the product path.
- A live, GPU-only projectM-to-ASCII handoff works in an SDL OpenGL window.

## Next

- Render continuously into a shared GPU texture.
- Replace the CPU PPM conversion with the existing glyph-atlas display pass.
- Feed real PipeWire PCM into `projectm_pcm_add_float()`.
- Add MPRIS cover art as an Omadrop-controlled intro and transition layer.
- Curate a known-good preset subset before enabling arbitrary preset loading.
