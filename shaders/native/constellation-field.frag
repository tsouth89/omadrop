#version 330 core

in vec2 uv;
out vec4 color;

#include "scene-uniforms.glsl"

vec2 starPosition(float index) {
    return vec2(sin(index * 12.9898 + 1.7), sin(index * 78.233 + 0.4))
         * vec2(0.58, 0.39);
}

float segmentDistance(vec2 p, vec2 a, vec2 b) {
    vec2 ab = b - a;
    float t = clamp(dot(p - a, ab) / max(dot(ab, ab), 0.0001), 0.0, 1.0);
    return length(p - a - ab * t);
}

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 p = (uv - 0.5) * aspect;

    vec2 previousP = p;
    previousP = rotate2d(0.00010 + 0.014 * snare) * previousP;
    previousP *= 1.0 - 0.021 * beatPulse - 0.014 * onsetPulse - 0.010 * kick
                         + 0.0010 * beatAnticipation;
    previousP += vec2(sin(p.y * 45.0), cos(p.x * 43.0)) * 0.0015 * hat;
    vec2 previousUv = previousP / aspect + 0.5;
    float edge = smoothstep(0.0, 0.06, uv.x) * smoothstep(0.0, 0.06, uv.y)
               * smoothstep(0.0, 0.06, 1.0 - uv.x)
               * smoothstep(0.0, 0.06, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.82, 0.91, harmonic)
                  * (1.0 - 0.050 * beatPulse - 0.060 * onsetPulse) * edge;

    float stars = 0.0;
    float links = 0.0;
    float travelers = 0.0;
    for (int index = 0; index < 12; ++index) {
        float fi = float(index);
        vec2 a = starPosition(fi);
        vec2 b = starPosition(mod(fi + 3.0 + floor(fi / 4.0), 12.0));
        a *= 1.0 + 0.075 * beatPulse + 0.050 * onsetPulse + 0.035 * kick;
        b *= 1.0 + 0.075 * beatPulse + 0.050 * onsetPulse + 0.035 * kick;
        a.x += stereoWidth * 0.035 * sign(a.x);
        b.x += stereoWidth * 0.035 * sign(b.x);
        a += snare * 0.035 * vec2(-a.y, a.x);
        b += snare * 0.035 * vec2(b.y, -b.x);
        float starRadius = 0.009 + 0.010 * spectrumLevel[index * 2]
                         + 0.005 * beatPulse + 0.004 * onsetPulse
                         + 0.022 * kick * (0.5 + 0.5 * sin(fi));
        stars += 1.0 - smoothstep(starRadius, starRadius * 2.2, length(p - a));
        float connection = line(segmentDistance(p, a, b),
                                0.0035 + 0.002 * harmonic
                                + 0.0035 * beatPulse);
        links = max(links, connection);
        float travel = fract(flowTime * (0.18 + 0.22 * drive) + fi * 0.137 + beatPhase);
        vec2 node = mix(a, b, travel);
        travelers += (1.0 - smoothstep(0.008, 0.022, length(p - node))) * hat;
    }
    float anchor = line(length(p) - (0.055 + 0.050 * beatPulse
                           + 0.045 * kick), 0.008 + 0.004 * beatPulse);
    float beatWave = line(length(p) - mix(0.12, 0.58,
                          1.0 - clamp(beatPulse, 0.0, 1.0)), 0.010)
                   * beatPulse * clockConfidence;
    float downbeatPulse = line(length(p) - mix(0.07, 0.70, beatPhase), 0.009)
                        * downbeat * clockConfidence;
    float sectionLink = line(abs(p.x + p.y) - 0.22 * section, 0.010) * section;
    float nebula = line(sin(p.x * 8.0 + p.y * 11.0 + flowTime * 0.11), 0.25)
                 * harmonic * 0.13;

    vec3 primary = palettePrimary(3.28);
    vec3 secondary = paletteSecondary(3.28);
    vec3 accent = paletteAccent(3.28);
    vec3 injection = primary * stars * (0.13 + 0.07 * spectralCentroid)
                   + secondary * links * (0.055 + 0.075 * harmonic)
                   + accent * (travelers + anchor + beatWave
                               + downbeatPulse + sectionLink) * 0.24
                   + mix(primary, secondary, 0.5) * nebula * 0.07;
    injection *= 1.0 - 0.62 * release;
    vec3 result = (feedback + injection)
                * (1.0 + 0.09 * beatPulse + 0.10 * onsetPulse);
    result = max(result - vec3(0.0048), vec3(0.0));
    color = vec4(result, 1.0);
}
