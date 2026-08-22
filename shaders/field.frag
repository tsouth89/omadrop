#version 440
layout(location = 0) in vec2 vTex;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 grid;      // cols, rows, atlasCols, atlasRows
    vec4 audio;     // bass, mid, treble, energy
    vec4 anim;      // time, onset, dissolve, artFade
    vec4 view;      // aspect, zoom, gain, edgeAmount
    vec4 tint;      // rgb tint for desaturated art, a = amount
    vec4 fx;        // vignette strength, reserved, reserved, reserved
    vec4 scene;     // sceneA index, sceneB index, blend, spare
    vec4 pal0;      // dominant colours of the current cover
    vec4 pal1;
    vec4 pal2;
    vec4 pal3;
    vec4 sp0;       // 32-band spectrum, 4 bands per vec4
    vec4 sp1;
    vec4 sp2;
    vec4 sp3;
    vec4 sp4;
    vec4 sp5;
    vec4 sp6;
    vec4 sp7;
    vec4 music;     // beatPhase, beatImpact, beatSwell, beatConf
    vec4 music2;    // percussive, harmonic, barPhase, bpm/200
};
layout(binding = 1) uniform sampler2D artTex;
layout(binding = 2) uniform sampler2D artTexB;
layout(binding = 3) uniform sampler2D prevTex;   // previous accumulator

// Wall time (anim.x) drifts at a constant rate no matter what is playing.
// mtime() advances with energy instead, so motion tracks the track. pulse()
// is the rare bass beat, distinct from the ~4/sec onset.
// Declared here because toArt() uses them, and GLSL needs them first.
float mtime() { return fx.z; }
float pulse() { return fx.w; }

// Beat strength. When the tracker is locked this is phase-derived, so it can
// never fire late -- it is a function of *where we are in the beat*, not a
// detection of one that already happened. Falls back to the flux pulse when
// confidence is low (non-rhythmic material, or before the tracker locks).
float beat() {
    return clamp(mix(pulse(), music.y, music.w) + anim.y * 0.07, 0.0, 1.4);
}

// Anticipation: rises through the back half of the beat, peaking just before
// the next one lands. This is the whole point of tracking tempo.
float swell() { return music.z; }

// Percussive and harmonic streams, so drums and sustained tones drive
// different things instead of competing for the same channel.
float perc() { return music2.x; }
float harm() { return music2.y; }
float slowEnergy() { return music2.w; }

// Long-form motion. This is the animation; the beat is only an accent on top
// of it. Without a continuously evolving base every parameter just oscillates
// at beat rate and the whole thing reads as jittering back and forth rather
// than moving. Periods here are 25-140s and mutually irrational, so the field
// never visibly repeats.
// Slow oscillator, 0..1, for morphing scene shape over tens of seconds.
float lfo(float period, float offset) {
    return 0.5 + 0.5 * sin(mtime() * 6.2831853 / period + offset);
}

vec2 flowWarp(vec2 p) {
    float mt = mtime();
    float rot = 0.22 * sin(mt * 0.047) + 0.11 * sin(mt * 0.131) + 0.06 * sin(mt * 0.019);
    float sc  = 1.0 + 0.13 * sin(mt * 0.037) + 0.07 * sin(mt * 0.091);
    float cs = cos(rot), sn = sin(rot);
    p = vec2(p.x * cs - p.y * sn, p.x * sn + p.y * cs) * sc;
    p += vec2(0.055 * sin(mt * 0.029), 0.045 * cos(mt * 0.041));
    return p;
}

