#version 330 core

in vec2 uv;
out vec4 color;

uniform sampler2D previousFrame;
uniform sampler2D artworkFrame;
uniform vec2 resolution;
uniform float artworkAvailable;
uniform float artworkAspect;
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
    float artworkStructure = 0.0;
    float artworkLight = 0.0;
    if (artworkAvailable > 0.5) {
        vec2 artworkP = uv - 0.5;
        float screenAspect = resolution.x / max(1.0, resolution.y);
        if (screenAspect > artworkAspect) artworkP.x *= screenAspect / artworkAspect;
        else artworkP.y *= artworkAspect / screenAspect;
        vec2 artworkUv = artworkP + 0.5;
        if (all(greaterThanEqual(artworkUv, vec2(0.0)))
            && all(lessThanEqual(artworkUv, vec2(1.0)))) {
            artworkUv.y = 1.0 - artworkUv.y;
            vec2 artworkTexel = 1.0 / vec2(textureSize(artworkFrame, 0));
            vec3 artwork = texture(artworkFrame, artworkUv).rgb;
            vec3 artworkRight = texture(
                artworkFrame, artworkUv + vec2(artworkTexel.x, 0.0)).rgb;
            vec3 artworkUp = texture(
                artworkFrame, artworkUv + vec2(0.0, artworkTexel.y)).rgb;
            artworkLight = dot(artwork, vec3(0.299, 0.587, 0.114));
            artworkStructure = clamp(
                0.24 * artworkLight + 1.8 * length(artwork - artworkRight)
                + 1.8 * length(artwork - artworkUp), 0.0, 1.0);
        }
    }
    vec2 center = vec2(0.018 * sin(flowTime * 0.23 + phrasePhase * tau),
                       0.014 * cos(flowTime * 0.19 + phrasePhase * tau));
    center.x += stereoWidth * 0.026 * sin(flowTime * 0.29 + barPhase * tau);
    vec2 p = (uv - 0.5) * aspect - center;
    // Development gradually opens an asymmetric corridor. It avoids the
    // static target-like symmetry of a purely radial tunnel while retaining a
    // stable aperture that the eye can track.
    p.x += p.y * (0.035 + 0.055 * development)
         * sin(flowTime * 0.17 + phrasePhase * tau);
    vec2 perspectiveP = p * vec2(1.0, mix(0.92, 0.76, development));
    float radius = max(0.035, length(perspectiveP));
    float angle = atan(p.y, p.x);

    // Sample the previous frame through the tunnel's own flow field. The
    // vanishing point remains stable while detail travels through depth.
    float pull = 0.00018 + 0.00035 * energySlow + 0.00025 * drive
               + 0.0120 * beatPulse + 0.0160 * kick
               - 0.0024 * beatAnticipation * clockConfidence;
    float twist = 0.0005 * sin(radius * 8.0 + flowTime * 0.31)
                + 0.0210 * snare * smoothstep(0.08, 0.75, radius);
    vec2 previousP = rotate2d(twist) * p * (1.0 - pull);
    previousP += normalize(p) * sin(angle * 10.0 + flowTime * 3.0)
               * 0.0080 * hat;
    vec2 previousUv = previousP / aspect + 0.5 + center / aspect;
    float edge = smoothstep(0.0, 0.08, uv.x)
               * smoothstep(0.0, 0.08, uv.y)
               * smoothstep(0.0, 0.08, 1.0 - uv.x)
               * smoothstep(0.0, 0.08, 1.0 - uv.y);
    vec3 feedback = texture(previousFrame, clamp(previousUv, 0.001, 0.999)).rgb
                  * mix(0.905, 0.958, harmonic)
                  * mix(1.0, 1.014, release)
                  * (1.0 - 0.045 * beatPulse - 0.055 * onsetPulse - 0.035 * kick
                         - 0.045 * snare - 0.050 * hat) * edge;

    float depth = mix(0.24, 0.34, development) / radius
                + flowTime * (0.20 + 0.20 * development + 0.28 * drive
                              + 0.18 * bandLevel[0]);
    depth += beatPulse * 2.8 + onsetPulse * 2.2 + kick * 4.2
           - beatAnticipation * 0.8;
    depth += artworkStructure * mix(0.58, 0.16, development);
    float localLow = clamp(0.5 * spectrumLevel[3] + 0.5 * spectrumLevel[7],
                           0.0, 2.0);
    float localMid = clamp(0.5 * spectrumLevel[13] + 0.5 * spectrumLevel[18],
                           0.0, 2.0);
    float localHigh = clamp(0.5 * spectrumLevel[24] + 0.5 * spectrumLevel[29],
                            0.0, 2.0);
    float rings = line(sin(depth * tau * mix(0.82, 1.12, development)),
                       0.12 + 0.025 * energyFast);
    rings *= smoothstep(0.055, 0.16, radius) * smoothstep(1.18, 0.48, radius);
    rings *= (0.66 + 0.20 * localLow)
           * (1.0 + 0.26 * beatPulse + 0.42 * kick);

    float wallBend = 0.20 * sin(radius * 6.0 - flowTime * 0.37)
                   + 0.92 * snare;
    float ribCount = mix(8.0, 14.0, development) + 2.0 * peak;
    float ribs = line(sin(angle * ribCount + wallBend + phrasePhase * 0.7),
                      0.055 + 0.018 * bandLevel[3]);
    ribs *= smoothstep(0.12, 0.46, radius) * smoothstep(1.25, 0.62, radius);
    ribs *= (0.68 + 0.18 * localMid) * (1.0 + 0.34 * snare);

    float fineRibs = line(sin(angle * 42.0 - flowTime * 0.7
                              + hat * 2.4), 0.028);
    fineRibs *= hat * (1.20 + 0.72 * peak) * (0.76 + 0.32 * localHigh)
                    * smoothstep(0.22, 0.62, radius)
                    * smoothstep(1.15, 0.58, radius);

    float kickShock = line(radius - (0.17 + 0.09 * kick),
                           0.014 + 0.008 * kick)
                    * kick * smoothstep(0.06, 0.18, radius);
    float snareShutter = line(
        sin(angle * 12.0 + phrasePhase * tau + snare * 2.1), 0.030)
        * smoothstep(0.17, 0.42, radius)
        * smoothstep(1.10, 0.58, radius) * snare;
    float hatGlints = line(sin(angle * 56.0 - beatPhase * tau), 0.020)
        * line(sin(radius * 34.0 + flowTime * 0.5), 0.075)
        * smoothstep(0.20, 0.48, radius)
        * smoothstep(1.15, 0.62, radius) * hat;

    float beatRingRadius = 0.16 + beatPhase * 0.72;
    float beatRing = line(radius - beatRingRadius, 0.008 + 0.012 * percussive)
                   * clockConfidence
                   * (0.20 * beatPulse + 0.80 * downbeat);
    float sectionWave = line(radius - section * 0.9, 0.018) * section * 0.55;

    float backgroundMedium = line(
        sin(radius * 7.0 - flowTime * 0.42 + angle * 2.0), 0.24);
    backgroundMedium *= harmonic * smoothstep(0.10, 0.30, radius)
                      * smoothstep(1.30, 0.64, radius) * 0.24;
    float artworkMedium = artworkStructure * mix(0.44, 0.14, development)
                        * smoothstep(0.10, 0.28, radius)
                        * smoothstep(1.24, 0.58, radius);
    backgroundMedium = max(backgroundMedium, artworkMedium);
    float focalSubject = max(
        rings * (0.32 + 0.45 * bandLevel[1] + 0.34 * kick),
        ribs * (0.20 + 0.36 * bandLevel[3] + 0.18 * snare));
    focalSubject *= 0.72 + 0.22 * sin(barPhase * tau + radius * 2.0);
    float accents = fineRibs * (0.76 + 0.72 * hat) + beatRing + sectionWave;

    vec3 primary = palettePrimary(-0.16);
    vec3 secondary = paletteSecondary(-0.16);
    vec3 tunnelColor = mix(primary, secondary,
                           0.24 + 0.26 * spectralCentroid
                           + 0.24 * sin(angle * 2.0 + flowTime * 0.12));
    tunnelColor = mix(tunnelColor, vec3(0.86, 0.94, 1.0),
                      0.10 + 0.16 * hat);
    vec3 mediumColor = mix(primary * 0.42, secondary * 0.40,
                           0.5 + 0.5 * sin(angle + flowTime * 0.08
                                         + artworkLight * 0.9));
    vec3 accentColor = mix(paletteAccent(-0.16), vec3(0.92, 0.97, 1.0),
                           0.42 + 0.24 * hat);
    float lifecycleLight = 0.125 + 0.045 * development + 0.05 * drive
                         + 0.06 * peak + 0.10 * beatPulse;
    vec3 injection = mediumColor * backgroundMedium * (0.12 + 0.10 * harmonic)
                   + tunnelColor * focalSubject
                     * (lifecycleLight + 0.07 * max(0.0, energySlope))
                   + accentColor * accents * (0.32 + 0.16 * peak)
                   + mix(accentColor, vec3(1.0), 0.28) * kickShock * 0.50
                   + mix(accentColor, vec3(1.0), 0.40) * snareShutter * 0.31
                   + mix(tunnelColor, vec3(1.0), 0.58) * hatGlints * 0.43;
    injection *= 1.0 - 0.58 * release;

    // Preserve a dark, readable vanishing point and prevent feedback haze.
    float core = smoothstep(0.055, 0.15, radius);
    vec3 result = (feedback + injection) * core
                * (1.0 + 0.09 * beatPulse + 0.08 * onsetPulse + 0.060 * kick
                       + 0.060 * snare + 0.050 * hat);
    result = max(result - vec3(0.0045), vec3(0.0));
    color = vec4(result, 1.0);
}
