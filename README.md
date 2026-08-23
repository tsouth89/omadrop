# Omadrop

MilkDrop reborn for Omarchy.

Omadrop is a native Linux music visualizer that begins with the current album
cover, dissolves it into a living feedback world, and conducts that world from
the music playing through PipeWire. Its optional GPU ASCII material gives
classic MilkDrop presets an unmistakable Omarchy finish.

[omadrop.com](https://omadrop.com)

## What works

- Real MilkDrop preset execution through libprojectM
- PipeWire capture from the active audio output
- MPRIS album art with a held intro and ribbon dissolve
- Color-aware ASCII cover sampling that preserves saturated artwork
- Kick, snare, hat, bass, mid, and treble analysis
- Scene-aware movement that deforms preset geometry
- Smooth, music-aware preset transitions
- Randomized curated openings with repeat avoidance
- Optional ASCII or traditional MilkDrop rendering
- Paired fullscreen output across multiple Hyprland monitors

Omadrop is in active development. The current preset rotation is intentionally
small while each scene is tuned for coherence, musical response, and ASCII
readability.

## Requirements

- libprojectM and the classic projectM preset collection
- SDL2, GLEW, and OpenGL 3.3
- PipeWire
- libpng
- ImageMagick
- curl and GLib

Run the environment check before building:

```bash
./bin/omadrop-doctor
```

## Build and run

```bash
./experiments/projectm-ascii/build.sh
./bin/omadrop
```

Omadrop fills every connected monitor by default. Closing either window closes
the pair.

```text
Super + Shift + V  toggle Omadrop
Super + Alt + V    hide or restore the secondary display
Esc                quit
A                  toggle ASCII and original MilkDrop
N / P              next or previous preset
[ / ]              adjust audio sync by 10 ms
F11                toggle fullscreen
```

Use `./bin/omadrop --single` for one display or `./bin/omadrop --all` for every
connected display.

## How it works

```text
PipeWire audio  ->  analysis and beat clock  ->  libprojectM feedback
MPRIS artwork   ->  hold and dissolve        ->  Omadrop ASCII material
```

MilkDrop presets remain programs, not static configurations. Omadrop preserves
their recursive feedback, shaders, waves, and shapes, then adds a scene-aware
conductor that maps musical roles onto their existing structures. The ASCII
material is applied last so fine lines and feedback trails survive accumulation.

## Development

Read [NOTES.md](NOTES.md) before changing the renderer. It documents the audio
fixtures, visual testing workflow, architecture decisions, and silent failure
modes already encountered.

```bash
cd experiments/projectm-ascii
./build.sh
./audio-features-test
./audio-queue-test
./preset-profiles-test
./preset-adapters-test
```

The landing page is an Astro project in [`site/`](site/):

```bash
cd site
npm install
npm run dev
```

Production builds run with `npm run build`. Cloudflare Pages deploys the
`site/dist` output from the `master` branch and serves it at
[omadrop.com](https://omadrop.com).

## Presets and attribution

Omadrop does not bundle the projectM preset library. Presets are loaded from the
user's installed projectM collection and remain subject to their respective
authors and licenses. projectM is a separate project and is not affiliated with
Omadrop.

## License

No open-source license has been selected yet. The repository is public for
development and evaluation, but no permission to redistribute or modify the
Omadrop source is granted until a license is added.