// Differential beat warp. Phase-shifting a pattern barely moves pixels --
// measured at 0.05-0.12x of lit content -- because alternating shifts cancel.
// Geometry does move: concentric zones scale in opposite directions and
// counter-rotate, so a hit visibly tears the field apart and lets it settle.
// Every scene runs this, seeded differently so they do not all warp alike.
vec2 beatWarp(vec2 p, float seed) {
    // Wind up against the coming beat, then release on it. The wind-up is
    // what separates a gesture that looks intended from one that looks like a
    // flinch after the fact.
    float b = beat() - swell() * 0.55;
    if (abs(b) < 0.002) return p;
    float r = length(p);
    // Zones must alternate SMOOTHLY. A parity step (dirOf) makes adjacent
    // zones scale by +-48% with a hard jump between them, and those
    // discontinuities show up as fixed circles burned into the middle of
    // every scene. A sine alternates direction with no seam.
    float zone = sin(r * 7.5 + seed);
    float sc  = 1.0 + zone * b * 0.17;
    float rot = zone * b * 0.26;
    float cs = cos(rot), sn = sin(rot);
    return vec2(p.x * cs - p.y * sn, p.x * sn + p.y * cs) * sc;
}

// Spectrum lookup. GLSL ES will not index uniforms dynamically, hence the
// chain. x is 0 (low) to 1 (high).
float bandAt(int i) {
    vec4 v = i < 16 ? (i < 8  ? (i < 4  ? sp0 : sp1)
                              : (i < 12 ? sp2 : sp3))
                    : (i < 24 ? (i < 20 ? sp4 : sp5)
                              : (i < 28 ? sp6 : sp7));
    int c = i - (i / 4) * 4;
    return c == 0 ? v.x : c == 1 ? v.y : c == 2 ? v.z : v.w;
}

float spectrum(float x) {
    float f = clamp(x, 0.0, 0.9999) * 31.0;
    int i = int(f);
    return mix(bandAt(i), bandAt(i + 1 > 31 ? 31 : i + 1), fract(f));
}

float dotValue(int dx, int dy) {
    if (dx == 0) return dy == 3 ? 64.0 : float(1 << dy);
    return dy == 3 ? 128.0 : float(8 << dy);
}

