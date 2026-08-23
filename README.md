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

Current release: **0.1.1**

## Requirements

- libprojectM
- SDL2, GLEW, and OpenGL 3.3
- PipeWire
- FFTW
- libpng
- ImageMagick
- curl, GLib, and jq
- GCC and pkgconf when building from source

Run the environment check before building:

```bash
./bin/omadrop-doctor
```

## Install on Omarchy

```bash
git clone --branch v0.1.1 https://github.com/tsouth89/omadrop.git
cd omadrop
./install.sh
```

The installer uses `omarchy pkg add` for missing Arch dependencies, builds the
native renderer, installs it under `~/.local/share/omadrop`, and adds the
Omadrop shortcuts to the user-owned Hyprland bindings file. It backs up the
bindings and rolls them back if Hyprland reports a configuration error.

Run `./install.sh` again to update an existing installation. To omit automatic
package or shortcut setup, use `--no-deps` or `--no-bindings`.

Uninstall with:

```bash
~/.local/share/omadrop/uninstall.sh
```

Audio synchronization settings and cached covers are preserved.

## Run

```bash
omadrop
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

Use `omadrop --single` for one display or `omadrop --all` for every connected
display.

## Build from source

```bash
./experiments/projectm-ascii/build.sh
./bin/omadrop
```

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

Omadrop bundles six presets selected from the classic projectM collection.
Their original filenames and author credits are preserved. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the complete list and
rights notice. projectM is a separate project and is not affiliated with
Omadrop.

## License

Omadrop is released under the [MIT License](LICENSE). Bundled MilkDrop presets
are excluded from that license and retain their respective authorship and
rights.
