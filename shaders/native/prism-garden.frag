#version 330 core

in vec2 uv;
out vec4 color;

#include "scene-uniforms.glsl"

float crystalDistance(vec2 p, float centerX, float height, float width) {
    vec2 q = p - vec2(centerX, -0.42 + height * 0.5);
    q.y /= max(height, 0.04);
    q.x /= max(width, 0.01);
    float body = max(abs(q.x) * 0.72 + abs(q.y) * 0.22, abs(q.y));
    return abs(body - 0.50);
}

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 p = (uv - 0.5) * aspect;

    vec2 previousP = p;
    previousP.y -= 0.00010 + 0.00028 * energySlow;
    previousP.x *= 1.0 - 0.020 * beatPulse - 0.013 * onsetPulse - 0.012 * kick
                         + 0.0012 * beatAnticipation;
    previousP.x -= snare * 0.009 * previousP.y;
    previousP += vec2(sin(p.y * 47.0), cos(p.x * 41.0)) * 0.0014 * hat;
    vec2 previousUv = previousP / aspect + 0.5;
    float edge = smoothstep(0.0, 0.07, uv.x) * smoothstep(0.0, 0.07, uv.y)
               * smoothstep(0.0, 0.07, 1.0 - uv.x)
               * smoothstep(0.0, 0.07, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.91, 0.958, harmonic)
                  * (1.0 - 0.038 * beatPulse - 0.055 * onsetPulse) * edge;

    float stems = 0.0;
    float facets = 0.0;
    float tips = 0.0;
    for (int index = 0; index < 9; ++index) {
        float fi = float(index);
        float x = (fi - 4.0) * 0.105;
        float spectral = spectrumLevel[index * 3 + 2];
        float height = 0.22 + 0.31 * development + 0.075 * spectral
                     + 0.070 * beatPulse + 0.055 * onsetPulse
                     + kick * (0.055 + 0.014 * mod(fi, 3.0));
        float width = 0.024 + 0.008 * bandLevel[index % 6];
        vec2 q = p;
        q.x -= snare * 0.065 * sin(fi * 1.7 + barPhase * tau);
        float crystal = line(crystalDistance(q, x, height, width), 0.055);
        stems = max(stems, crystal);
        float localY = (q.y + 0.42) / max(height, 0.04);
        facets = max(facets, crystal * line(fract(localY * 4.0) - 0.5, 0.10));
        vec2 tip = q - vec2(x, -0.42 + height);
        tips += (1.0 - smoothstep(0.008, 0.026, length(tip)))
              * hat * (0.6 + 0.3 * spectral);
    }
    float ground = line(p.y + 0.42, 0.008 + 0.005 * bandLevel[0]);
    float downbeatWave = line(abs(p.x) - mix(0.04, 0.58, beatPhase), 0.010)
                       * downbeat * clockConfidence;
    float sectionCanopy = line(p.y - mix(-0.25, 0.38, section), 0.014) * section;
    float harmonicMist = line(sin(p.x * 11.0 + p.y * 8.0 - flowTime * 0.25), 0.20)
                       * harmonic * smoothstep(0.62, 0.12, abs(p.y)) * 0.20;

    vec3 primary = palettePrimary(1.34);
    vec3 secondary = paletteSecondary(1.34);
    vec3 accent = paletteAccent(1.34);
    vec3 injection = primary * stems * (0.13 + 0.08 * energySlow)
                   + secondary * facets * (0.10 + 0.08 * spectralCentroid)
                   + accent * (tips + downbeatWave + sectionCanopy) * 0.22
                   + mix(primary, secondary, 0.5) * harmonicMist * 0.10
                   + primary * ground * 0.09;
    injection *= 1.0 - 0.55 * release;
    vec3 result = (feedback + injection) * smoothstep(0.82, 0.62, abs(p.x))
                * (1.0 + 0.13 * beatPulse + 0.10 * onsetPulse);
    result = max(result - vec3(0.0044), vec3(0.0));
    color = vec4(result, 1.0);
}
