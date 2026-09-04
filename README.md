# Omadrop

MilkDrop reborn for Omarchy.

Omadrop is a native Linux music visualizer that begins with the current album
cover, dissolves it into a living feedback world, and conducts that world from
the music playing through PipeWire. Its optional GPU ASCII material gives
Omadrop's original scenes and classic MilkDrop compatibility mode an
unmistakable Omarchy finish.

[omadrop.com](https://omadrop.com)

## What works

- Ten original feedback scenes, from Depth Tunnel and Wire Organism to Tidal
  Grid, Prism Garden, and Constellation Field
- A renderer-facing music model with distinct kick, snare, hat, harmonic,
  phrase, and section signals
- Real MilkDrop preset execution through libprojectM compatibility mode
- PipeWire capture from the active audio output
- High-resolution MPRIS album art with a clean held intro and ribbon dissolve
- Full-resolution cover underlays beneath ASCII dots, or clean cover
  presentation when ASCII is off
- Kick, snare, hat, bass, mid, and treble analysis
- Native beat, bar, phrase, and arrangement-change tracking
- A silence-gated visual heartbeat that reshapes each scene on the detected beat
- A fast onset pulse that preserves attacks with ambiguous instrumental timbre
- Automatic visual-family recall for recurring musical entrances
- Scene-aware movement that deforms preset geometry
- Authored Reactive Tunnel, Reactive Orbit, and Reactive Wire editions with
  distinct kick, snare, and hat motion
- Texture-scored native scene selection with recurring-section recall
- Coordinated borderless startup across every display
- Randomized curated openings with repeat avoidance
- Optional ASCII or traditional MilkDrop rendering
- Paired fullscreen output across multiple Hyprland monitors

Omadrop is in active development. Its native ten-scene set and 11-preset
projectM compatibility rotation are tuned for coherence, musical response, and
ASCII readability.

Current release: **0.3.0**

## Requirements

- libprojectM
- SDL2, GLEW, and OpenGL 3.3
- PipeWire
- FFTW
- json-c
- libpng
- ImageMagick
- curl, GLib, and jq
- FFmpeg and gpu-screen-recorder for demo capture
- GCC and pkgconf when building from source

Run the environment check before building:

```bash
./bin/omadrop-doctor
```

## Install on Omarchy

```bash
git clone --branch v0.3.0 https://github.com/btsouth/omadrop.git
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
A                  toggle ASCII and continuous rendering (remembered)
N / P              next or previous scene
[ / ]              adjust audio sync by 10 ms
F11                toggle fullscreen
```

When Omadrop spans multiple displays, these controls are synchronized. The
focused window forwards its input through the paired-display leader and every
window applies the same state. ASCII mode and per-output audio delay are saved
for the next launch. `OMADROP_ASCII=0` remains available as a temporary launch
override.

Use `omadrop --single` for one display or `omadrop --all` for every connected
display.

For a short recording-ready showcase of five strong scenes:

```bash
omadrop-demo
```

It showcases Spectral Ribbons, Constellation Field, Prism Garden, Wire Organism,
and Orbital Loom. It uses a shortened cover intro, holds each scene for at least
3.25 seconds, then starts its 1.75-second blend on a detected bar boundary. A
5.75-second fallback keeps the showcase moving when the beat clock is
uncertain. It closes cleanly after the final scene. Add `--single` to keep it on
one display or `--loop` to repeat the showcase.

To capture the showcase on the focused monitor without recording the terminal
before it or the desktop after it:

```bash
omadrop-demo-record
```

The recorder saves a timestamped 60 FPS MP4 in `~/Videos`. Pass an output path
as its only argument to choose another location. It begins on the cover intro
and stops inside the final fade. On Omarchy it temporarily enables Do Not
Disturb, clears visible popups, and restores the previous notification setting
afterward. The capture is remuxed to the video endpoint, and its tighter maximum
scene dwell keeps the finished clip near 30 seconds.

For a focused Reactive Tunnel test after installation:

```bash
OMADROP_ENGINE=projectm OMADROP_CONTORTION_ONLY=1 omadrop --single
```

Add `OMADROP_CLASSIC_CONTORTION=1` to the same command to run the preserved
classic preset against identical music.

For the spinning center and square-frame preset:

```bash
OMADROP_ENGINE=projectm OMADROP_HALLS_ONLY=1 omadrop --single
```

Add `OMADROP_CLASSIC_HALLS=1` to run the preserved Halls Of Centrifuge original.

For the authored Wire Dance edition:

```bash
OMADROP_ENGINE=projectm OMADROP_WIRE_ONLY=1 omadrop --single
```

Add `OMADROP_CLASSIC_WIRE=1` to compare the preserved original.

The original native visual set is the default renderer:

```bash
omadrop --single
```

This runs ten original scenes through the same PipeWire, MPRIS, cover-art,
palette, and ASCII pipeline. The opening scene is randomized, while paired
displays receive the same choice. Press `N` or `P` to transition between them. Use
`OMADROP_NATIVE_SCENE` with `depth-tunnel`, `centrifuge`, `wire-organism`,
`prism-garden`, `orbital-loom`, `tidal-grid`, `pulse-cathedral`,
`constellation-field`, `spectral-ribbons`, or `bloom-engine` to start with one
scene for review. Add `OMADROP_ASCII=0` to inspect its continuous native field. See
[`docs/native-engine-plan.md`](docs/native-engine-plan.md) for the staged
redesign plan.

Run the preserved projectM renderer with:

```bash
OMADROP_ENGINE=projectm omadrop --single
```

## Build from source

```bash
./experiments/projectm-ascii/build.sh
./bin/omadrop
```

## How it works

```text
PipeWire audio  ->  MusicFrame and director  ->  native feedback scenes
MPRIS artwork   ->  structure and palette   ->  continuous or ASCII material
```

Each native scene maps musical roles into its own geometry before the optional
ASCII material is applied. In compatibility mode, MilkDrop presets remain
programs rather than static configurations. Omadrop preserves their recursive
feedback, shaders, waves, and shapes for direct A/B review.

The live director is native signal processing, not a hosted AI service. It
learns the beat clock, groups bars into phrases, and looks for sustained texture
changes before replacing a scene. Similar section entrances recall the same
native topology while its continuous details keep evolving. There is no model
download, account, or song upload.

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
./preset-selector-test
./preset-adapters-test
./musical-structure-test
./music-frame-test
./native-scene-state-test
./native-renderer-test ../../shaders/native
./structure-timeline-test ./fixtures/manual-song.json
../../bin/reactivity-audit
```

The reactivity audit renders all three authored editions and their preserved
classics against isolated kick, snare, and hat fixtures. It fails unless all
three gestures are clearly above idle motion and materially stronger than the
corresponding classic preset.

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

Omadrop's rotation contains 11 presets selected from the classic projectM
collection. Authored derivatives retain the original author credits, and their
source presets are preserved for A/B testing. See
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) for the complete list and
rights notice. projectM is a separate project and is not affiliated with
Omadrop.

## License

Omadrop is released under the [MIT License](LICENSE). Bundled MilkDrop presets
are excluded from that license and retain their respective authorship and
rights.
