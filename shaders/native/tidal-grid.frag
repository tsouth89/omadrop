#version 330 core

in vec2 uv;
out vec4 color;

#include "scene-uniforms.glsl"

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 p = (uv - 0.5) * aspect;
    float horizon = 0.08 + 0.05 * sin(flowTime * 0.12 + phrasePhase * tau);
    float wave = 0.055 * sin(p.x * 7.0 - flowTime * 0.35)
               + 0.028 * sin(p.x * 17.0 + flowTime * 0.22);
    wave += (0.075 * beatPulse + 0.055 * onsetPulse + 0.105 * kick)
          * exp(-5.0 * abs(p.x));

    vec2 previousP = p;
    previousP.y += 0.00014 + 0.00030 * energySlow;
    previousP.y -= (0.012 * beatPulse + 0.009 * onsetPulse + 0.015 * kick)
                 * exp(-3.0 * abs(previousP.x));
    previousP.x -= snare * 0.016 * (previousP.y - horizon);
    previousP.y += hat * 0.0018 * sin(previousP.x * 50.0 - flowTime * 4.0);
    vec2 previousUv = previousP / aspect + 0.5;
    float edge = smoothstep(0.0, 0.07, uv.x) * smoothstep(0.0, 0.07, uv.y)
               * smoothstep(0.0, 0.07, 1.0 - uv.x)
               * smoothstep(0.0, 0.07, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.91, 0.965, harmonic)
                  * (1.0 - 0.038 * beatPulse - 0.050 * onsetPulse) * edge;

    float surfaceY = horizon + wave;
    float surface = line(p.y - surfaceY, 0.010 + 0.006 * bandLevel[1]);
    float perspectiveY = max(0.025, horizon - p.y + 0.05);
    float depth = 0.18 / perspectiveY + flowTime * (0.28 + 0.20 * drive)
                + beatPulse * 1.25 + onsetPulse * 0.95 + kick * 2.0
                - beatAnticipation * 0.45;
    float horizontalGrid = line(sin(depth * tau), 0.10 + 0.025 * energyFast)
                         * smoothstep(surfaceY + 0.03, surfaceY - 0.08, p.y);
    float spread = 1.0 / max(0.12, perspectiveY * 4.0);
    float verticalGrid = line(sin((p.x * spread + snare * p.y * 0.9) * 18.0), 0.06)
                       * smoothstep(surfaceY + 0.03, surfaceY - 0.08, p.y);
    float crestTicks = line(sin(p.x * 44.0 - flowTime * 2.2), 0.055)
                     * surface * hat;
    float downbeatTide = line(p.y - mix(-0.44, surfaceY, beatPhase), 0.010)
                       * downbeat * clockConfidence;
    float sectionHorizon = line(p.y - horizon - section * 0.22, 0.014) * section;
    float sky = line(sin(p.x * 5.0 + p.y * 9.0 + flowTime * 0.13), 0.24)
              * harmonic * smoothstep(horizon + 0.5, horizon, p.y) * 0.16;

    vec3 primary = palettePrimary(2.34);
    vec3 secondary = paletteSecondary(2.34);
    vec3 accent = paletteAccent(2.34);
    vec3 injection = primary * horizontalGrid * (0.09 + 0.06 * bandLevel[0])
                   + secondary * verticalGrid * (0.08 + 0.06 * bandLevel[2])
                   + mix(primary, secondary, 0.42) * surface * 0.16
                   + accent * (crestTicks + downbeatTide + sectionHorizon) * 0.22
                   + secondary * sky * 0.07;
    injection *= 1.0 - 0.58 * release;
    vec3 result = (feedback + injection)
                * (1.0 + 0.13 * beatPulse + 0.09 * onsetPulse);
    result = max(result - vec3(0.0044), vec3(0.0));
    color = vec4(result, 1.0);
}
