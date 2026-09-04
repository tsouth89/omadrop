#version 330 core

in vec2 uv;
out vec4 color;

#include "scene-uniforms.glsl"

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 p = (uv - 0.5) * aspect;

    vec2 previousP = p;
    previousP.x += 0.00012 + 0.00035 * energySlow;
    previousP.y *= 1.0 - 0.020 * beatPulse - 0.012 * onsetPulse - 0.009 * kick
                         + 0.0012 * beatAnticipation;
    previousP.x -= snare * 0.011 * sign(previousP.y);
    previousP.y += hat * 0.0014 * sin(previousP.x * 52.0);
    vec2 previousUv = previousP / aspect + 0.5;
    float edge = smoothstep(0.0, 0.07, uv.x) * smoothstep(0.0, 0.07, uv.y)
               * smoothstep(0.0, 0.07, 1.0 - uv.x)
               * smoothstep(0.0, 0.07, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.865, 0.935, harmonic)
                  * (1.0 - 0.050 * beatPulse - 0.060 * onsetPulse
                         - 0.075 * max(kick, max(snare, hat))) * edge;

    float lowRibbon = 0.0;
    float midRibbon = 0.0;
    float highRibbon = 0.0;
    float intersections = 0.0;
    for (int index = 0; index < 8; ++index) {
        float fi = float(index);
        float y = (fi - 3.5) * 0.105;
        float band = spectrumLevel[index * 4 + 1];
        float frequency = 2.2 + fi * 0.72;
        float amplitude = 0.016 + 0.023 * band + 0.008 * development
                        + (beatPulse + 0.72 * onsetPulse)
                          * (0.030 + 0.0035 * fi);
        float roleWidth = 0.0;
        if (index < 3) {
            // Low ribbons open vertically and deepen their large curve.
            amplitude += kick * (0.070 + 0.008 * fi);
            y += sign(y) * kick * 0.040;
            roleWidth = 0.0045 * kick;
        } else if (index < 6) {
            // Snares make the middle voices fold through one another.
            amplitude += snare * 0.058;
            y += snare * 0.032 * sin(fi * 2.3 + barPhase * tau);
            roleWidth = 0.0040 * snare;
        } else {
            // Hats reveal short, high-frequency ripples on the upper voices.
            amplitude += hat * 0.034;
            roleWidth = 0.0032 * hat;
        }
        float curve = y + amplitude * sin(p.x * frequency * tau
                    - flowTime * (0.14 + 0.040 * fi) + phrasePhase * tau);
        if (index < 3) {
            curve += kick * 0.030
                   * sin(p.x * (frequency + 1.4) * tau + fi * 0.7);
        } else if (index < 6) {
            curve += snare * 0.052
                   * sin(p.x * frequency * 1.75 * tau + fi * 0.9);
        } else {
            curve += hat * 0.021
                   * sin(p.x * frequency * 3.2 * tau - flowTime * 3.0 + fi);
        }
        float ribbon = line(p.y - curve, 0.006 + 0.004 * band + roleWidth);
        if (index < 3) lowRibbon = max(lowRibbon, ribbon);
        else if (index < 6) midRibbon = max(midRibbon, ribbon);
        else highRibbon = max(highRibbon, ribbon);
        intersections += ribbon * line(sin(p.x * 32.0 + fi), 0.06) * hat;
    }
    float playhead = line(p.x - mix(-0.72, 0.72, beatPhase), 0.008)
                   * clockConfidence * (0.16 + 0.84 * downbeat);
    float sectionBand = line(abs(p.y) - mix(0.06, 0.46, section), 0.012) * section;
    float harmonicField = line(sin(p.x * 7.0 + p.y * 13.0 - flowTime * 0.18), 0.24)
                        * harmonic * 0.09;

    vec3 primary = palettePrimary(3.76);
    vec3 secondary = paletteSecondary(3.76);
    vec3 accent = paletteAccent(3.76);
    vec3 injection = primary * lowRibbon
                     * (0.12 + 0.07 * bandLevel[0]
                        + 0.11 * beatPulse + 0.20 * kick)
                   + secondary * midRibbon
                     * (0.12 + 0.07 * bandLevel[3]
                        + 0.11 * beatPulse + 0.19 * snare)
                   + accent * highRibbon
                     * (0.13 + 0.08 * bandLevel[5]
                        + 0.11 * beatPulse + 0.22 * hat)
                   + accent * (intersections + playhead + sectionBand) * 0.20
                   + mix(primary, secondary, 0.5) * harmonicField * 0.07;
    injection *= 1.0 - 0.57 * release;
    vec3 result = (feedback + injection)
                * (1.0 + 0.12 * beatPulse + 0.10 * onsetPulse);
    result = max(result - vec3(0.0045), vec3(0.0));
    color = vec4(result, 1.0);
}
