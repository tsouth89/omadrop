# Omadrop roadmap after v0.3

## Product direction

Omadrop should turn a song into an authored visual performance. It must respond
at three time scales:

1. **Attacks:** kicks, snares, hats, and other transients change the image
   immediately and differently.
2. **Groove:** beats and bars establish repeated motion that makes the rhythm
   clear without reducing the scene to a spectrum display.
3. **Arrangement:** phrases, sections, breakdowns, returns, and peaks change
   composition, density, color, and scene choice.

The renderer should be nearly still when the audio is silent. Music must change
the scene's own geometry and behavior. Generic flashes, rings, zooms, and camera
shake are not substitutes for scene-specific response.

## Rules that do not change

- Ship fewer scenes when the alternatives are weaker.
- A new scene needs a distinct composition and motion system, not a new palette
  on an existing shader.
- Every scene needs a stable subject or spatial structure that can be followed
  over time.
- Color supports form. A scene must still read in grayscale.
- ASCII is a material applied to the same high-quality continuous image.
- Automatic direction respects the song. It does not change scenes to meet a
  timer when a musical boundary is available.
- Silence produces almost no activity.
- Launch, display synchronization, controls, cover art, and shutdown remain as
  polished as the visuals.
- Audio analysis stays local and deterministic.
- Automated metrics catch regressions. Human review decides whether a visual is
  beautiful enough to ship.

## The quality gate

Every scene, transition, and release must pass the same review.

### Music response

- Declare at least three primary musical inputs and the visible role of each.
- Produce a measurable response to each declared input within 100 ms of its
  presentation time.
- Keep each role spatially or behaviorally distinct from the others.
- Keep quiet-frame motion below 15 percent of typical active motion.
- Avoid continuous high response caused by long envelopes or false onsets.
- Preserve readable beat motion across sparse acoustic, dense electronic, and
  heavily compressed mixes.

The initial quantitative floor is 1.75 times quiet-frame motion for each
declared transient response. Existing baselines will determine stricter
per-scene thresholds. A high score does not compensate for ugly motion.

### Visual composition

- The still frame has a deliberate focal structure, balance, and negative
  space.
- Motion develops the composition instead of erasing it every frame.
- Brightness stays controlled during feedback accumulation.
- Album-derived palettes remain legible for neutral, dark, and oversaturated
  covers.
- Continuous and ASCII materials both preserve the identity of the scene.
- The scene remains strong during quiet, developing, driving, peak, and release
  passages.

### Variety

Review each scene on these axes:

| Axis | Examples |
| --- | --- |
| Composition | radial, horizontal, perspective, figure and ground, full field |
| Matter | line, surface, particle, fluid, volume, glyph |
| Motion | orbit, flow, growth, fracture, folding, wave, camera travel |
| Density | sparse, layered, dense |
| Depth | flat graphic, shallow field, deep space |
| Energy character | restrained, elastic, sharp, heavy, explosive |

A candidate does not ship when it matches an existing scene on most axes.
Contrast across the rotation matters as much as the quality of one scene.

### Operational quality

- No desktop, title bar, intermediate window, or setup frame is visible at
  launch or shutdown.
- Multi-monitor windows show the same musical state and control response.
- Manual scene changes do not disable automatic direction.
- Saved preferences survive updates and output-device changes.
- The 99th-percentile frame time stays below 18.5 ms at 1080p on the reference
  machine, with no sustained drop below 55 FPS at native display resolution.
- A deterministic recording contains only its approved audio source.

## Phase 0: v0.3.1 quality floor

Do not add scenes yet. Establish a reliable baseline for the ten that exist.

### Work

- Build a locked, rights-cleared replay set covering sparse percussion, dense
  rock, electronic music, acoustic music, vocals, quiet intros, breakdowns, and
  fast section changes.
- Add a scene scorecard that records role response, false activity, frame
  pacing, brightness, continuity, and transition behavior.
- Raise Spectral Ribbons and Constellation Field to the response level of the
  strongest current scenes without destroying their form.
- Review all ten scenes in grayscale, continuous color, ASCII, and at least six
  album-derived palettes.
- Add regression cases for launch while a track is already playing, pause,
  seek, track change, output change, display hotplug, and shutdown.
- Keep the new recording audio guard and add an automated opening-audio check.
- Fix installation and GPU compatibility issues reported after the v0.3 post.

### Exit criteria

- All ten scenes pass their declared response thresholds.
- No current scene fails the still-frame composition review.
- The launch and ending remain clean in every test configuration.
- There are no known release-blocking install, display, audio, or persistence
  defects.

