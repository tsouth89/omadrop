#version 330 core

in vec2 uv;
out vec4 color;

#include "scene-uniforms.glsl"

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 p = (uv - 0.5) * aspect;
    p.x -= stereoWidth * 0.035 * sin(p.y * 3.0 + flowTime * 0.15);

    vec2 previousP = p;
    previousP.y += 0.00012 + 0.00025 * energySlow;
    previousP.x *= 1.0 - 0.022 * beatPulse - 0.014 * onsetPulse - 0.014 * kick
                         + 0.0012 * beatAnticipation;
    previousP.x -= snare * 0.022 * sign(previousP.x)
                 * (0.3 + abs(previousP.y));
    previousP.y += hat * 0.0016 * sin(previousP.x * 38.0);
    vec2 previousUv = previousP / aspect + 0.5;
    float edge = smoothstep(0.0, 0.07, uv.x) * smoothstep(0.0, 0.07, uv.y)
               * smoothstep(0.0, 0.07, 1.0 - uv.x)
               * smoothstep(0.0, 0.07, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.92, 0.968, harmonic)
                  * (1.0 - 0.040 * beatPulse - 0.050 * onsetPulse) * edge;

    float architecture = 0.0;
    float windows = 0.0;
    for (int index = 0; index < 5; ++index) {
        float fi = float(index);
        float depth = fi / 4.0;
        float width = mix(0.18, 0.68, depth) + 0.070 * beatPulse
                    + 0.050 * onsetPulse
                    + 0.065 * kick;
        float roof = mix(0.13, 0.48, depth) + 0.06 * development
                   + 0.035 * beatPulse;
        vec2 q = p;
        q.x -= snare * 0.070 * sin(fi * 1.8 + barPhase * tau);
        float archRadius = length(vec2(q.x / width, (q.y - roof * 0.28) / roof));
        float arch = line(archRadius - 1.0, 0.025 + 0.012 * bandLevel[index]);
        arch *= smoothstep(-0.45, -0.12, q.y);
        float pillars = line(abs(q.x) - width, 0.009 + 0.005 * bandLevel[2]);
        pillars *= smoothstep(roof * 0.25, -0.52, q.y);
        architecture = max(architecture, max(arch, pillars));
        windows += arch * line(sin(atan(q.y, q.x) * 18.0 + flowTime), 0.08) * hat;
    }
    float aisle = line(abs(p.x) - (0.05 + 0.34 * (p.y + 0.5)), 0.009)
                * smoothstep(0.52, -0.22, p.y);
    float floorBars = line(sin((0.15 / max(0.04, p.y + 0.57)
                              + flowTime * 0.25 + 1.20 * beatPulse
                              + kick) * tau), 0.10)
                    * smoothstep(-0.08, -0.48, p.y);
    float downbeatArch = line(length(p / vec2(0.38, 0.28))
                            - mix(0.35, 1.0, beatPhase), 0.014)
                       * downbeat * clockConfidence;
    float beatArch = line(length(p / vec2(0.52, 0.38))
                          - mix(0.34, 1.0,
                            1.0 - clamp(beatPulse, 0.0, 1.0)), 0.018)
                   * beatPulse * clockConfidence;
    float sectionRose = line(length(p - vec2(0.0, 0.18)) - 0.13, 0.012)
                      * section;
    float ambience = line(sin(p.y * 10.0 + p.x * 4.0 - flowTime * 0.18), 0.24)
                   * harmonic * 0.16;

    vec3 primary = palettePrimary(2.82);
    vec3 secondary = paletteSecondary(2.82);
    vec3 accent = paletteAccent(2.82);
    vec3 injection = primary * architecture * (0.12 + 0.07 * harmonic)
                   + secondary * (aisle + floorBars) * (0.09 + 0.06 * energySlow)
                   + accent * (windows + beatArch
                               + downbeatArch + sectionRose) * 0.24
                   + mix(primary, secondary, 0.5) * ambience * 0.08;
    injection *= 1.0 - 0.56 * release;
    vec3 result = (feedback + injection)
                * (1.0 + 0.13 * beatPulse + 0.09 * onsetPulse);
    result = max(result - vec3(0.0043), vec3(0.0));
    color = vec4(result, 1.0);
}
