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

float backboneX(float y) {
    float slowShape = 0.105 * sin(y * 4.4 + flowTime * 0.13
                                + phrasePhase * tau);
    float fineShape = 0.030 * sin(y * 10.0 - flowTime * 0.09
                                + barPhase * tau * 0.25);
    float snareBend = snare * 0.110 * sin((y + 0.54) * 3.4 + flowTime * 0.31);
    return slowShape + fineShape + snareBend;
}

void main() {
    vec2 aspect = vec2(resolution.x / max(1.0, resolution.y), 1.0);
    vec2 p = (uv - 0.5) * aspect;
    p.x -= stereoWidth * 0.025 * sin(p.y * 4.0 + flowTime * 0.19);

    // Feedback follows the organism longitudinally. Each percussion role uses
    // a different deformation instead of sharing a global camera impulse.
    vec2 previousP = p;
    previousP.y += 0.00008 + 0.00022 * energySlow;
    previousP.x -= snare * 0.0085 * sin(previousP.y * 5.4 + flowTime * 0.43);
    previousP.x += hat * 0.0018 * sin(previousP.y * 34.0 - flowTime * 2.8);
    float previousSpine = backboneX(previousP.y);
    previousP.x = previousSpine + (previousP.x - previousSpine)
                * (1.0 - 0.0180 * beatPulse - 0.0400 * onsetPulse
                   - 0.0100 * kick
                   + 0.0025 * beatAnticipation * clockConfidence);
    vec2 previousUv = previousP / aspect + 0.5;
    float edge = smoothstep(0.0, 0.075, uv.x)
               * smoothstep(0.0, 0.075, uv.y)
               * smoothstep(0.0, 0.075, 1.0 - uv.x)
               * smoothstep(0.0, 0.075, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.74, 0.86, harmonic)
                  * mix(1.0, 1.012, release)
                  * (1.0 - 0.040 * beatPulse - 0.090 * onsetPulse
                         - 0.045 * hat) * edge;

    float low = clamp(0.5 * spectrumLevel[3] + 0.5 * spectrumLevel[7], 0.0, 2.0);
    float middle = clamp(0.5 * spectrumLevel[13] + 0.5 * spectrumLevel[18],
                         0.0, 2.0);
    float high = clamp(0.5 * spectrumLevel[25] + 0.5 * spectrumLevel[30],
                       0.0, 2.0);

    float spineX = backboneX(p.y);
    float filamentWidth = 0.0065 + 0.0045 * middle + 0.0020 * development;
    float filament = line(p.x - spineX, filamentWidth)
                   * smoothstep(0.63, 0.49, abs(p.y));

    float loops = 0.0;
    float attachments = 0.0;
    float loopInterior = 0.0;
    for (int index = 0; index < 5; ++index) {
        float fi = float(index);
        float anchorY = -0.36 + fi * 0.18;
        float anchorX = backboneX(anchorY);
        float side = mod(fi, 2.0) < 0.5 ? -1.0 : 1.0;
        float reach = mix(0.075, 0.125, development)
                    + 0.012 * sin(fi * 2.1 + phrasePhase * tau);
        vec2 loopP = p - vec2(anchorX + side * reach, anchorY);
        loopP = rotate2d(side * (0.12 + 0.30 * snare)) * loopP;
        loopP.x *= mix(0.78, 0.62, development);
        float pulse = kick * (0.026 + 0.010 * sin(fi * 1.7 + barPhase * tau));
        float anticipationTension = beatAnticipation * clockConfidence * 0.018;
        float loopRadius = 0.066 + 0.009 * sin(flowTime * 0.13 + fi * 1.9)
                         + 0.035 * beatPulse + 0.052 * onsetPulse
                         + pulse - anticipationTension;
        float loopDistance = length(loopP);
        float loopLine = line(loopDistance - loopRadius,
                              0.0065 + 0.0035 * low);
        loops = max(loops, loopLine);
        loopInterior = max(loopInterior,
            smoothstep(loopRadius, loopRadius - 0.030, loopDistance));

        float between = (p.x - anchorX) * side;
        float attachment = line(p.y - anchorY,
                                0.0055 + 0.0030 * bandLevel[2]);
        attachment *= smoothstep(-0.012, 0.025, between)
                    * smoothstep(reach + 0.025, reach - 0.020, between);
        attachments = max(attachments, attachment);
    }

    // Hats travel as small nodes along the persistent filament.
    float nodePosition = fract((p.y + 0.62) * 4.1
                             - flowTime * (0.72 + 0.55 * drive)
                             - beatPhase * 0.75);
    float nodes = line(nodePosition - 0.5, 0.055)
                * line(p.x - spineX, 0.016 + 0.006 * high)
                * hat * (1.08 + 0.52 * high);
    float hatRungs = line(
        sin((p.y + 0.62) * 42.0 - flowTime * 1.1), 0.030)
        * line(abs(p.x - spineX) - 0.050, 0.014)
        * smoothstep(0.60, 0.45, abs(p.y)) * hat;

    float beatY = mix(-0.47, 0.47, beatPhase);
    vec2 beatPoint = p - vec2(backboneX(beatY), beatY);
    float beatNode = 1.0 - smoothstep(0.022, 0.055, length(beatPoint));
    beatNode *= clockConfidence * (0.15 + 0.85 * downbeat);
    float sectionBranch = line(abs(p.y) - mix(0.08, 0.45, section), 0.012)
                        * line(abs(p.x - spineX) - 0.10, 0.022)
                        * section;

    float cellPhase = sin(p.y * 13.0 - flowTime * 0.28)
                    + sin((p.x - spineX) * 17.0 + flowTime * 0.21);
    float backgroundMedium = line(cellPhase, 0.23)
                           * smoothstep(0.48, 0.07, abs(p.x - spineX))
                           * smoothstep(0.70, 0.48, abs(p.y))
                           * harmonic * 0.24;

    float focalSubject = filament * (0.58 + 0.42 * bandLevel[2])
                       + loops * (0.24 + 0.30 * low + 0.18 * kick)
                       + attachments * (0.22 + 0.34 * middle);
    float accents = nodes + beatNode + sectionBranch;

    vec3 primary = palettePrimary(0.88);
    vec3 secondary = paletteSecondary(0.88);
    vec3 filamentColor = mix(primary, secondary,
        0.25 + 0.24 * spectralCentroid
        + 0.16 * sin(p.y * 4.0 - flowTime * 0.12));
    vec3 loopColor = mix(secondary, primary,
        0.44 + 0.18 * sin(p.y * 5.0 + phrasePhase * tau));
    vec3 mediumColor = mix(primary * 0.38, secondary * 0.36,
                           0.5 + 0.5 * sin(p.y * 3.0 + flowTime * 0.08));
    vec3 accentColor = mix(paletteAccent(0.88), vec3(0.94, 0.98, 1.0),
                           0.48 + 0.24 * hat);
    float lifecycleLight = 0.13 + 0.045 * development + 0.050 * drive
                         + 0.065 * peak;
    vec3 injection = mediumColor * backgroundMedium * (0.11 + 0.10 * harmonic)
                   + filamentColor * filament * (0.31 + lifecycleLight)
                   + loopColor * (focalSubject - filament * (0.58 + 0.42 * bandLevel[2]))
                     * (lifecycleLight + 0.060 * max(0.0, energySlope))
                   + accentColor * accents * (0.20 + 0.13 * peak)
                   + mix(accentColor, vec3(1.0), 0.44) * hatRungs * 0.30;
    injection += mix(filamentColor, vec3(1.0), 0.30)
               * (filament + loops * 0.72 + attachments * 0.46)
               * onsetPulse * 0.16;
    injection += loopColor * loopInterior * percussive * 0.009;
    injection *= 1.0 - 0.57 * release;

    float organismMask = smoothstep(0.86, 0.66, abs(p.x))
                       * smoothstep(0.76, 0.56, abs(p.y));
    vec3 result = (feedback + injection) * organismMask
                * (1.0 + 0.14 * beatPulse + 0.27 * onsetPulse);
    result = max(result - vec3(0.0043), vec3(0.0));
    color = vec4(result, 1.0);
}