float hash(vec2 p) {
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

// Interleaved-gradient noise. A Bayer matrix leaves a visible crosshatch at
// this dot pitch; this dithers just as evenly without the grid artefact.
float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

vec3 sampleArt(vec2 uv) {
    return mix(texture(artTex, uv).rgb, texture(artTexB, uv).rgb, anim.w);
}

// A 1:1 cover on a 16:9 screen cannot both fill the frame and stay whole.
// view.y = 1/aspect shows all of it; the sides are then filled by an ambient
// mirrored spill (below) rather than black bars.
vec2 toArt(vec2 uv) {
    vec2 c = (uv - 0.5) * vec2(view.x, 1.0);
    float t = anim.x;
    // Slow drift keeps the held frame alive rather than static.
    c *= 1.0 - 0.015 * sin(t * 0.11);
    c.y += 0.008 * sin(t * 0.07);
    // Bass breathes the frame and a kick punches in. This has to live outside
    // the dissolve branch or the whole hold phase sits motionless.
    c *= 1.0 - audio.x * 0.055 - pulse() * 0.045;  // slow bass breath, punch only on a real beat
    return 0.5 + c / (view.x * view.y);
}

vec4 coverField(vec2 uv) {
    float t = anim.x;
    float dis = anim.z;

    vec2 base = toArt(uv);
    {
        vec2 d = base - 0.5;
        float rot = anim.y * 0.045 + audio.x * 0.012;
        float cs = cos(rot), sn = sin(rot);
        base = 0.5 + vec2(d.x * cs - d.y * sn, d.x * sn + d.y * cs);
    }

    // Scale-separated displacement. Bass reads clearly because it moves
    // geometry; mids and highs previously only nudged brightness and the
    // dither threshold, which is far less salient than motion. Each band now
    // displaces at its own spatial scale and rate, so they can be told apart:
    // long slow swells, medium waves, fine fast chatter.
    {
        vec2 d = base - 0.5;
        float rr = length(d) + 1e-5;
        // Ripple amplitude follows the spectrum by radius, so the artwork
        // itself carries the spectrum rather than pumping uniformly.
        float bandR = spectrum(clamp(rr * 1.7, 0.0, 1.0));
        float amp = (audio.x * 0.040 + bandR * 0.030);
        base += (d / rr) * amp * sin(rr * 26.0 - t * 4.0);
    }

    // Mid: medium wavelength, lateral.
    base.x += audio.y * 0.062 * sin(base.y * 17.0 - t * 2.6);
    base.y += audio.y * 0.032 * sin(base.x * 13.0 + t * 2.1);

    // Treble: short wavelength, fast. Small amplitude, but motion at this
    // scale reads as shimmer and is visible where a brightness nudge is not.
    base += vec2(sin(base.y * 96.0 + t * 15.0),
                 cos(base.x * 84.0 - t * 12.0)) * audio.z * 0.0052;

    // Devolve: swirl and scatter grow as `dissolve` rises, carrying the cover
    // apart into the abstract field.
    if (dis > 0.001) {
        vec2 c = base - 0.5;
        float r = length(c);
        float a = atan(c.y, c.x);
        a += dis * (1.1 * sin(t * 0.25) + r * 3.4) + audio.z * 0.18 * sin(r * 16.0 - t * 2.0);
        r += audio.x * 0.05 * sin(r * 11.0 - t * 1.6) - dis * 0.06 * sin(t * 0.6);
        base = 0.5 + vec2(cos(a), sin(a)) * r;
        base += (vec2(hash(floor(uv * 500.0)), hash(floor(uv * 500.0) + 7.3)) - 0.5)
                * dis * dis * 0.5 * (0.35 + audio.w);
    }

    // Outside the cover, mirror the artwork outward at low intensity. Reads
    // as light spilling off the record instead of a hard rectangle on black.
    float inside = step(0.0, base.x) * step(base.x, 1.0)
                 * step(0.0, base.y) * step(base.y, 1.0);
    vec2 mirrored = abs(fract(base * 0.5) * 2.0 - 1.0);
    vec2 sampleUV = mix(mirrored, base, inside);

    vec3 col = sampleArt(sampleUV);
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    // Spill is dimmer and falls away from the cover edge.
    float spill = 1.0 - smoothstep(0.0, 0.85, max(abs(base.x - 0.5), abs(base.y - 0.5)) - 0.5);
    l *= mix(0.30 * spill, 1.0, inside);

    // Structure. A pure luminance halftone reads as a fax; adding the local
    // gradient puts contours back so it reads as drawn.
    if (view.w > 0.001) {
        vec2 e = vec2(1.6 / grid.x, 1.6 / grid.y);
        float lx = dot(sampleArt(clamp(base + vec2(e.x, 0.0), 0.0, 1.0)), vec3(0.299, 0.587, 0.114))
                 - dot(sampleArt(clamp(base - vec2(e.x, 0.0), 0.0, 1.0)), vec3(0.299, 0.587, 0.114));
        float ly = dot(sampleArt(clamp(base + vec2(0.0, e.y), 0.0, 1.0)), vec3(0.299, 0.587, 0.114))
                 - dot(sampleArt(clamp(base - vec2(0.0, e.y), 0.0, 1.0)), vec3(0.299, 0.587, 0.114));
        float edge = sqrt(lx * lx + ly * ly);
        l = clamp(l + edge * view.w * 2.6, 0.0, 1.4);
        col = mix(col, col + vec3(edge * view.w * 1.4), 0.6);
    }

    // An S-curve before dithering is what creates negative space. Without it
    // dark regions still light one or two dots per cell and the whole frame
    // reads as a uniform grey haze instead of art.
    l = smoothstep(0.13, 0.80, clamp(l, 0.0, 1.0)) * view.z;

    float sat = max(max(col.r, col.g), col.b) - min(min(col.r, col.g), col.b);
    col = mix(col, tint.rgb * l * 1.25, tint.a * (1.0 - smoothstep(0.0, 0.22, sat)));

    return vec4(col, l);
}


// ---------------------------------------------------------------- scenes
//
// Abstract fields the cover devolves into. Each returns rgb plus a 0..1 level
// that the braille dither turns into dots. Colour is sampled from the album
// through a slow swirl, so the journey stays visually tied to the record it
// started from rather than becoming generic.

vec3 palAt(int i) {
    if (i == 0) return pal0.rgb;
    if (i == 1) return pal1.rgb;
    if (i == 2) return pal2.rgb;
    return pal3.rgb;
}

// Scene colour is the album's *palette*, not its picture. Sampling the cover
// directly drags its imagery into the middle of every scene and buries what
// the scene is drawing; averaging samples to hide that just converges to grey.
// Four dominant colours, blended by angle and radius, keep the record's colour
// with none of its structure. fx.y is 1 when a cover (and palette) is loaded.
vec3 albumColor(vec2 uv, float t) {
    vec2 p = uv - 0.5;
    float a = atan(p.y, p.x);
    float r = length(p);

    float m = fract(r * 1.15 + 0.16 * sin(a * 2.0 + r * 3.5) - t * 0.025) * 4.0;
    int i0 = int(m);
    vec3 fromPal = mix(palAt(i0), palAt(int(mod(float(i0) + 1.0, 4.0))),
                       smoothstep(0.0, 1.0, fract(m)));

    vec3 base = tint.rgb;
    vec3 alt = vec3(base.b, base.r, base.g);
    vec3 fallback = mix(base, alt, 0.5 + 0.5 * sin(a * 1.5 + r * 4.0 - t * 0.35));

    return mix(fallback, fromPal, fx.y);
}

vec2 aspectP(vec2 uv) { return (uv - 0.5) * vec2(view.x, 1.0); }




// Concentric ripples riding the low end, with a wavefront released on a kick.
vec4 sceneRings(vec2 uv) {
    float t = anim.x;
    vec2 p = beatWarp(flowWarp(aspectP(uv)), 0.0);
    p += vec2(sin(t * 0.13) * 0.06, cos(t * 0.11) * 0.05);
    float r = length(p);
    float aa = atan(p.y, p.x);
    // A slight angular warp keeps the rings from reading as a flat bullseye.
    r += 0.012 * sin(aa * 3.0 + t * 0.5);
    float mt = mtime();
    float ringDir = sin(r * 26.0);
    float kR = 22.0 + 14.0 * lfo(53.0, 0.0);
    float w = sin(r * kR - mt * 2.1 - audio.x * 7.0 + ringDir * beat() * 4.0)
            + 0.45 * sin(r * (kR * 2.05) - mt * 3.4);
    // Radius maps to frequency: each ring answers to its own band, so the
    // field shows the spectrum instead of pumping as one body.
    float bandR = spectrum(clamp(r * 1.5, 0.0, 1.0));
    float l = smoothstep(0.15, 1.05, w) * exp(-r * (0.45 + 0.6 * lfo(81.0, 1.3))) * (0.35 + bandR * 1.15);
    l *= 0.75 + audio.w * 0.45 + pulse() * 0.18;
    return vec4(albumColor(uv, t), clamp(l, 0.0, 1.0));
}

// Perspective tunnel: rings in depth, spokes in angle.
vec4 sceneTunnel(vec2 uv) {
    float t = anim.x;
    vec2 p = beatWarp(flowWarp(aspectP(uv)), 0.7);
    float r = max(length(p), 0.02);
    float a = atan(p.y, p.x);
    float depth = (0.22 + 0.22 * lfo(59.0, 0.6)) / max(r, 0.05) + mtime() * 0.62 + audio.x * 0.6;
    // Kill the centre singularity: past this radius the ring frequency
    // outruns the cell grid and aliases into noise.
    float core = smoothstep(0.045, 0.20, r);
    depth += sin(depth * 6.2831853) * beat() * 0.30;
    float rings = smoothstep(0.20, 1.0, sin(depth * 6.2831)) * core;
    float spokeN = 8.0 + 12.0 * lfo(67.0, 2.1);
    float spokes = smoothstep(0.30, 1.0, sin(a * spokeN + t * 0.35 + depth * 0.6));
    // Frequency lies along the tunnel's depth: the vanishing point is the top
    // of the spectrum and the mouth is the bottom, so energy is seen
    // travelling down the tunnel rather than around it.
    float depthBand = spectrum(clamp(1.0 - r * 1.55, 0.0, 1.0));
    // A second, weaker angular term keeps the wheel from looking uniform.
    float angBand = spectrum(fract(a / 6.2831853 + 0.5)) * 0.35;
    float l = max(rings * (0.28 + depthBand * 1.45),
                  spokes * (0.20 + (depthBand * 0.5 + angBand) * 0.9))
            * smoothstep(1.35, 0.10, r);
    // A bright vanishing point sells the depth the rings can no longer draw.
    l += (1.0 - core) * 0.85 * (0.4 + audio.x * 0.8);
    l *= 0.85 + audio.w * 0.35;
    return vec4(albumColor(uv, t), clamp(l, 0.0, 1.0));
}

// Interference field. Three sources at deliberately *detuned* frequencies --
// the small offsets beat against each other and produce slow moire bands,
// which is what gives the field large-scale structure instead of a uniform
// weave. Crests are sharpened into thin bright lines and the zero-crossings
// are drawn as fine nodal tracery.
vec4 sceneWaves(vec2 uv) {
    float t = anim.x;
    vec2 p = beatWarp(flowWarp(aspectP(uv)), 1.4);

    vec2 s1 = vec2(sin(t * 0.21) * 0.36, cos(t * 0.17) * 0.24);
    vec2 s2 = vec2(cos(t * 0.15) * -0.33, sin(t * 0.25) * 0.27);
    vec2 s3 = vec2(sin(t * 0.11) * 0.12, cos(t * 0.13) * -0.30);
    // The sources push apart on a beat rather than all brightening together.
    s1 += normalize(s1 + 1e-5) * beat() * 0.135;
    s2 -= normalize(s2 + 1e-5) * beat() * 0.135;

    float k1 = 33.0 + audio.x * 6.0;   // low end detunes one source
    float k2 = 37.5;
    float k3 = 29.0;

    float mt = mtime();
    float w = sin(length(p - s1) * k1 - mt * 1.9)
            + sin(length(p - s2) * k2 + mt * 1.5)
            + 0.85 * sin(length(p - s3) * k3 - mt * 2.4);
    // Each source is driven by a different part of the spectrum.
    w = sin(length(p - s1) * k1 - mt * 1.9) * (0.5 + spectrum(0.12) * 1.1)
      + sin(length(p - s2) * k2 + mt * 1.5) * (0.5 + spectrum(0.45) * 1.1)
      + 0.85 * sin(length(p - s3) * k3 - mt * 2.4) * (0.5 + spectrum(0.78) * 1.1);
    float wn = w / 2.85;

    float crest = smoothstep(0.30, 0.95, wn);
    float node  = smoothstep(0.055, 0.0, abs(wn)) * 0.7;
    float l = max(crest, node);

    // A ring released on every transient: as the onset envelope decays the
    // radius grows, so each hit throws a wavefront outward. Stateless -- the
    // decay itself is the clock.

    l *= 0.78 + audio.y * 0.4 + pulse() * 0.15;
    l *= smoothstep(1.35, 0.08, length(p));
    return vec4(albumColor(uv, t), clamp(l, 0.0, 1.0));
}

// Terminal rain over the album. Drops fall on the musical clock -- so they
// surge on transients and slow in quiet passages -- and each drop *develops*
// the artwork behind it: cells the rain has recently passed glow with the
// cover, fading back to a faint standing image.
vec4 sceneRain(vec2 uv) {
    float t = anim.x;
    float mt = mtime();
    float col = floor(uv.x * grid.x);
    float row = floor(uv.y * grid.y);

    float h1 = hash(vec2(col, 1.0));
    float h2 = hash(vec2(col, 7.0));
    float h3 = hash(vec2(col, 13.0));

    // Each column answers to its own band: columns fall and brighten with
    // the frequency they represent, so the curtain has internal rhythm.
    float bandC = spectrum(uv.x);
    float period = (2.2 + h1 * 5.5) / (0.55 + bandC * 1.1);
    // Half the columns surge on a beat and half hold back.
    float colDir = hash(vec2(col, 3.0)) < 0.5 ? -1.0 : 1.0;
    float phase = fract(mt / period + h2 + colDir * beat() * 0.10);
    float head = phase * 1.5 - 0.25;
    float d = head - uv.y;

    // The cover, positioned exactly as in the reveal phase.
    vec2 av = toArt(0.5 + beatWarp(flowWarp(uv - 0.5), 4.9));
    float insideArt = step(0.0, av.x) * step(av.x, 1.0)
                    * step(0.0, av.y) * step(av.y, 1.0);
    vec3 acol = sampleArt(clamp(av, 0.0, 1.0));
    float al = smoothstep(0.16, 0.86, dot(acol, vec3(0.299, 0.587, 0.114))) * insideArt;

    // Above the leading cell: only the faint standing image.
    if (d < 0.0) return vec4(acol, clamp(al * 0.15, 0.0, 1.0));

    // Behind the drop the image is briefly developed, then decays back.
    float wake = exp(-d * 2.1);
    float artLevel = al * (0.15 + wake * 0.80);

    float trail = 0.10 + h3 * 0.34;
    float body = exp(-d / trail);
    float headGlow = smoothstep(2.5 / grid.y, 0.0, d);
    float churn = hash(vec2(col * 31.0 + row, floor(mt * (7.0 + audio.z * 16.0))));

    float rainL = body * (0.62 + churn * 0.6) + headGlow * 1.15;
    rainL *= 0.55 + bandC * 1.1;

    vec3 c = mix(acol, albumColor(uv, t), 0.3);
    c = mix(c, vec3(1.0), headGlow * 0.8);
    return vec4(c, clamp(max(rainL, artLevel), 0.0, 1.0));
}


// Logarithmic spiral arms -- rotational, where rings are concentric.
// A beat kicks the winding, so the arms visibly torque on the hit.
vec4 sceneSpiral(vec2 uv) {
    float mt = mtime();
    vec2 p = beatWarp(flowWarp(aspectP(uv)), 2.1);
    float r = max(length(p), 0.02);
    float a = atan(p.y, p.x);

    float wind = 3.5 + 6.0 * lfo(47.0, 2.7) + beat() * 5.5 + audio.x * 1.4;
    float armN = 3.0 + 4.0 * lfo(71.0, 0.9);
    float armDir = sin(a * armN);
    float w = sin(a * armN + log(r) * wind - mt * 1.5 + armDir * beat() * 4.2)
            + 0.45 * sin(a * (armN * 1.8) - log(r) * 3.0 + mt * 0.9);

    float band = spectrum(clamp(r * 1.5, 0.0, 1.0));
    float l = smoothstep(0.10, 1.05, w) * (0.42 + band * 1.5);
    l *= smoothstep(1.30, 0.04, r);
    l *= 0.85 + audio.w * 0.6 + anim.y * 0.30;
    return vec4(albumColor(uv, anim.x), clamp(l, 0.0, 1.0));
}

// Rectilinear lattice -- the only scene with no radial symmetry, which is what
// makes it read as a change of subject rather than a variation. The cell size
// snaps on a beat, which is the most visible gesture a grid has.
vec4 sceneLattice(vec2 uv) {
    float mt = mtime();
    vec2 p = beatWarp(flowWarp(aspectP(uv)), 2.8);
    float bx = spectrum(clamp(abs(p.x) * 1.5, 0.0, 1.0));
    float by = spectrum(clamp(abs(p.y) * 2.1, 0.0, 1.0));

    vec2 q = p * (1.0 - pulse() * 0.14);
    q.x += 0.070 * by * sin(p.y * 9.0 + mt * 1.1);
    q.y += 0.070 * bx * sin(p.x * 7.0 - mt * 0.9);
    // Bands of rows and columns slide opposite ways on a beat.
    // sin(q * 18.0): half a cycle is pi/18 = 0.1745.
    q.x += beat() * 0.1745 * sin(p.y * 15.0);
    q.y += beat() * 0.1745 * sin(p.x * 12.0);

    // Wider bands than a hairline, or the grid is invisible at this dot pitch.
    float gx = smoothstep(0.62, 1.0, abs(sin(q.x * (11.0 + 12.0 * lfo(63.0, 0.4) + beat() * 9.0) + mt * 0.3)));
    float gy = smoothstep(0.62, 1.0, abs(sin(q.y * (11.0 + 12.0 * lfo(63.0, 0.4) - 0.0 + beat() * 9.0) - mt * 0.25)));
    float l = max(gx * (0.45 + bx * 1.5), gy * (0.45 + by * 1.5));
    // Intersections glow, giving the lattice nodes.
    l += gx * gy * (0.5 + anim.y * 0.8);
    l *= smoothstep(1.45, 0.10, length(p)) * (0.9 + audio.w * 0.5);
    return vec4(albumColor(uv, anim.x), clamp(l, 0.0, 1.0));
}

// Topographic contours -- organic closed loops, no straight lines, no centre.
// A beat shifts the contour levels, so the whole map ripples outward at once.
vec4 sceneContour(vec2 uv) {
    float mt = mtime();
    vec2 p = beatWarp(flowWarp(aspectP(uv)), 3.5);
    vec2 pb = p * (1.0 - beat() * 0.10);
    float kC = 3.2 + 3.6 * lfo(73.0, 1.7);
    float f = sin(pb.x * kC + mt * 0.42) * cos(pb.y * (kC * 0.85) - mt * 0.31)
            + 0.62 * sin((pb.x + pb.y) * 7.5 + mt * 0.5)
            + 0.40 * sin(length(pb) * 11.0 - mt * 0.66);

    float band = spectrum(clamp(fract(f * 0.22 + 0.5), 0.0, 1.0));
    float levels = f * (1.9 + beat() * 1.4) + mt * 0.08 + sin(f * 6.0) * beat() * 0.5;      // half a cycle through fract()
    float c = abs(fract(levels) - 0.5);
    float l = smoothstep(0.20, 0.0, c) * (0.55 + band * 1.5);
    l *= smoothstep(1.5, 0.05, length(p)) * (0.9 + audio.y * 0.6 + anim.y * 0.3);
    return vec4(albumColor(uv, anim.x), clamp(l, 0.0, 1.0));
}

// Directional streams -- streaks following a slowly turning flow field. Beats
// surge the streaks along the flow rather than brightening them in place.
vec4 sceneStreams(vec2 uv) {
    float mt = mtime();
    vec2 p = beatWarp(flowWarp(aspectP(uv)), 4.2);
    float ang = sin(p.y * 2.8 + mt * 0.21) * 1.15 + cos(p.x * 2.1 - mt * 0.16) * 0.95
              + sin(p.y * 9.0) * beat() * 0.45;
    float across = p.x * cos(ang) + p.y * sin(ang);
    float along  = -p.x * sin(ang) + p.y * cos(ang);

    float band = spectrum(clamp(fract(across * 1.4 + 0.5), 0.0, 1.0));
    float surge = mt * 2.6 + sin(across * 22.0) * beat() * 3.14159265;  // half a cycle
    float streak = smoothstep(0.45, 1.0, abs(sin(across * (13.0 + 14.0 * lfo(57.0, 2.3) + beat() * 10.0) - surge)));
    streak *= 0.6 + 0.4 * sin(along * 6.0 + mt * 1.4);
    float l = streak * (0.5 + band * 1.5);
    l *= smoothstep(1.45, 0.06, length(p)) * (0.85 + audio.z * 0.7 + anim.y * 0.3);
    return vec4(albumColor(uv, anim.x), clamp(l, 0.0, 1.0));
}

vec4 sceneField(vec2 uv, float idx) {
    int i = int(idx + 0.5);
    if (i == 0) return sceneRings(uv);
    if (i == 1) return sceneTunnel(uv);
    if (i == 2) return sceneWaves(uv);
    if (i == 3) return sceneRain(uv);
    if (i == 4) return sceneSpiral(uv);
    if (i == 5) return sceneLattice(uv);
    if (i == 6) return sceneContour(uv);
    return sceneStreams(uv);
}

vec4 field(vec2 uv) {
    vec4 c = coverField(uv);
    float dis = anim.z;
    if (dis > 0.002) {
        vec4 s = mix(sceneField(uv, scene.x), sceneField(uv, scene.y), scene.z);
        c = mix(c, s, dis);
    }
    if (fx.x > 0.001) {
        vec2 q = (uv - 0.5) * vec2(1.06, 1.42);
        c.a *= 1.0 - fx.x * smoothstep(0.42, 0.78, length(q));
    }
    return c;
}

// Field pass. Renders the scene into an accumulator that samples ITSELF
// through a slow warp each frame and decays. This is MilkDrop's core trick:
// nothing here draws a trail explicitly -- trails, tunnels, smoke and drifting
// structure all fall out of feeding the previous frame back through a warp.
// Without it every frame is a pure function of (time, audio), so nothing
// persists and there is nothing for the eye to follow.
//
// Output is premultiplied (rgb = colour * level, a = level) so it survives
// compositing into the accumulator texture unchanged.
void main() {
    vec4 f = field(vTex);

    // Feedback warp: a slow zoom/rotate/drift, nudged by the music. Keep it
    // small -- a few percent per frame compounds into strong motion because
    // it is applied again to its own output every frame.
    float mt = mtime();
    float zoom = 0.978 - 0.012 * slowEnergy() - 0.008 * beat();
    float rot  = 0.0045 * sin(mt * 0.053) + 0.0022 * sin(mt * 0.017)
               + 0.0030 * beat() * sin(mt * 0.11);
    vec2 drift = vec2(0.0016 * sin(mt * 0.031), 0.0013 * cos(mt * 0.023));

    vec2 c = vTex - 0.5;
    float cs = cos(rot), sn = sin(rot);
    vec2 pv = 0.5 + vec2(c.x * cs - c.y * sn, c.x * sn + c.y * cs) * zoom + drift;

    vec4 prev = texture(prevTex, clamp(pv, 0.001, 0.999));

    // Decay sets trail length. Higher = longer memory, but too high and the
    // frame saturates into mush.
    // Feedback ramps in with the dissolve. The cover reveal should be a
    // clean, crisp image; trails belong to the abstract phase it devolves
    // into. This also stops the accumulator smearing the artwork.
    float decay = (0.80 + 0.055 * slowEnergy()) * smoothstep(0.0, 0.30, anim.z);
    prev *= decay;
    // Edges must not feed back or the border smears inward forever.
    float edge = smoothstep(0.0, 0.06, min(min(pv.x, 1.0 - pv.x), min(pv.y, 1.0 - pv.y)));
    prev *= edge;

    vec4 add = vec4(f.rgb * f.a, f.a) * 0.45;
    fragColor = clamp(prev + add, 0.0, 1.0);
}