## Phase 1: v0.4 music perception

Improve the information available to every scene before increasing the scene
count.

### Work

- Normalize transient and band response across quiet, loud, compressed, and
  bass-heavy masters.
- Improve downbeat, tempo, and half-time or double-time stability.
- Add rhythmic density, syncopation, spectral contrast, and tonal movement to
  `MusicFrame`.
- Add chroma and harmonic-change signals for color and structural movement.
- Separate short attacks from sustained percussion more reliably.
- Improve online phrase and section detection without requiring a full-song
  preprocessing pass.
- Add an optional developer signal monitor that overlays timing and confidence
  during replay, never during normal playback.
- Calibrate end-to-end presentation delay per output with a repeatable tool.

### Exit criteria

- The beat clock remains stable through intros, breakdowns, and tempo
  ambiguity.
- Section events occur on useful musical boundaries across the replay set.
- Existing scenes improve or remain unchanged under deterministic A/B review.
- No visual backend reads analyzer internals outside `MusicFrame`.

## Phase 2: v0.5 visual platform

Make the renderer easy to extend without turning it into a collection of
special cases.

### Work

- Split the current application loop into audio, track state, display session,
  cover presentation, director, and render modules.
- Replace hardcoded scene switches with a versioned scene registry containing
  identity, materials, musical roles, selection traits, transition anchors,
  and performance limits.
- Give each scene persistent CPU-side state where its composition needs memory
  beyond the shared feedback buffers.
- Add shader hot reload and deterministic replay controls for development.
- Build shared primitives for flow fields, particles, curves, surfaces,
  refraction, signed-distance geometry, and feedback transport.
- Formalize a three-role palette system for background, body, and accent while
  keeping scene-specific color behavior.
- Define transition inputs that allow scenes to expose a focal point, axis,
  depth field, or motion vector to the incoming scene.
- Add GPU timing per render pass and automatic quality scaling that preserves
  the composition.

### Exit criteria

- The current ten scenes render identically within the golden-image tolerance.
- A new scene can be added through the registry without editing launch,
  director, input, or multi-monitor code.
- Transitions can preserve a declared landmark or motion direction.
- Performance diagnostics identify the cost of each active pass.

## Phase 3: v0.6 visual expansion

Prototype broadly, then ship only the strongest work. The target is 18 to 20
excellent native scenes, not the largest possible count.

### Candidate families

1. **Ink Current:** fluid calligraphy with bass-driven flow, kick vortices,
   snare cuts, and hat droplets.
2. **Glass Choir:** refractive vertical forms whose harmonic content changes
   shape while percussion creates controlled fractures.
3. **Shadow Architecture:** deep, sparse structures lit by onsets, with bars
   changing perspective and sections rebuilding the space.
4. **Particle Weave:** independent particle threads for rhythmic roles that
   braid together across a phrase.
5. **Kinetic Relief:** a moving topographic surface where groove controls
   traversal and arrangement changes reshape the terrain.
6. **Cellular Bloom:** organic growth and division tied to sustained energy,
   with transients changing growth direction rather than adding flashes.
7. **Glyph Weather:** a native glyph field whose density, flow, and grouping
   respond to musical structure. It must work as a full composition, not an
   overlay.
8. **Negative Space:** a restrained scene where the music carves darkness out
   of a luminous field. This provides contrast with dense feedback scenes.
9. **Vector Storm:** sharp directional forms that make syncopation and stereo
   movement visible.
10. **Chroma Tides:** broad layered color fields driven by tonal movement and
    harmonic change, with percussion affecting boundaries rather than the whole
    frame.

Build at least two rough candidates in each broad visual grammar. Promote at
most one when the alternatives are too similar. A candidate is cut if it does
not look strong in a still, does not move distinctly, or duplicates an existing
scene.

### Exit criteria

- The rotation contains at least two strong sparse scenes, two deep scenes, two
  surface or fluid scenes, two particle or line scenes, and two high-energy
  scenes.
- Every addition passes the full quality gate and has a clear reason to exist.
- A blind contact sheet and motion review can distinguish every shipped scene.
- The full rotation stays within the frame-time budget.

## Phase 4: v0.7 director and transitions

Turn a sequence of scenes into a coherent performance of the whole song.

### Work

- Score candidates by instrumentation, energy direction, rhythmic density,
  stereo width, tonal motion, current palette, visual density, and recent use.
