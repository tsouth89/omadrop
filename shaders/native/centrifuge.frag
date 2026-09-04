#version 330 core

in vec2 uv;
out vec4 color;

uniform sampler2D previousFrame;
uniform vec2 resolution;
uniform vec3 albumColor;
uniform float bandLevel[6];
uniform float spectrumLevel[32];
uniform float flowTime;
uniform float beatPhase;
uniform float beatAnticipation;
uniform float beatPulse;
uniform float onsetPulse;
uniform float downbeat;
uniform float barPhase;
uniform float phrasePhase;
uniform float clockConfidence;
uniform float kick;
uniform float snare;
uniform float hat;
uniform float percussive;
uniform float harmonic;
uniform float spectralCentroid;
uniform float stereoWidth;
uniform float energyFast;
uniform float energySlow;
uniform float energySlope;
uniform float section;
uniform float development;
uniform float drive;
uniform float peak;
uniform float release;
uniform float sceneBeats;

#include "common.glsl"

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 center = vec2(0.022 * sin(flowTime * 0.16 + phrasePhase * tau),
                       0.017 * cos(flowTime * 0.13 + barPhase * tau));
    center.x += stereoWidth * 0.032 * sin(flowTime * 0.24);
    vec2 p = (uv - 0.5) * aspect - center;
    float radius = max(0.001, length(p));
    float angle = atan(p.y, p.x);

    // The outer frame and inner aperture counter-rotate. Snare transfers
    // angular momentum between them while kick changes aperture depth.
    float region = smoothstep(0.25, 0.68, radius);
    float idleSpin = 0.00020 + 0.00040 * drive;
    float snareSpin = 0.016 * snare;
    float feedbackRotation = mix(-idleSpin - snareSpin,
                                  idleSpin + snareSpin * 0.46, region);
    float aperturePull = 0.00020 + 0.00045 * energySlow
                       + 0.0120 * beatPulse + 0.0080 * onsetPulse
                       + 0.0090 * kick
                       - 0.0014 * beatAnticipation * clockConfidence;
    vec2 previousP = rotate2d(feedbackRotation) * p * (1.0 - aperturePull);
    previousP += vec2(sin(p.y * 26.0), cos(p.x * 23.0)) * 0.0018 * hat;
    vec2 previousUv = previousP / aspect + 0.5 + center / aspect;
    float edge = smoothstep(0.0, 0.07, uv.x)
               * smoothstep(0.0, 0.07, uv.y)
               * smoothstep(0.0, 0.07, 1.0 - uv.x)
               * smoothstep(0.0, 0.07, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.938, 0.974, harmonic) * mix(1.0, 1.012, release)
                  * (1.0 - 0.034 * beatPulse - 0.050 * onsetPulse) * edge;

    float low = clamp(0.5 * spectrumLevel[3] + 0.5 * spectrumLevel[8], 0.0, 2.0);
    float middle = clamp(0.5 * spectrumLevel[14] + 0.5 * spectrumLevel[18], 0.0, 2.0);
    float high = clamp(0.5 * spectrumLevel[25] + 0.5 * spectrumLevel[30], 0.0, 2.0);

    float apertureRadius = mix(0.13, 0.22, development)
                         + 0.060 * beatPulse + 0.050 * onsetPulse
                         + 0.075 * kick;
    float apertureEdge = line(radius - apertureRadius, 0.010 + 0.009 * low);
    float shellPhase = radius * mix(24.0, 38.0, development)
                     - flowTime * (1.2 + 1.7 * drive)
                     - beatPulse * 1.8 - kick * 2.4;
    float shells = line(sin(shellPhase), 0.10 + 0.025 * energyFast);
    shells *= smoothstep(apertureRadius + 0.015, apertureRadius + 0.13, radius)
            * smoothstep(1.18, 0.52, radius);

    vec2 frameP = rotate2d(0.15 * sin(flowTime * 0.11)
                                 + 0.38 * snare) * p;
    float squareRadius = max(abs(frameP.x), abs(frameP.y));
    float frameSize = mix(0.52, 0.70, development) + 0.025 * middle;
    float squareFrame = line(squareRadius - frameSize, 0.012 + 0.008 * middle);
    squareFrame *= smoothstep(0.08, 0.32, radius);

    float spokeCount = mix(8.0, 14.0, development) + 4.0 * peak;
    float spokes = line(sin(angle * spokeCount + snare * 1.4
                          - flowTime * (0.22 + 0.45 * drive)), 0.050);
    spokes *= smoothstep(apertureRadius, apertureRadius + 0.10, radius)
            * smoothstep(frameSize + 0.08, frameSize - 0.12, radius);

    // Hats run around the existing square boundary and divide its corners.
    float frameTravel = fract(angle / tau + flowTime * 0.55 + beatPhase);
    float cornerTicks = line(sin(frameTravel * tau * 18.0), 0.065);
    cornerTicks *= squareFrame * hat * (0.66 + 0.24 * high);
    float downbeatSquare = line(squareRadius - mix(0.16, frameSize, beatPhase),
                                0.008 + 0.008 * percussive)
                         * downbeat * clockConfidence * 0.55;
    float beatSquare = line(squareRadius - mix(0.18, frameSize,
                             1.0 - clamp(beatPulse, 0.0, 1.0)), 0.010)
                     * beatPulse * clockConfidence;
    float sectionSquare = line(squareRadius - section * frameSize,
                               0.014) * section * 0.52;

    float backgroundMedium = line(
        sin(radius * 5.0 + angle * 3.0 + flowTime * 0.35), 0.25);
    backgroundMedium *= harmonic * smoothstep(0.12, 0.34, radius)
                      * smoothstep(1.24, 0.66, radius) * 0.22;
    float focalSubject = max(apertureEdge * (0.42 + 0.40 * low + 0.28 * kick),
                             shells * (0.24 + 0.34 * bandLevel[1]));
    focalSubject = max(focalSubject,
                       squareFrame * (0.26 + 0.32 * middle + 0.20 * snare));
    focalSubject = max(focalSubject, spokes * (0.18 + 0.28 * bandLevel[3]));
    float accents = cornerTicks + beatSquare + downbeatSquare + sectionSquare;

    vec3 primary = palettePrimary(0.42);
    vec3 secondary = paletteSecondary(0.42);
    vec3 focalColor = mix(primary, secondary,
                          0.26 + 0.24 * spectralCentroid
                          + 0.20 * sin(angle * 2.0 - flowTime * 0.10));
    vec3 mediumColor = mix(primary * 0.38, secondary * 0.38,
                           0.5 + 0.5 * sin(angle - flowTime * 0.07));
    vec3 accentColor = mix(paletteAccent(0.42), vec3(0.94, 0.98, 1.0),
                           0.48 + 0.22 * hat);
    float lifecycleLight = 0.12 + 0.045 * development + 0.055 * drive
                         + 0.065 * peak;
    vec3 injection = mediumColor * backgroundMedium * (0.11 + 0.10 * harmonic)
                   + focalColor * focalSubject
                     * (lifecycleLight + 0.065 * max(0.0, energySlope))
                   + accentColor * accents * (0.17 + 0.11 * peak);
    injection *= 1.0 - 0.56 * release;

    float core = smoothstep(apertureRadius * 0.52, apertureRadius, radius);
    vec3 result = (feedback + injection) * core
                * (1.0 + 0.13 * beatPulse + 0.09 * onsetPulse);
    result = max(result - vec3(0.0042), vec3(0.0));
    color = vec4(result, 1.0);
}
