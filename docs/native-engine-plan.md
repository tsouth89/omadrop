# Omadrop native engine plan

> **Status:** Completed in v0.3. This file records the native-engine buildout.
> Current product work is tracked in the [post-v0.3 roadmap](roadmap.md).

## Goal

Build an original visual engine whose rhythm, instrumentation, and song
structure are visible in the composition. Keep projectM as a compatibility and
reference backend until the native path is proven.

## Product rules

1. A scene keeps a stable focal subject for several bars.
2. Kick, snare, hats, sustained bass, and harmonic material have different
   visual roles.
3. Beat phase creates anticipation before a hit, not only motion afterward.
4. Bars develop a composition. Phrases change its state. Sections transform
   or replace its topology.
5. Audio changes the scene's own geometry. It does not add generic flashes,
   rings, or camera shake over every scene.
6. ASCII is a final material pass over the continuous field.
7. Every scene must pass deterministic replay and live motion review.

## Architecture

```text
PipeWire -> AudioFeatureBus -> MusicFrame -> Director -> RenderBackend
MPRIS   -> TrackState -----------------------^            |
                                                         v
                                      feedback and scene render
                                                         |
                                                         v
                                             ASCII/native material
```

`MusicFrame` is the only musical interface exposed to a visual backend. The
projectM and native backends receive the same clock and track state so their
output can be compared under identical input.

The native renderer uses separate passes for scene injection, feedback
advection, transition composition, and display material. The first vertical
slice combines injection and feedback while the interface is validated.

## Delivery stages

### Stage 1: vertical slice

- [x] Add the renderer-facing `MusicFrame` contract.
- [x] Add a selectable native renderer, initially behind `OMADROP_ENGINE=native`.
- [x] Move native GLSL into external shader files.
- [x] Implement HDR ping-pong feedback.
- [x] Implement the first original Depth Tunnel.
- [x] Preserve the projectM compatibility and A/B path.
- [x] Add a `MusicFrame` unit test.
- [x] Add a deterministic native render harness.
- [x] Add initial role-strength and spatial-separation measurements.
- [x] Add event latency and multi-frame landmark-continuity measurements.

### Stage 2: complete musical input

- [x] Add a 32-band log-spaced spectrum and per-band flux.
- [x] Add native harmonic and percussive estimates.
- [x] Add stereo width, spectral centroid, and energy direction.
- [x] Attach monotonic timestamps and configured presentation delay to audio features.
- [x] Replay full-song raw fixtures through `MusicFrame`.

### Stage 3: finish Depth Tunnel

- [x] Separate focal subject, background medium, and accents.
- [x] Improve perspective and reduce radial symmetry.
- [x] Establish calm, developing, driving, peak, and releasing states.
- [x] Make downbeats readable without making every beat larger.
- [x] Tune original and ASCII materials independently.
- [x] Carry album-cover structure into the first tunnel state.

### Stage 4: expand the native visual set

- [x] Build Centrifuge around a stable aperture, frame, and counter-rotating shells.
- [x] Build Wire Organism around a persistent filament and attached loops.
- [x] Extract shared scene primitives only after all three scenes work.
- [x] Define topology-preserving transitions among the three scenes.

### Stage 5: director and recurrence

- [x] Drive scene lifecycle from energy direction, phrases, and sections.
- [x] Recall a topology when a musical section returns while varying its details.
- [x] Schedule transitions on structural boundaries and preserve a shared landmark.
- [x] Keep manual next, previous, and timeline controls deterministic.

### Stage 6: cutover

- [x] Run the native engine at a stable 60 FPS on supported hardware.
- [x] Synchronize one musical state across all displays.
- [x] Handle output-device changes without restarting.
- [x] Match current cover, palette, ASCII, and installation behavior.
- [x] Make native the default and retain projectM as classic mode.

### Stage 7: launch polish and visual breadth

- [x] Expand the native set to ten visually distinct scene grammars.
- [x] Guarantee deliberate multicolor output even for neutral album artwork.
- [x] Score scene choices against the current musical texture.
- [x] Avoid recent scenes while preserving recurring-motif recall.
- [x] Render only active and incoming scenes to retain GPU headroom.
- [x] Gate paired startup until every borderless window is placed and fullscreen.
- [x] Fade the complete display pair in and out without exposing renderer setup.
- [x] Route every interactive control through the paired-display leader.
- [x] Audit color separation, rhythmic roles, continuity, and landmarks for all ten scenes.

## Acceptance tests

The current pixel-motion audit remains useful but is not sufficient. The
native audit will additionally measure:

- peak motion time relative to known kick, snare, and hat events;
- spatial and directional separation among those roles;
- motion during idle periods and false responses;
- correlation between beat phase and visible motion;
- lifetime and continuity of the focal landmark;
- section-change timing and recurring-section topology recall;
- frame pacing and GPU time at native and ASCII output.

The final manual check is a muted clip: a viewer should be able to tap the beat,
distinguish major percussion roles, and mark section changes without hearing
the track.