- Plan contrast across multiple scene choices instead of selecting only the
  next scene.
- Recall a scene family when a chorus or motif returns, while allowing its
  details and intensity to develop.
- Distinguish intros, verses, choruses, bridges, breakdowns, peaks, and outros
  when confidence is sufficient.
- Build several transition grammars: flow carryover, focal morph, depth travel,
  controlled fracture, and negative-space reveal.
- Select transitions from scene compatibility and current musical state.
- Make `N` request the next scene while automatic direction continues. Use a
  separate explicit control if a manual lock is ever added.
- Show a short, optional state label after manual input so auto, locked, scene,
  ASCII, and sync state are never ambiguous.

### Exit criteria

- High-confidence changes land on detected musical boundaries.
- No automatic sequence repeats a recent visual shape or energy character.
- Repeated song sections produce recognizable visual recurrence.
- Every scene has at least two strong incoming and outgoing transition paths.
- Manual input never leaves the user in an accidental permanent mode.

## Phase 5: v0.8 control and accessibility

Give users meaningful control without making configuration necessary.

### Work

- Persist favorites, hidden scenes, ASCII state, intensity, brightness, motion
  level, audio delay, display selection, and director profile.
- Add a small set of director profiles such as balanced, kinetic, restrained,
  and high contrast. Profiles change selection and intensity, not core timing.
- Add a first-run control reference and an optional minimal status overlay.
- Add reduced-motion and photosensitivity-conscious modes with explicit flash,
  luminance-change, and camera-motion limits.
- Add color-vision-safe palette constraints and high-contrast materials.
- Work with deaf and hard-of-hearing testers before making accessibility claims.
  Test whether rhythm roles and arrangement changes are understandable, then
  revise the mappings from their feedback.
- Version the preferences file and test migration between releases.

### Exit criteria

- Default behavior remains strong with no setup.
- Every preference has a clear visual effect and a safe default.
- Reduced-motion and high-contrast modes pass their measured limits.
- Accessibility wording reflects completed testing rather than assumptions.

## Phase 6: v0.9 authoring and curation

Allow more people to create scenes while keeping the official rotation strict.

### Work

- Define a versioned scene-pack format with shader files, metadata, declared
  musical roles, transition anchors, performance limits, and attribution.
- Build `omadrop pack validate` for compilation, missing metadata, unsafe
  resource use, response thresholds, and frame-time limits.
- Add an authoring session with shader reload, deterministic audio replay,
  signal inspection, and side-by-side continuous and ASCII output.
- Separate installed community packs from the curated official rotation.
- Require manual review before a community scene can be featured.
- Keep projectM as a compatibility and historical preset path, not the design
  constraint for native scenes.

### Exit criteria

- A third party can create and test a scene without modifying Omadrop source.
- Broken or incompatible packs fail with specific errors.
- Community content cannot silently enter the official automatic rotation.
- The scene API can evolve without breaking older validated packs.

## Phase 7: v1.0 release quality

### Work

- Complete long-duration stability, GPU compatibility, display hotplug, output
  switching, suspend and resume, and memory-growth testing.
- Ship reliable packages and updates for Omarchy first, followed by broader
  Wayland packaging only when behavior matches the Omarchy build.
- Add crash diagnostics that contain no captured audio or private track data.
- Finish the website gallery, scene documentation, keyboard reference, and
  troubleshooting path.
- Record a rights-cleared launch film that demonstrates quiet, rhythmic, dense,
  transitional, and peak passages.
- Review every scene again and remove anything below the final quality floor.

### Exit criteria

- At least 18 visually distinct native scenes pass every gate.
- The director produces coherent full-song performances across the replay set.
- Installation, launch, playback, display behavior, and shutdown have no known
  critical defects.
- Omadrop can run for hours without frame degradation, memory growth, or sync
  drift.

## Immediate work order

1. Create the ten-scene quality scorecard and replay matrix.
2. Quantify and improve Spectral Ribbons and Constellation Field first.
3. Add automated contaminated-audio detection to the demo acceptance script.
4. Split the 2,000-line live application into stable modules before adding
   renderer complexity.
5. Add rhythmic density, tonal motion, and harmonic-change signals to
   `MusicFrame` with deterministic tests.
6. Prototype Ink Current, Glass Choir, and Negative Space as the first three
   deliberately different scene families.
7. Promote only the candidates that pass still-frame, motion, music-response,
   ASCII, transition, and performance review.

This order improves the existing product before increasing its surface area.
Each release should be obviously better in use, not only larger in a feature
list.
