#version 330 core

in vec2 uv;
out vec4 color;

#include "scene-uniforms.glsl"

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 p = (uv - 0.5) * aspect;
    float radius = max(0.002, length(p));
    float angle = atan(p.y, p.x);

    float twist = 0.00020 + 0.017 * snare * smoothstep(0.10, 0.66, radius);
    vec2 previousP = rotate2d(twist) * p;
    previousP *= 1.0 - 0.024 * beatPulse - 0.014 * onsetPulse - 0.016 * kick
                         + 0.0012 * beatAnticipation;
    previousP += normalize(p) * sin(angle * 30.0 - flowTime * 3.5) * 0.0016 * hat;
    vec2 previousUv = previousP / aspect + 0.5;
    float edge = smoothstep(0.0, 0.07, uv.x) * smoothstep(0.0, 0.07, uv.y)
               * smoothstep(0.0, 0.07, 1.0 - uv.x)
               * smoothstep(0.0, 0.07, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.88, 0.952, harmonic)
                  * (1.0 - 0.055 * beatPulse - 0.060 * onsetPulse - 0.080 * kick
                         - 0.085 * snare - 0.090 * hat) * edge;

    float petals = 7.0 + floor(development * 5.0);
    float petalRadius = 0.27 + 0.12 * development + 0.090 * beatPulse
                      + 0.060 * onsetPulse
                      + 0.145 * kick
                      + 0.045 * sin(angle * petals + phrasePhase * tau);
    float petalEdge = line(radius - petalRadius, 0.010 + 0.006 * bandLevel[2]);
    float veins = line(sin(angle * petals + snare * 2.5
                          + flowTime * (0.16 + 0.26 * drive)), 0.055)
                * smoothstep(0.10, 0.25, radius)
                * smoothstep(petalRadius + 0.08, petalRadius - 0.04, radius);
    float innerPetals = line(radius - (0.13 + 0.055 * beatPulse
                         + 0.078 * kick
                         + 0.020 * sin(angle * (petals - 2.0) - flowTime * 0.2)), 0.008);
    float pollen = line(sin(angle * 24.0 + flowTime * 2.4 + beatPhase * tau), 0.055)
                 * line(radius - 0.20, 0.065) * hat;
    float snareSpokes = line(
        sin(angle * petals * 2.0 + phrasePhase * tau + snare * 1.8),
        0.030 + 0.012 * bandLevel[3])
        * smoothstep(0.10, 0.22, radius)
        * smoothstep(petalRadius + 0.10, petalRadius - 0.015, radius)
        * snare;
    float hatHalo = line(
        radius - (0.23 + 0.035 * sin(angle * 32.0 - flowTime * 2.1)),
        0.006 + 0.003 * bandLevel[5])
        * line(sin(angle * 32.0 + beatPhase * tau), 0.045)
        * hat;
    float kickShock = line(radius - (0.10 + 0.23 * (1.0 - kick)),
                           0.009 + 0.008 * kick)
                    * kick;
    float downbeatBloom = line(radius - mix(0.08, 0.62, beatPhase), 0.009)
                        * downbeat * clockConfidence;
    float beatBloom = line(radius - mix(0.10, 0.54,
                        1.0 - clamp(beatPulse, 0.0, 1.0)), 0.012)
                    * beatPulse * clockConfidence;
    float sectionPetal = line(sin(angle * (petals + 4.0)), 0.06)
                       * line(radius - 0.48 * section, 0.014) * section;
    float medium = line(sin(radius * 18.0 - flowTime * 0.34), 0.20)
                 * harmonic * smoothstep(0.64, 0.12, radius) * 0.16;

    vec3 primary = palettePrimary(4.28);
    vec3 secondary = paletteSecondary(4.28);
    vec3 accent = paletteAccent(4.28);
    vec3 injection = mix(primary, secondary,
                         0.5 + 0.5 * sin(angle * petals))
                         * petalEdge * (0.13 + 0.07 * energySlow)
                   + secondary * veins * (0.10 + 0.07 * harmonic)
                   + primary * innerPetals * 0.16
                   + accent * (pollen + beatBloom + downbeatBloom
                               + sectionPetal + kickShock) * 0.30
                   + mix(accent, vec3(1.0), 0.34) * snareSpokes * 0.34
                   + mix(primary, vec3(1.0), 0.48) * hatHalo * 0.42
                   + mix(primary, secondary, 0.5) * medium * 0.08;
    injection *= 1.0 - 0.56 * release;
    float darkCore = smoothstep(0.045, 0.11, radius);
    vec3 result = (feedback + injection) * darkCore
                * (1.0 + 0.16 * beatPulse + 0.10 * onsetPulse + 0.12 * kick
                       + 0.10 * snare + 0.08 * hat);
    result = max(result - vec3(0.0043), vec3(0.0));
    color = vec4(result, 1.0);
}
