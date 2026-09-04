#version 330 core

in vec2 uv;
out vec4 color;

#include "scene-uniforms.glsl"

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 p = (uv - 0.5) * aspect;
    float radius = max(0.002, length(p));
    float angle = atan(p.y, p.x);

    float rotation = 0.00020 + 0.00055 * drive + 0.024 * snare;
    vec2 previousP = rotate2d(rotation * smoothstep(0.08, 0.72, radius)) * p;
    previousP *= 1.0 - 0.021 * beatPulse - 0.016 * onsetPulse - 0.013 * kick
                         + 0.0012 * beatAnticipation;
    previousP += normalize(p) * sin(angle * 24.0 - flowTime * 3.2) * 0.0016 * hat;
    vec2 previousUv = previousP / aspect + 0.5;
    float edge = smoothstep(0.0, 0.07, uv.x) * smoothstep(0.0, 0.07, uv.y)
               * smoothstep(0.0, 0.07, 1.0 - uv.x)
               * smoothstep(0.0, 0.07, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.875, 0.945, harmonic)
                  * (1.0 - 0.050 * beatPulse - 0.070 * onsetPulse - 0.050 * kick
                         - 0.060 * snare - 0.070 * hat) * edge;

    float threads = 0.0;
    float crossings = 0.0;
    for (int index = 0; index < 6; ++index) {
        float fi = float(index);
        float tilt = fi * tau / 6.0 + phrasePhase * 0.35 + snare * 0.46;
        vec2 q = rotate2d(tilt) * p;
        q.y /= 0.34 + 0.08 * sin(fi * 2.2 + flowTime * 0.13);
        q.x /= 0.52 + 0.09 * development + 0.065 * beatPulse
              + 0.065 * onsetPulse + 0.095 * kick;
        float ellipse = abs(length(q) - 1.0);
        float strand = line(ellipse, 0.012 + 0.006 * bandLevel[index]);
        threads = max(threads, strand);
        crossings += strand * line(
            sin(angle * 12.0 + fi + flowTime * 0.18), 0.08);
    }
    float aperture = line(radius - (0.10 + 0.055 * kick), 0.009);
    float shuttlePhase = fract(angle / tau + flowTime * (0.42 + 0.3 * drive)
                             + beatPhase);
    float shuttles = line(shuttlePhase - 0.5, 0.035) * threads * hat;
    float kickKnot = line(radius - (0.14 + 0.11 * kick),
                          0.011 + 0.008 * kick) * kick;
    float snareSpokes = line(
        sin(angle * 10.0 + phrasePhase * tau + snare * 2.0), 0.028)
        * smoothstep(0.12, 0.30, radius)
        * smoothstep(0.76, 0.52, radius) * snare;
    float hatGlints = line(sin(angle * 44.0 - beatPhase * tau), 0.025)
        * line(radius - 0.36, 0.12) * hat;
    float onsetRing = line(radius - (0.18 + 0.16 * onsetPulse),
                           0.010 + 0.008 * onsetPulse) * onsetPulse;
    float downbeatOrbit = line(radius - mix(0.12, 0.64, beatPhase), 0.009)
                        * downbeat * clockConfidence;
    float beatOrbit = line(radius - mix(0.12, 0.58,
                       1.0 - clamp(beatPulse, 0.0, 1.0)), 0.011)
                    * beatPulse * clockConfidence;
    float sectionKnot = line(abs(p.x * p.y) - 0.12 * section, 0.012) * section;
    float medium = line(sin(radius * 12.0 + angle * 3.0 + flowTime * 0.3), 0.22)
                 * harmonic * smoothstep(0.72, 0.16, radius) * 0.18;

    vec3 primary = palettePrimary(1.86);
    vec3 secondary = paletteSecondary(1.86);
    vec3 accent = paletteAccent(1.86);
    vec3 injection = mix(primary, secondary,
                         0.5 + 0.5 * sin(angle * 3.0))
                         * threads * (0.12 + 0.07 * energySlow)
                   + secondary * crossings * 0.10
                   + accent * (aperture + shuttles + beatOrbit
                               + downbeatOrbit + sectionKnot) * 0.22
                   + mix(accent, vec3(1.0), 0.32) * kickKnot * 0.34
                   + mix(secondary, vec3(1.0), 0.40) * snareSpokes * 0.27
                   + mix(primary, vec3(1.0), 0.52) * hatGlints * 0.34
                   + mix(accent, vec3(1.0), 0.46) * onsetRing * 0.36
                   + primary * medium * 0.09;
    injection *= 1.0 - 0.54 * release;
    float darkCore = smoothstep(0.045, 0.105, radius);
    vec3 result = (feedback + injection) * darkCore
                * (1.0 + 0.15 * beatPulse + 0.12 * onsetPulse + 0.07 * kick
                       + 0.06 * snare + 0.05 * hat);
    result = max(result - vec3(0.0043), vec3(0.0));
    color = vec4(result, 1.0);
}
