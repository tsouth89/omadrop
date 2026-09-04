#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <projectM-4/projectM.h>
#include <png.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <regex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "audio_features.h"
#include "audio_queue.h"
#include "mpris_state.h"
#include "musical_structure.h"
#include "music_frame.h"
#include "native_renderer.h"
#include "paired_display.h"
#include "paired_music_state.h"
#include "preset_adapters.h"
#include "preset_profiles.h"
#include "preset_selector.h"
#include "structure_timeline.h"
#include "visual_motifs.h"

namespace {
constexpr int width = 1280;
constexpr int height = 720;
constexpr float visualTempo = 0.82f;
volatile sig_atomic_t stopRequested = 0;

void requestStop(int) {
    stopRequested = 1;
}

const char* vertexSource = R"GLSL(
#version 330 core
out vec2 uv;
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    uv = p;
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)GLSL";

const char* fragmentSource = R"GLSL(
#version 330 core
in vec2 uv;
out vec4 color;
uniform sampler2D sourceFrame;
uniform sampler2D nextFrame;
uniform float presetMix;
uniform sampler2D coverFrame;
uniform vec2 resolution;
uniform float coverAspect;
uniform float coverMix;
uniform vec3 albumColor;
uniform float paletteInfluence;
uniform float bassLevel;
uniform float bassImpact;
uniform float midLevel;
uniform float trebleLevel;
uniform float midImpact;
uniform float trebleImpact;
uniform int asciiEnabled;
uniform int transitionMode;
uniform int sourceReactionMode;
uniform int nextReactionMode;
uniform vec3 sourceReactionGain;
uniform vec3 nextReactionGain;
uniform float asciiExposure;
uniform float fieldExposure;
uniform int nativeRenderer;
uniform float visibility;

float luminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

vec2 reactedUv(vec2 sampleUv, int mode, vec3 gain) {
    vec2 p = sampleUv - 0.5;
    float radius = max(0.001, length(p));
    float angle = atan(p.y, p.x);
    // Quiet low-end movement stays restrained. A genuinely hard transient
    // crosses into a stronger deformation of the preset's own geometry.
    float kickAccent = smoothstep(0.52, 0.82, bassImpact);
    float kick = min(2.10, (1.10 * bassImpact + 0.75 * kickAccent) * gain.x);
    float snare = midImpact * gain.y;
    float hat = trebleImpact * gain.z;
    vec2 movement = vec2(0.0);
    if (mode == 0) {
        // Contortion: kick opens tunnel depth, snare turns its existing walls.
        movement = p * (0.026 * kick)
                 + vec2(-p.y, p.x) * (0.009 * snare);
        movement += p / radius * sin(angle * 10.0 + radius * 42.0) * (0.0018 * hat);
    } else if (mode == 1) {
        // Wire Dance: separate its connected lobes instead of shaking the camera.
        movement = vec2(sign(p.x), sign(p.y)) * (0.018 * kick)
                 * smoothstep(0.05, 0.44, radius);
        movement += vec2(-p.y, p.x) * (0.010 * snare);
        movement += vec2(sin(sampleUv.y * 41.0), cos(sampleUv.x * 37.0))
                  * (0.0022 * hat);
    } else if (mode == 2) {
        // Halls of Centrifuge: a kick travels down the vanishing point.
        movement = p * (0.027 * kick * (1.0 - 0.35 * smoothstep(0.18, 0.70, radius)));
        movement += vec2(-p.y, p.x) * (0.012 * snare * (0.35 + radius));
        movement += p / radius * sin(radius * 46.0) * (0.0020 * hat);
    } else if (mode == 3) {
        // Night Cathedral: compress the corridor and flex its opposing planes.
        movement = p * vec2(0.014, 0.025) * kick;
        movement.x += sin(sampleUv.y * 10.0) * (0.013 * snare);
        movement.y += sin(sampleUv.x * 33.0) * (0.0018 * hat);
    } else if (mode == 4) {
        // Bitterfeld: fracture the crystal field along its existing facets.
        movement.x = sin(sampleUv.y * 9.0 + angle * 2.0) * (0.021 * kick);
        movement.y = sin(sampleUv.x * 8.0 - angle * 2.0) * (0.010 * snare);
        movement += vec2(cos(angle * 12.0), sin(angle * 12.0)) * (0.0022 * hat);
    } else {
        // Airhandler: bend its connected tendrils without breaking their silhouette.
        movement = vec2(sin(sampleUv.y * 8.0 + sampleUv.x * 2.0),
                        sin(sampleUv.x * 7.0 - sampleUv.y * 1.5)) * (0.014 * kick);
        movement += vec2(-p.y, p.x) * (0.010 * snare);
        movement += vec2(cos(sampleUv.y * 39.0), sin(sampleUv.x * 43.0))
                  * (0.0020 * hat);
    }
    return clamp(sampleUv + movement, vec2(0.002), vec2(0.998));
}

vec3 sceneSample(vec2 sampleUv) {
    vec2 coverSampleUv = sampleUv;
    float easedPresetMix = presetMix * presetMix * (3.0 - 2.0 * presetMix);
    float bridge = sin(3.14159265 * easedPresetMix);
    vec2 outgoingUv = (sampleUv - 0.5) * (1.0 - 0.025 * bridge) + 0.5;
    vec2 incomingUv = (sampleUv - 0.5) * (1.025 - 0.025 * easedPresetMix) + 0.5;
    if (transitionMode == 6) {
        // Native scenes share a stable center landmark. Keep it registered
        // while the outer fields exchange in broad connected bands.
        outgoingUv = sampleUv;
        incomingUv = sampleUv;
    }
    outgoingUv = reactedUv(outgoingUv, sourceReactionMode, sourceReactionGain);
    incomingUv = reactedUv(incomingUv, nextReactionMode, nextReactionGain);
    vec3 outgoing = texture(sourceFrame, outgoingUv).rgb;
    vec3 incoming = texture(nextFrame, incomingUv).rgb;

    // Do not reveal the incoming composition as one rectangular layer. Broad
    // connected flow bands let its geometry form inside the outgoing feedback
    // while a temporary luminance match prevents a sudden palette block.
    float flow;
    if (transitionMode == 6) {
        float radius = length(sampleUv - 0.5);
        flow = 0.50 + 0.15 * sin(sampleUv.y * 9.0
                               + sin(sampleUv.x * 6.0) * 1.4)
                    + 0.08 * smoothstep(0.08, 0.72, radius);
    } else if (transitionMode == 0) {
        float radius = length(sampleUv - 0.5);
        flow = 0.38 + radius * 0.42 + 0.09 * sin(radius * 35.0);
    } else if (transitionMode == 1) {
        flow = 0.5 + 0.17 * sin((sampleUv.x + sampleUv.y) * 12.0)
                         + 0.07 * sin(sampleUv.y * 31.0);
    } else if (transitionMode == 2) {
        flow = 0.5 + 0.19 * sin(sampleUv.y * 10.0 + sin(sampleUv.x * 7.0) * 1.6)
                         + 0.07 * sin(sampleUv.y * 27.0 - sampleUv.x * 5.0);
    } else {
        flow = 0.5 + 0.12 * sin(sampleUv.y * 13.0 + sampleUv.x * 4.0)
                         + 0.07 * sin(sampleUv.y * 29.0 - sampleUv.x * 7.0);
    }
    float localMix = smoothstep(flow - 0.46, flow + 0.46, easedPresetMix);
    float nativeAnchor = 0.0;
    if (transitionMode == 6) {
        float radius = length(sampleUv - 0.5);
        nativeAnchor = 1.0 - smoothstep(0.07, 0.30, radius);
        localMix = mix(localMix, easedPresetMix, nativeAnchor);
    }
    float outgoingLight = luminance(outgoing);
    float incomingLight = max(0.08, luminance(incoming));
    vec3 matchedIncoming = incoming * clamp((0.35 + outgoingLight * 0.9) / incomingLight,
                                             0.55, 1.35);
    incoming = mix(incoming, matchedIncoming, (1.0 - easedPresetMix) * 0.42);
    vec3 visual = mix(outgoing, incoming, localMix);
    if (transitionMode == 6) {
        visual += max(outgoing, incoming) * nativeAnchor * bridge * 0.12;
    }
    float visualLight = luminance(visual);
    vec3 albumGrade = visual * mix(vec3(1.0), albumColor * 1.65, 0.58)
                    + albumColor * visualLight * 0.16;
    visual = mix(visual, albumGrade, paletteInfluence);
    if (nativeRenderer != 0) {
        visual = visual / (vec3(1.0) + visual * 0.85);
        float nativeLight = luminance(visual);
        visual = clamp(mix(vec3(nativeLight), visual, 1.34), 0.0, 1.0);
    }
    if (coverMix <= 0.0) return visual;
    vec2 p = coverSampleUv - 0.5;
    float screenAspect = resolution.x / resolution.y;
    if (screenAspect > coverAspect) p.x *= screenAspect / coverAspect;
    else p.y *= coverAspect / screenAspect;
    vec2 coverUv = p + 0.5;
    vec3 cover = vec3(0.0);
    float dissolve = 1.0 - coverMix;
    // The artwork leaves in broad coordinated ribbons, not random pixels.
    // Adjacent rows travel together so the cover remains recognizable through
    // most of the transition and appears to become the feedback field.
    float ribbon = 0.5 + 0.23 * sin(coverUv.y * 17.0)
                       + 0.12 * sin(coverUv.y * 43.0 + 1.7);
    float coverPresence = smoothstep(ribbon - 0.18, ribbon + 0.18, coverMix);
    coverUv.x += dissolve * dissolve * 0.055 * sin(coverUv.y * 31.0 + coverMix * 5.0);
    if (all(greaterThanEqual(coverUv, vec2(0.0))) && all(lessThanEqual(coverUv, vec2(1.0)))) {
        cover = texture(coverFrame, vec2(coverUv.x, 1.0 - coverUv.y)).rgb;
    }
    return mix(visual, cover, coverPresence);
}

void main() {
    if (asciiEnabled == 0) {
        color = vec4(sceneSample(uv) * fieldExposure * visibility, 1.0);
        return;
    }
    // ASCII mode keeps its identity on the cover, but starts from the same
    // full-resolution source used by continuous mode. A sharp image underlay
    // carries typography, faces, and fine illustration beneath the dots.
    vec3 cleanSource = sceneSample(uv);
    vec2 pixel = uv * resolution;
    // Deliberately coarse 2x4 braille cells. The earlier 6x12 grid preserved
    // so much source detail that it read as a slightly dotted normal image.
    vec2 cellSize = vec2(12.0, 24.0);
    vec2 cell = floor(pixel / cellSize);
    vec2 local = mod(pixel, cellSize);

    int dx = local.x < 6.0 ? 0 : 1;
    int dy = clamp(int(local.y / 6.0), 0, 3);
    vec2 center = vec2(dx == 0 ? 3.0 : 9.0, 3.0 + float(dy) * 6.0);
    // Keep transients bounded. Large glyph growth turns every lit cell into a
    // white screen on bright presets and destroys the underlying composition.
    float glyphDistance = length(local - center);
    float kickAccent = smoothstep(0.52, 0.82, bassImpact);
    float coverColorLock = smoothstep(0.0, 0.85, coverMix);
    float visualReaction = 1.0 - coverColorLock;
    // Album artwork needs more than a binary dot mask to survive difficult
    // photography. A dim, color-faithful underlay retains faces, typography,
    // and fine texture while the braille field remains the dominant material.
    // It disappears early in the cover dissolve, so presets stay pure ASCII.
    float underlayMix = smoothstep(0.62, 0.96, coverMix);
    vec3 underlaySource = cleanSource;
    float underlayLight = luminance(underlaySource);
    float underlayTarget = pow(clamp(underlayLight, 0.0, 1.0), 0.88);
    float underlayMaximum = max(underlaySource.r,
                                max(underlaySource.g, underlaySource.b));
    float underlayLift = min(underlayTarget / max(0.01, underlayLight),
                             0.98 / max(0.01, underlayMaximum));
    vec3 coverUnderlay = underlaySource * underlayLift * 0.70 * underlayMix;
    float glyphRadius = min(3.05, 1.90 + visualReaction
                          * (0.52 * bassImpact + 0.70 * kickAccent
                             + 0.09 * midImpact));
    float glyphMask = 1.0 - smoothstep(glyphRadius - 0.48, glyphRadius + 0.48,
                                      glyphDistance);
    if (glyphMask <= 0.01) {
        color = vec4(coverUnderlay * visibility, 1.0); return;
    }

    vec2 dotOrigin = cell * cellSize + vec2(float(dx) * 6.0, float(dy) * 6.0);
    vec3 brightest = vec3(0.0);
    vec3 coverColorSum = vec3(0.0);
    float coverLightSum = 0.0;
    float best = 0.0;
    for (int oy = 0; oy < 6; ++oy) {
        for (int ox = 0; ox < 6; ++ox) {
            vec2 sampleUv = (dotOrigin + vec2(float(ox) + 0.5, float(oy) + 0.5)) / resolution;
            // Keep the viewpoint stable. Bass affects glyph weight and light,
            // never the camera, so the eye can continue following the subject.
            vec3 candidate = sceneSample(sampleUv);
            float level = luminance(candidate);
            if (level > best) { best = level; brightest = candidate; }
            if (coverMix > 0.0) {
                coverColorSum += candidate;
                coverLightSum += level;
            }
        }
    }

    vec3 sourceColor = brightest;
    float coverLevel = best;
    if (coverMix > 0.0) {
        // A cell average retains the artwork's real RGB balance and local
        // shading. Brightest-sample pooling turned photographs into flat white
        // masks whenever a small neutral highlight crossed a cell.
        vec3 coverAverage = coverColorSum / 36.0;
        float coverAverageLight = coverLightSum / 36.0;
        float targetLight = pow(clamp(coverAverageLight, 0.0, 1.0), 0.82);
        float coverMaximum = max(coverAverage.r,
                                 max(coverAverage.g, coverAverage.b));
        float lift = min(targetLight / max(0.01, coverAverageLight),
                         0.98 / max(0.01, coverMaximum));
        sourceColor = mix(brightest, coverAverage * lift, coverColorLock);
        // Keep one dot in reserve even for pure white. The ordered braille
        // texture remains visible instead of becoming a solid white field.
        coverLevel = min(0.92, targetLight * 0.96);
    }

    const float threshold[8] = float[8](0.08, 0.58, 0.33, 0.83, 0.70, 0.20, 0.95, 0.45);
    float materialExposure = mix(asciiExposure, 1.0, coverMix);
    float presetLevel = min(1.0, sqrt(max(best, 0.0)) * 1.25
                                 * sqrt(materialExposure));
    presetLevel = floor(presetLevel * 5.0 + 0.5) / 5.0;
    float level = mix(presetLevel, coverLevel, coverColorLock);
    // Bass briefly reveals more of the preset's own dim structure. This makes
    // the active subject feel heavier without adding a ring or moving the
    // camera independently of the composition.
    // Treble exposes the smallest source details while mids enrich the color
    // already present in the preset. Each band changes a different material
    // property, so the result reads as music rather than one global pulse.
    float activeThreshold = threshold[dy * 2 + dx] - visualReaction
                          * (0.055 * bassImpact + 0.060 * kickAccent
                             + 0.025 * trebleLevel + 0.045 * trebleImpact);
    if (level < activeThreshold) {
        color = vec4(coverUnderlay * visibility, 1.0); return;
    }
    vec3 energized = mix(sourceColor, sourceColor * sourceColor * 1.12,
                         0.16 * bassImpact * (1.0 - coverColorLock));
    float grey = luminance(energized);
    energized = mix(vec3(grey), energized,
                    1.0 + (0.08 * midLevel + 0.12 * midImpact)
                        * (1.0 - coverColorLock));
    // The source preset owns luminance. Audio changes glyph weight, detail,
    // and chroma, but must never globally flash the frame toward white.
    float visualLightResponse = 0.80 + level * 0.32 + 0.035 * bassLevel
                              + 0.025 * trebleLevel;
    float outputScale = mix(visualLightResponse, 1.0, coverColorLock);
    vec3 dotColor = energized * outputScale * materialExposure;
    float dotOpacity = mix(1.0, 0.78, coverColorLock);
    vec3 asciiMaterial = mix(coverUnderlay, dotColor,
                             glyphMask * dotOpacity);
    color = vec4(asciiMaterial * visibility, 1.0);
}
)GLSL";

GLuint compileShader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    GLint ok = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        std::array<char, 4096> log{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(log.size()), nullptr, log.data());
        std::cerr << log.data() << "\n";
        std::exit(1);
    }
    return shader;
}

std::string commandOutput(const char* command) {
    FILE* pipe = popen(command, "r");
    if (!pipe) return {};
    std::array<char, 512> buffer{};
    std::string result;
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe)) result += buffer.data();
    pclose(pipe);
    while (!result.empty() && (result.back() == '\n' || result.back() == '\r')) result.pop_back();
    return result;
}

bool loadPresetAtVisualTempo(projectm_handle projectm, const std::string& filename,
                             bool smoothTransition) {
    std::ifstream input(filename, std::ios::binary);
    if (!input) return false;
    std::ostringstream contents;
    contents << input.rdbuf();
    std::string preset = contents.str();

    // projectM intentionally has no global time-scale control. Transforming
    // the preset program slows time-based EEL and shader motion while its
    // bass/mid/treble inputs continue to receive live audio at full speed.
    applyOmadropAdapter(filename, preset);
    const std::string scale = std::to_string(visualTempo);
    preset = std::regex_replace(preset, std::regex(R"(\btime\b)"), "(time*" + scale + ")");
    preset = std::regex_replace(preset, std::regex(R"(\bframe\b)"), "(frame*" + scale + ")");
    float sensitivity = 1.30f;
    if (filename.find("Halls Of Centrifuge") != std::string::npos) sensitivity = 1.36f;
    else if (filename.find("Songflower") != std::string::npos) sensitivity = 1.34f;
    projectm_set_beat_sensitivity(projectm, sensitivity);
    projectm_load_preset_data(projectm, preset.c_str(), smoothTransition);
    return true;
}

bool loadPngTexture(const std::string& filename, GLuint texture, float& aspect) {
    png_image image{};
    image.version = PNG_IMAGE_VERSION;
    if (!png_image_begin_read_from_file(&image, filename.c_str())) return false;
    image.format = PNG_FORMAT_RGBA;
    std::vector<unsigned char> pixels(PNG_IMAGE_SIZE(image));
    if (!png_image_finish_read(&image, nullptr, pixels.data(), 0, nullptr)) {
        png_image_free(&image);
        return false;
    }
    aspect = static_cast<float>(image.width) / static_cast<float>(image.height);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, static_cast<GLsizei>(image.width),
                 static_cast<GLsizei>(image.height), 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    png_image_free(&image);
    return true;
}

std::array<float, 3> loadPaletteColor(const std::string& filename) {
    std::ifstream input(filename + ".pal");
    std::string hex;
    std::array<float, 3> best{0.72f, 0.82f, 1.0f};
    float bestScore = -1.0f;
    while (input >> hex) {
        if (hex.size() != 7 || hex[0] != '#') continue;
        try {
            const int value = std::stoi(hex.substr(1), nullptr, 16);
            std::array<float, 3> color{
                ((value >> 16) & 255) / 255.0f,
                ((value >> 8) & 255) / 255.0f,
                (value & 255) / 255.0f};
            const auto [minimum, maximum] = std::minmax_element(color.begin(), color.end());
            const float light = (color[0] + color[1] + color[2]) / 3.0f;
            const float score = (*maximum - *minimum) * 1.7f + light * 0.35f;
            if (score > bestScore && light > 0.12f) {
                best = color;
                bestScore = score;
            }
        } catch (...) {}
    }
    return best;
}

std::filesystem::path configDirectory() {
    if (const char* configHome = std::getenv("XDG_CONFIG_HOME")) {
        return std::filesystem::path(configHome) / "omadrop";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".config" / "omadrop";
    }
    return {};
}

std::filesystem::path syncSettingsPath(const std::string& sink) {
    std::string filename;
    for (const unsigned char character : sink) {
        filename += std::isalnum(character) || character == '-' || character == '_'
            || character == '.' ? static_cast<char>(character) : '_';
    }
    if (filename.empty()) filename = "default";
    if (filename.size() > 180) filename.resize(180);
    const auto directory = configDirectory();
    return directory.empty() ? directory : directory / "sync-by-sink" / (filename + ".ms");
}

std::filesystem::path legacySyncSettingsPath() {
    const auto directory = configDirectory();
    return directory.empty() ? directory : directory / "sync-ms";
}

std::filesystem::path asciiSettingsPath() {
    const auto directory = configDirectory();
    return directory.empty() ? directory : directory / "ascii-enabled";
}

bool loadAsciiEnabled() {
    std::ifstream input(asciiSettingsPath());
    int enabled = 1;
    if (input >> enabled) return enabled != 0;
    return true;
}

void saveAsciiEnabled(bool enabled) {
    const auto path = asciiSettingsPath();
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return;
    std::ofstream output(path, std::ios::trunc);
    if (output) output << (enabled ? 1 : 0) << "\n";
}

void saveSyncDelay(unsigned int milliseconds, const std::string& sink) {
    const auto path = syncSettingsPath(sink);
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return;
    std::ofstream output(path);
    if (output) output << milliseconds << "\n";
}

unsigned int loadSyncDelay(const std::string& sink) {
    if (const char* configuredDelay = std::getenv("OMADROP_SYNC_MS")) {
        return static_cast<unsigned int>(
            std::clamp(std::atoi(configuredDelay), 0, 500));
    }
    unsigned int milliseconds = sink.rfind("bluez_", 0) == 0 ? 180u : 35u;
    const auto sinkSettings = syncSettingsPath(sink);
    std::ifstream savedDelay(sinkSettings);
    bool migrateLegacyDelay = false;
    if (!savedDelay && !std::filesystem::exists(sinkSettings.parent_path())) {
        savedDelay.clear();
        savedDelay.open(legacySyncSettingsPath());
        migrateLegacyDelay = static_cast<bool>(savedDelay);
    }
    int savedMilliseconds = 0;
    if (savedDelay >> savedMilliseconds) {
        milliseconds = static_cast<unsigned int>(
            std::clamp(savedMilliseconds, 0, 500));
        if (migrateLegacyDelay) saveSyncDelay(milliseconds, sink);
    }
    return milliseconds;
}

std::string defaultSinkName() {
    if (const char* testPath = std::getenv("OMADROP_TEST_SINK_PATH")) {
        std::ifstream input(testPath);
        std::string sink;
        if (std::getline(input, sink)) return sink;
    }
    return commandOutput("pactl get-default-sink");
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: projectm-ascii-live PRESET.milk [PRESET.milk ...]\n";
        return 2;
    }
    const std::filesystem::path projectRoot
        = std::filesystem::canonical(argv[0]).parent_path() / ".." / "..";
    const std::string rendererName = std::getenv("OMADROP_ENGINE")
        ? std::getenv("OMADROP_ENGINE") : "native";
    if (rendererName != "native" && rendererName != "projectm") {
        std::cerr << "renderer: unknown OMADROP_ENGINE value '"
                  << rendererName << "'\n";
        return 2;
    }
    const bool nativeEnabled = rendererName == "native";
    bool asciiEnabled = std::getenv("OMADROP_ASCII")
        ? std::string(std::getenv("OMADROP_ASCII")) != "0"
        : loadAsciiEnabled();
    unsigned int syncDelayMs = 0;
    bool closeRequested = false;
    std::optional<NativeSceneKind> selectedNativeScene;
    std::vector<NativeSceneKind> scriptedScenes;
    float scriptedSceneDwellSeconds = 5.0f;
    float scriptedSceneMaximumSeconds = 7.0f;
    float scriptedTransitionSeconds = 2.0f;
    bool scriptedSequenceOnce = false;
    if (nativeEnabled) {
        if (const char* requestedScene = std::getenv("OMADROP_NATIVE_SCENE")) {
            NativeSceneKind selectedScene;
            if (!nativeSceneFromName(requestedScene, selectedScene)) {
                std::cerr << "native scene: unknown OMADROP_NATIVE_SCENE value '"
                          << requestedScene << "'\n";
                return 2;
            }
            selectedNativeScene = selectedScene;
        }
        if (const char* requestedSequence
            = std::getenv("OMADROP_NATIVE_SEQUENCE")) {
            std::istringstream input(requestedSequence);
            std::string name;
            while (std::getline(input, name, ',')) {
                name.erase(name.begin(), std::find_if(name.begin(), name.end(),
                    [](unsigned char character) { return !std::isspace(character); }));
                name.erase(std::find_if(name.rbegin(), name.rend(),
                    [](unsigned char character) { return !std::isspace(character); }).base(),
                    name.end());
                NativeSceneKind scene;
                if (name.empty() || !nativeSceneFromName(name, scene)) {
                    std::cerr << "native sequence: unknown scene '" << name << "'\n";
                    return 2;
                }
                if (scriptedScenes.empty() || scriptedScenes.back() != scene) {
                    scriptedScenes.push_back(scene);
                }
            }
            if (scriptedScenes.size() < 2) {
                std::cerr << "native sequence: provide at least two distinct scenes\n";
                return 2;
            }
            selectedNativeScene = scriptedScenes.front();
            if (const char* dwell = std::getenv("OMADROP_NATIVE_SEQUENCE_SECONDS")) {
                scriptedSceneDwellSeconds = std::clamp(
                    std::strtof(dwell, nullptr), 1.0f, 30.0f);
            }
            scriptedSceneMaximumSeconds = scriptedSceneDwellSeconds + 2.0f;
            if (const char* maximum
                = std::getenv("OMADROP_NATIVE_SEQUENCE_MAX_SECONDS")) {
                scriptedSceneMaximumSeconds = std::clamp(
                    std::strtof(maximum, nullptr),
                    scriptedSceneDwellSeconds, 32.0f);
            }
            if (const char* duration
                = std::getenv("OMADROP_NATIVE_TRANSITION_SECONDS")) {
                scriptedTransitionSeconds = std::clamp(
                    std::strtof(duration, nullptr), 0.6f, 8.0f);
            }
            scriptedSequenceOnce
                = std::getenv("OMADROP_NATIVE_SEQUENCE_ONCE")
               && std::string(std::getenv("OMADROP_NATIVE_SEQUENCE_ONCE")) != "0";
        }
    }
    std::optional<StructureTimeline> timeline;
    if (const char* timelinePath = std::getenv("OMADROP_TIMELINE_PATH")) {
        StructureTimeline loaded;
        std::string error;
        if (!loaded.load(timelinePath, error)) {
            std::cerr << "timeline: " << error << "\n";
            return 2;
        }
        std::cerr << "timeline: " << timelinePath << " ("
                  << loaded.sections().size() << " sections)\n";
        timeline = std::move(loaded);
    }
    signal(SIGTERM, requestStop);
    signal(SIGINT, requestStop);
    SDL_SetHint(SDL_HINT_SCREENSAVER_INHIBIT_ACTIVITY_NAME, "Visualizing music");
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    if (const char* borderless = std::getenv("OMADROP_BORDERLESS");
        borderless && std::string(borderless) != "0") {
        windowFlags |= SDL_WINDOW_BORDERLESS;
    }
    if (const char* fullscreen = std::getenv("OMADROP_FULLSCREEN");
        fullscreen && std::string(fullscreen) != "0") {
        windowFlags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
    }
    int displayIndex = 0;
    if (const char* requestedDisplay = std::getenv("OMADROP_DISPLAY_INDEX")) {
        displayIndex = std::clamp(std::atoi(requestedDisplay), 0,
                                  std::max(0, SDL_GetNumVideoDisplays() - 1));
    }
    const int windowPosition = SDL_WINDOWPOS_CENTERED_DISPLAY(displayIndex);
    SDL_Window* window = SDL_CreateWindow("Omadrop",
        windowPosition, windowPosition, width, height,
        windowFlags);
    if (!window) {
        std::cerr << "window creation failed: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::cerr << "OpenGL context failed: " << SDL_GetError() << "\n";
        return 1;
    }
    SDL_GL_SetSwapInterval(1);
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return 1;

    std::array<GLuint, 2> frameTextures{};
    glGenTextures(2, frameTextures.data());
    for (GLuint texture : frameTextures) {
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }
    int sourceWidth = width;
    int sourceHeight = height;
    GLuint coverTexture = 0;
    glGenTextures(1, &coverTexture);
    glBindTexture(GL_TEXTURE_2D, coverTexture);
    const std::array<unsigned char, 4> blackPixel{0, 0, 0, 255};
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, blackPixel.data());
    GLuint vertex = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragment = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex);
    glAttachShader(program, fragment);
    glLinkProgram(program);
    GLuint vao = 0;
    glGenVertexArrays(1, &vao);
    std::unique_ptr<NativeRenderer> nativeRenderer;
    if (nativeEnabled) {
        nativeRenderer = std::make_unique<NativeRenderer>();
        std::string error;
        if (!nativeRenderer->initialize(projectRoot / "shaders" / "native", error)) {
            std::cerr << "native renderer: " << error << "\n";
            return 1;
        }
        std::cerr << "renderer: Omadrop native scenes\n";
    }

    const char* texturePaths[] = {"/usr/share/projectM/textures", "/usr/share/projectM/presets"};
    std::array<projectm_handle, 2> engines{projectm_create(), projectm_create()};
    if (!engines[0] || !engines[1]) return 1;
    for (projectm_handle engine : engines) {
        projectm_set_window_size(engine, width, height);
        projectm_set_mesh_size(engine, 48, 36);
        projectm_set_fps(engine, 60);
        projectm_set_preset_locked(engine, true);
        projectm_set_hard_cut_enabled(engine, false);
        projectm_set_texture_search_paths(engine, texturePaths, 2);
    }
    std::vector<std::string> presets(argv + 1, argv + argc);
    for (const auto& preset : presets) {
        if (!findProfileForPreset(preset)) {
            std::cerr << "unprofiled preset: " << presetBasename(preset) << "\n";
            return 2;
        }
    }
    const unsigned int randomSeed = std::getenv("OMADROP_RANDOM_SEED")
        ? static_cast<unsigned int>(std::strtoul(std::getenv("OMADROP_RANDOM_SEED"), nullptr, 10))
        : std::random_device{}();
    std::mt19937 randomEngine(randomSeed);
    const std::filesystem::path pairedStatePath = std::getenv("OMADROP_PAIR_STATE")
        ? std::getenv("OMADROP_PAIR_STATE") : "";
    const std::string pairedRole = std::getenv("OMADROP_PAIR_ROLE")
        ? std::getenv("OMADROP_PAIR_ROLE") : "";
    const bool pairedLeader = !pairedStatePath.empty() && pairedRole == "leader";
    const bool pairedFollower = !pairedStatePath.empty() && pairedRole == "follower";
    bool fullscreenEnabled
        = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0
       || pairedLeader || pairedFollower;
    const std::filesystem::path pairedRequestPath = pairedStatePath.empty()
        ? std::filesystem::path{}
        : std::filesystem::path(pairedStatePath.string() + ".request");
    const std::filesystem::path pairedMusicPath = pairedStatePath.empty()
        ? std::filesystem::path{}
        : std::filesystem::path(pairedStatePath.string() + ".music");
    std::uint64_t pairedSerial = 0;
    std::uint64_t pairedMusicSerial = 0;
    PairedDisplayFollower pairedDisplayFollower;
    PairedMusicFollower pairedMusicFollower;
    auto publishPairedState = [&](std::size_t index, std::uint64_t duration,
                                  int mode, bool hardSync, int nativeScene = -1,
                                  int nativeSourceScene = -1) {
        if (!pairedLeader) return;
        const std::filesystem::path temporary = pairedStatePath.string()
            + "." + std::to_string(getpid()) + ".tmp";
        std::ofstream output(temporary, std::ios::trunc);
        output << encodePairedDisplayState({
            .serial = ++pairedSerial,
            .presetIndex = index,
            .durationMs = duration,
            .transitionMode = mode,
            .hardSync = hardSync,
            .nativeScene = nativeScene,
            .nativeSourceScene = nativeSourceScene,
            .asciiMode = asciiEnabled ? 1 : 0,
            .fullscreenMode = fullscreenEnabled ? 1 : 0,
            .syncDelayMs = static_cast<int>(syncDelayMs),
            .closeMode = closeRequested ? 1 : 0,
        });
        output.close();
        std::error_code error;
        std::filesystem::rename(temporary, pairedStatePath, error);
        if (error) std::filesystem::remove(temporary, error);
    };
    auto publishPairedMusic = [&](const MusicFrame& frame) {
        if (!pairedLeader) return;
        const PairedMusicState state{
            .serial = ++pairedMusicSerial,
            .frame = frame,
        };
        const std::string encoded = encodePairedMusicState(state);
        const std::filesystem::path temporary = pairedMusicPath.string()
            + "." + std::to_string(getpid()) + ".tmp";
        std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
        output.write(encoded.data(), static_cast<std::streamsize>(encoded.size()));
        output.close();
        std::error_code error;
        std::filesystem::rename(temporary, pairedMusicPath, error);
        if (error) std::filesystem::remove(temporary, error);
    };
    std::uniform_int_distribution<std::size_t> openingPreset(0, presets.size() - 1);
    std::size_t presetIndex = std::getenv("OMADROP_START_PRESET")
        ? static_cast<std::size_t>(std::max(0, std::atoi(std::getenv("OMADROP_START_PRESET")))) % presets.size()
        : openingPreset(randomEngine);
    std::deque<std::size_t> recentPresets{presetIndex};
    auto chooseAutomaticPreset = [&](PresetEnergy targetEnergy, int preferredFamily = -1) {
        return choosePresetVariant(presets, presetIndex, recentPresets, targetEnergy,
                                   preferredFamily, randomEngine);
    };
    auto energyForTrack = [](float energy) {
        if (energy < 0.90f) return PresetEnergy::Calm;
        if (energy < 1.25f) return PresetEnergy::Medium;
        return PresetEnergy::Driving;
    };
    int activeEngine = 0;
    bool presetTransitionActive = false;
    uint64_t presetTransitionStartedAt = 0;
    uint64_t presetTransitionDuration = 6500;
    int transitionMode = 3;
    std::size_t transitionOutgoingPresetIndex = presetIndex;
    TimelineDirector timelineDirector;
    VisualMotifMemory visualMotifs;
    std::optional<std::size_t> pendingTimelinePreset;
    if (!loadPresetAtVisualTempo(engines[activeEngine], presets[presetIndex], false)) return 1;
    std::cerr << "preset: " << presets[presetIndex] << "\n";
    publishPairedState(presetIndex, 0, 0, true);
    if (pairedLeader || pairedFollower) {
        std::cerr << "paired display: " << pairedRole << "\n";
    }
    uint64_t transitionWindowAt = SDL_GetTicks64() + 9000;
    uint64_t transitionDeadlineAt = SDL_GetTicks64() + 13000;

    std::string sink = defaultSinkName();
    syncDelayMs = loadSyncDelay(sink);
    std::deque<float> delayedPcm;
    std::cerr << "audio sync delay: " << syncDelayMs << " ms\n";
    publishPairedState(presetIndex, 0, 0, true);
    pid_t audioPid = -1;
    int audioFd = -1;
    auto stopAudioCapture = [&]() {
        if (audioPid > 0) {
            kill(audioPid, SIGTERM);
            waitpid(audioPid, nullptr, 0);
            audioPid = -1;
        }
        if (audioFd >= 0) {
            close(audioFd);
            audioFd = -1;
        }
    };
    auto startAudioCapture = [&](const std::string& targetSink) {
        int audioPipe[2];
        if (pipe2(audioPipe, O_CLOEXEC) != 0) return false;
        const pid_t child = fork();
        if (child == 0) {
            dup2(audioPipe[1], STDOUT_FILENO);
            const int nullFd = open("/dev/null", O_WRONLY);
            if (nullFd >= 0) dup2(nullFd, STDERR_FILENO);
            close(audioPipe[0]);
            close(audioPipe[1]);
            if (targetSink.empty()) {
                execlp("pw-record", "pw-record", "--raw", "--rate", "44100",
                       "--channels", "2", "--format", "f32", "--latency", "20ms",
                       "-P", "{ stream.capture.sink=true }", "-",
                       static_cast<char*>(nullptr));
            } else {
                execlp("pw-record", "pw-record", "--raw", "--rate", "44100",
                       "--channels", "2", "--format", "f32", "--latency", "20ms",
                       "-P", "{ stream.capture.sink=true }", "--target",
                       targetSink.c_str(), "-", static_cast<char*>(nullptr));
            }
            _exit(127);
        }
        close(audioPipe[1]);
        if (child < 0) {
            close(audioPipe[0]);
            return false;
        }
        audioPid = child;
        audioFd = audioPipe[0];
        fcntl(audioFd, F_SETFL, fcntl(audioFd, F_GETFL) | O_NONBLOCK);
        return true;
    };
    if (!startAudioCapture(sink)) return 1;
    std::vector<float> pcm(4096 * 2);
    std::vector<float> visualPcm(4096 * 2);
    const unsigned int projectmSampleLimit = projectm_pcm_get_max_samples();
    std::vector<float> projectmPcm(projectmSampleLimit * 2);
    double visualBassPhase = 0.0;
    double visualMidPhase = 0.0;
    double visualTreblePhase = 0.0;
    double fallbackPhase = 0.0;
    const bool syntheticAudio = std::getenv("OMADROP_SYNTHETIC_AUDIO") != nullptr;
    bool lastFallback = false;
    bool reportedAudioMode = false;
    bool reportedPairedMusic = false;
    uint64_t discardedAudioFrames = 0;
    uint64_t lastNonSilentAudioAt = SDL_GetTicks64();
    uint64_t previousFrameAt = SDL_GetTicks64();
    AudioFeatureBus featureBus;
    AudioFeatures audioFeatures;
    MusicalStructureTracker structureTracker;
    MusicFrameBuilder musicFrameBuilder;
    MusicFrame musicFrame;
    NativeSceneDirector nativeSceneDirector;
    NativeSceneState nativeSceneState;
    bool nativeTransitionWasActive = false;
    NativeSceneKind initialNativeScene = NativeSceneKind::DepthTunnel;
    if (nativeEnabled) {
        if (selectedNativeScene) {
            initialNativeScene = *selectedNativeScene;
        } else {
            std::uniform_int_distribution<int> openingNativeScene(
                0, static_cast<int>(nativeSceneCount) - 1);
            initialNativeScene = static_cast<NativeSceneKind>(
                openingNativeScene(randomEngine));
        }
        nativeSceneDirector.selectScene(initialNativeScene);
        if (!scriptedScenes.empty()) {
            nativeSceneDirector.setTransitionDuration(scriptedTransitionSeconds);
        }
        std::cerr << "native scene: " << nativeSceneName(initialNativeScene)
                  << (!scriptedScenes.empty() ? " (scripted opening)"
                      : selectedNativeScene ? " (selected)"
                      : " (random opening)")
                  << "\n";
        publishPairedState(presetIndex, 0, 6, true,
                           static_cast<int>(initialNativeScene),
                           static_cast<int>(initialNativeScene));
    }
    std::size_t scriptedSceneIndex = 0;
    uint64_t scriptedSceneSettledAt = 0;
    float scriptedPreviousBarPhase = 0.0f;
    bool structureClockLocked = false;
    float bassImpact = 0.0f;
    float midImpact = 0.0f;
    float trebleImpact = 0.0f;
    float musicalEnergy = 1.0f;
    const bool debugAudio = std::getenv("OMADROP_DEBUG_AUDIO") != nullptr;
    const float reactionScale = std::getenv("OMADROP_REACTION_SCALE")
        ? std::clamp(std::strtof(std::getenv("OMADROP_REACTION_SCALE"), nullptr), 0.0f, 3.0f)
        : 1.0f;
    uint64_t lastClockLogAt = 0;
    float coverAspect = 1.0f;
    std::array<float, 3> albumColor{0.72f, 0.82f, 1.0f};
    bool hasCover = false;
    std::string currentArtPath;
    uint64_t coverStartedAt = 0;
    const float coverHoldSeconds = std::getenv("OMADROP_COVER_HOLD_SECONDS")
        ? std::clamp(std::strtof(
            std::getenv("OMADROP_COVER_HOLD_SECONDS"), nullptr), 0.0f, 20.0f)
        : 5.0f;
    const float coverDissolveSeconds = std::getenv("OMADROP_COVER_DISSOLVE_SECONDS")
        ? std::clamp(std::strtof(
            std::getenv("OMADROP_COVER_DISSOLVE_SECONDS"), nullptr), 0.5f, 20.0f)
        : 5.0f;
    uint64_t nextMprisPollAt = 0;
    const std::filesystem::path mprisHelper = projectRoot / "bin" / "mpris-state";
    pid_t mprisHelperPid = -1;
    int mprisHelperFd = -1;
    std::string mprisHelperOutput;
    uint64_t mprisPollStartedAt = 0;
    PlaybackClock playbackClock;
    bool timelineMismatchReported = false;
    uint64_t timelineClockStartedAt = SDL_GetTicks64();
    double forcedTimelinePosition = -1.0;
    if (const char* position = std::getenv("OMADROP_TIMELINE_POSITION")) {
        char* end = nullptr;
        const double parsed = std::strtod(position, &end);
        if (end != position && *end == '\0' && std::isfinite(parsed) && parsed >= 0.0) {
            forcedTimelinePosition = parsed;
        } else {
            std::cerr << "timeline: OMADROP_TIMELINE_POSITION must be a non-negative number\n";
            return 2;
        }
    }

    auto startMprisPoll = [&](bool skipArt) {
        int pipeFds[2];
        if (pipe2(pipeFds, O_CLOEXEC | O_NONBLOCK) != 0) return;
        const pid_t pid = fork();
        if (pid == 0) {
            dup2(pipeFds[1], STDOUT_FILENO);
            const int nullFd = open("/dev/null", O_WRONLY);
            if (nullFd >= 0) dup2(nullFd, STDERR_FILENO);
            close(pipeFds[0]);
            close(pipeFds[1]);
            if (skipArt) {
                execl(mprisHelper.c_str(), mprisHelper.c_str(), "--no-art",
                      static_cast<char*>(nullptr));
            } else {
                execl(mprisHelper.c_str(), mprisHelper.c_str(), static_cast<char*>(nullptr));
            }
            _exit(127);
        }
        close(pipeFds[1]);
        if (pid < 0) {
            close(pipeFds[0]);
            return;
        }
        mprisHelperPid = pid;
        mprisHelperFd = pipeFds[0];
        mprisHelperOutput.clear();
        mprisPollStartedAt = SDL_GetTicks64();
    };

    bool running = true;
    bool windowShown = false;
    const std::filesystem::path startGatePath = std::getenv("OMADROP_START_GATE")
        ? std::getenv("OMADROP_START_GATE") : "";
    const std::filesystem::path startReadyPath = startGatePath.empty()
        ? std::filesystem::path{}
        : std::filesystem::path(startGatePath.string() + "."
            + std::to_string(displayIndex) + ".ready");
    const std::filesystem::path recordStopPath
        = std::getenv("OMADROP_RECORD_STOP_FILE")
        ? std::getenv("OMADROP_RECORD_STOP_FILE") : "";
    const uint64_t closeDurationMs = recordStopPath.empty() ? 420u : 920u;
    bool recordStopSignaled = false;
    const auto signalRecordingMarker = [](const std::filesystem::path& path) {
        if (path.empty()) return;
        std::ofstream marker(path, std::ios::trunc);
        if (marker) marker << getpid() << '\n';
    };
    bool startGateOpened = startGatePath.empty();
    uint64_t revealStartedAt = 0;
    bool closing = false;
    uint64_t closeStartedAt = 0;
    const std::string forcedCoverPath = std::getenv("OMADROP_COVER_PATH")
        ? std::getenv("OMADROP_COVER_PATH") : "";
    const bool disableArt = std::getenv("OMADROP_DISABLE_ART") != nullptr
                         || !forcedCoverPath.empty();
    bool artLookupComplete = disableArt;
    const uint64_t initialArtDeadlineAt = SDL_GetTicks64() + 4500;
    bool suppressLateInitialCover = false;
    if (!forcedCoverPath.empty()
        && loadPngTexture(forcedCoverPath, coverTexture, coverAspect)) {
        currentArtPath = forcedCoverPath;
        albumColor = loadPaletteColor(forcedCoverPath);
        hasCover = true;
        coverStartedAt = SDL_GetTicks64();
        transitionWindowAt = coverStartedAt + 19000;
        transitionDeadlineAt = coverStartedAt + 23000;
        std::cerr << "cover: " << forcedCoverPath << " (forced)\n";
    }
    const uint64_t automaticQuitAt = std::getenv("OMADROP_AUTO_QUIT_MS")
        ? SDL_GetTicks64() + static_cast<uint64_t>(std::max(0, std::atoi(std::getenv("OMADROP_AUTO_QUIT_MS"))))
        : 0;
    uint64_t automaticNextAt = std::getenv("OMADROP_AUTO_NEXT_MS")
        ? SDL_GetTicks64() + static_cast<uint64_t>(std::max(0, std::atoi(std::getenv("OMADROP_AUTO_NEXT_MS"))))
        : 0;
    uint64_t nextSinkPollAt = SDL_GetTicks64() + 2000;
    using FrameClock = std::chrono::steady_clock;
    constexpr auto frameInterval = std::chrono::nanoseconds(1000000000 / 60);
    auto nextFrame = FrameClock::now() + frameInterval;
    while (running) {
        const uint64_t now = SDL_GetTicks64();
        if (stopRequested) closeRequested = true;
        bool skipPreset = false;
        bool previousPreset = false;
        bool bassHitThisFrame = false;
        bool phraseBoundaryThisFrame = false;
        bool sectionBoundaryThisFrame = false;
        float structureNovelty = 0.0f;
        int structureMotifIdentity = -1;
        bool structureMotifRecalled = false;
        bool pairedControlsChanged = false;
        std::string pairedControlRequest;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_KEYDOWN && event.key.repeat != 0) continue;
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) {
                if (pairedFollower) pairedControlRequest = "quit";
                else {
                    closeRequested = true;
                    pairedControlsChanged = true;
                }
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_n) skipPreset = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p) previousPreset = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_a) {
                if (pairedFollower) pairedControlRequest = "ascii";
                else {
                    asciiEnabled = !asciiEnabled;
                    saveAsciiEnabled(asciiEnabled);
                    pairedControlsChanged = true;
                    std::cerr << "display: "
                              << (asciiEnabled ? "Omadrop ASCII" : "continuous")
                              << "\n";
                }
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                if (pairedFollower) pairedControlRequest = "fullscreen";
                else {
                    fullscreenEnabled = !fullscreenEnabled;
                    SDL_SetWindowFullscreen(window, fullscreenEnabled
                        ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    pairedControlsChanged = true;
                }
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFTBRACKET) {
                if (pairedFollower) pairedControlRequest = "delay-down";
                else {
                    syncDelayMs = syncDelayMs >= 10 ? syncDelayMs - 10 : 0;
                    saveSyncDelay(syncDelayMs, sink);
                    pairedControlsChanged = true;
                    std::cerr << "audio sync delay: " << syncDelayMs << " ms\n";
                }
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHTBRACKET) {
                if (pairedFollower) pairedControlRequest = "delay-up";
                else {
                    syncDelayMs = std::min(500u, syncDelayMs + 10);
                    saveSyncDelay(syncDelayMs, sink);
                    pairedControlsChanged = true;
                    std::cerr << "audio sync delay: " << syncDelayMs << " ms\n";
                }
            }
        }
        if (pairedFollower
            && (skipPreset || previousPreset || !pairedControlRequest.empty())) {
            const std::filesystem::path temporary = pairedRequestPath.string()
                + "." + std::to_string(getpid()) + ".tmp";
            std::ofstream output(temporary, std::ios::trunc);
            const std::string request = previousPreset ? "previous"
                : skipPreset ? "next" : pairedControlRequest;
            output << request << '\n';
            output.close();
            std::error_code error;
            std::filesystem::rename(temporary, pairedRequestPath, error);
            if (error) std::filesystem::remove(temporary, error);
            skipPreset = false;
            previousPreset = false;
        }
        if (pairedLeader) {
            std::ifstream requestInput(pairedRequestPath);
            std::string request;
            if (requestInput >> request) {
                skipPreset = request == "next";
                previousPreset = request == "previous";
                if (request == "ascii") {
                    asciiEnabled = !asciiEnabled;
                    saveAsciiEnabled(asciiEnabled);
                    pairedControlsChanged = true;
                    std::cerr << "display: "
                              << (asciiEnabled ? "Omadrop ASCII" : "continuous")
                              << "\n";
                } else if (request == "fullscreen") {
                    fullscreenEnabled = !fullscreenEnabled;
                    SDL_SetWindowFullscreen(window, fullscreenEnabled
                        ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    pairedControlsChanged = true;
                } else if (request == "delay-down") {
                    syncDelayMs = syncDelayMs >= 10 ? syncDelayMs - 10 : 0;
                    saveSyncDelay(syncDelayMs, sink);
                    pairedControlsChanged = true;
                    std::cerr << "audio sync delay: " << syncDelayMs << " ms\n";
                } else if (request == "delay-up") {
                    syncDelayMs = std::min(500u, syncDelayMs + 10);
                    saveSyncDelay(syncDelayMs, sink);
                    pairedControlsChanged = true;
                    std::cerr << "audio sync delay: " << syncDelayMs << " ms\n";
                } else if (request == "quit") {
                    closeRequested = true;
                    pairedControlsChanged = true;
                }
                std::error_code error;
                std::filesystem::remove(pairedRequestPath, error);
            }
        }
        if (pairedLeader && pairedControlsChanged) {
            publishPairedState(presetIndex, 0, nativeEnabled ? 6 : transitionMode,
                               false);
        }
        if (closeRequested && !closing) {
            closing = true;
            closeStartedAt = now;
        }
        if (closing && now - closeStartedAt >= closeDurationMs) {
            running = false;
            continue;
        }
        if (!startGateOpened && std::filesystem::exists(startGatePath)) {
            startGateOpened = true;
            revealStartedAt = now;
            if (hasCover) coverStartedAt = now;
        }
        if (!artLookupComplete && now >= initialArtDeadlineAt) {
            artLookupComplete = true;
            suppressLateInitialCover = true;
            std::cerr << "cover: startup artwork unavailable; continuing cleanly"
                      << " without a late cover\n";
        }
        if (now >= nextSinkPollAt) {
            nextSinkPollAt = now + 2000;
            const std::string currentSink = defaultSinkName();
            if (!currentSink.empty() && currentSink != sink) {
                stopAudioCapture();
                sink = currentSink;
                syncDelayMs = loadSyncDelay(sink);
                delayedPcm.clear();
                featureBus.resetClock();
                musicFrameBuilder.reset();
                lastNonSilentAudioAt = now;
                reportedAudioMode = false;
                if (!startAudioCapture(sink)) {
                    std::cerr << "audio: could not follow default sink " << sink << "\n";
                    running = false;
                } else {
                    std::cerr << "audio: followed default sink " << sink
                              << ", sync delay " << syncDelayMs << " ms\n";
                }
            }
        }
        if (automaticQuitAt > 0 && now >= automaticQuitAt) closeRequested = true;
        if (automaticNextAt > 0 && now >= automaticNextAt) {
            skipPreset = true;
            automaticNextAt = 0;
        }
        bool seekedThisFrame = false;
        if (now >= nextMprisPollAt && mprisHelperPid < 0) startMprisPoll(disableArt);
        if (mprisHelperPid > 0) {
            std::array<char, 2048> stateBuffer{};
            ssize_t stateBytes = 0;
            while ((stateBytes = read(mprisHelperFd, stateBuffer.data(), stateBuffer.size())) > 0) {
                mprisHelperOutput.append(stateBuffer.data(), static_cast<std::size_t>(stateBytes));
            }
            int helperStatus = 0;
            if (waitpid(mprisHelperPid, &helperStatus, WNOHANG) == mprisHelperPid) {
                close(mprisHelperFd);
                mprisHelperFd = -1;
                mprisHelperPid = -1;
                nextMprisPollAt = now + (!artLookupComplete ? 250
                                             : timeline ? 500 : 1000);
                while (!mprisHelperOutput.empty()
                       && (mprisHelperOutput.back() == '\n' || mprisHelperOutput.back() == '\r')) {
                    mprisHelperOutput.pop_back();
                }
                if (!mprisHelperOutput.empty()) {
                    std::string error;
                    const auto state = parseMprisState(mprisHelperOutput, error);
                    if (!state) {
                        std::cerr << "mpris: " << error << "\n";
                        if (!artLookupComplete) {
                            artLookupComplete = true;
                            suppressLateInitialCover = true;
                        }
                    } else {
                        const PlaybackObservation observation
                            = playbackClock.observe(*state, now / 1000.0);
                        // A player that appears after Omadrop has already
                        // started is a new presentation, not a late result
                        // from the launch-time artwork lookup. Let that first
                        // track use its cover normally.
                        if (observation.first && suppressLateInitialCover
                            && mprisPollStartedAt >= initialArtDeadlineAt) {
                            suppressLateInitialCover = false;
                        }
                        if (observation.trackChanged) {
                            suppressLateInitialCover = false;
                        }
                        seekedThisFrame = observation.seeked;
                        if (observation.first || observation.trackChanged) {
                            timelineDirector.reset();
                            featureBus.resetClock();
                            structureTracker.reset();
                            musicFrameBuilder.reset();
                            nativeSceneDirector.resetForTrack();
                            visualMotifs.reset();
                            structureClockLocked = false;
                            pendingTimelinePreset.reset();
                            timelineClockStartedAt = now;
                            timelineMismatchReported = false;
                        }
                        if (observation.trackChanged && !pairedFollower) {
                            presetIndex = chooseAutomaticPreset(PresetEnergy::Medium);
                            recentPresets.clear();
                            recentPresets.push_back(presetIndex);
                            presetTransitionActive = false;
                            loadPresetAtVisualTempo(
                                engines[activeEngine], presets[presetIndex], false);
                            transitionWindowAt = now + 19000;
                            transitionDeadlineAt = now + 23000;
                            if (hasCover) coverStartedAt = now;
                            publishPairedState(
                                presetIndex, 0, nativeEnabled ? 6 : 0, true,
                                nativeEnabled
                                    ? static_cast<int>(
                                        nativeSceneDirector.state().currentScene)
                                    : -1,
                                nativeEnabled
                                    ? static_cast<int>(
                                        nativeSceneDirector.state().currentScene)
                                    : -1);
                            std::cerr << "track: " << state->identity << "\n";
                        }
                        if (!suppressLateInitialCover
                            && !state->artPath.empty()
                            && state->artPath != currentArtPath
                            && loadPngTexture(state->artPath, coverTexture, coverAspect)) {
                            currentArtPath = state->artPath;
                            albumColor = loadPaletteColor(state->artPath);
                            hasCover = true;
                            coverStartedAt = now;
                            std::cerr << "cover: " << state->artPath << "\n";
                        }
                        if (!artLookupComplete && hasCover) {
                            artLookupComplete = true;
                        }
                    }
                } else if (!artLookupComplete) {
                    // No MPRIS player is active, so there is no artwork to wait
                    // for. Begin with the native scene and never reverse into a
                    // cover from this launch attempt.
                    artLookupComplete = true;
                    suppressLateInitialCover = true;
                }
            }
        }
        unsigned int capturedFrames = 0;
        float peak = 0.0f;
        ssize_t bytes = 0;
        while ((bytes = read(audioFd, pcm.data(), pcm.size() * sizeof(float))) > 0) {
            std::size_t sampleCount = static_cast<std::size_t>(bytes) / sizeof(float);
            sampleCount -= sampleCount % 2;
            capturedFrames += static_cast<unsigned int>(sampleCount / 2);
            for (std::size_t i = 0; i < sampleCount; ++i) {
                peak = std::max(peak, std::abs(pcm[i]));
                if (!syntheticAudio) delayedPcm.push_back(pcm[i]);
            }
        }
        if (capturedFrames > 0 && peak >= 1e-5f) lastNonSilentAudioAt = now;
        // A nonblocking PipeWire fd normally has empty reads between packets.
        // Only call it silence after a sustained gap, or the analyzer receives
        // alternating real and synthetic audio and visibly loses the music.
        const bool fallback = syntheticAudio || now - lastNonSilentAudioAt >= 250;
        if (!reportedAudioMode || fallback != lastFallback) {
            std::cerr << "audio: "
                      << (fallback ? (syntheticAudio ? "synthetic test" : "silence")
                                   : "PipeWire") << "\n";
            lastFallback = fallback;
            reportedAudioMode = true;
        }
        constexpr unsigned int analysisHopFrames = 44100 / 60;
        constexpr std::size_t analysisHopSamples = analysisHopFrames * 2;
        constexpr std::size_t maximumAnalysisHops = 8;
        unsigned int audioHops = 0;
        if (fallback) {
            audioHops = 1;
            std::fill_n(pcm.begin(), analysisHopSamples, 0.0f);
            if (syntheticAudio) {
                const double seconds = SDL_GetTicks64() / 1000.0;
                constexpr double tau = 6.28318530717958647692;
                const double kickPulse = std::pow(std::max(0.0, std::cos(seconds * tau * 2.0)), 18.0);
                const double snarePulse = std::pow(std::max(0.0, std::cos((seconds - 0.25) * tau * 2.0)), 18.0);
                const double hatPulse = std::pow(std::max(0.0, std::cos((seconds - 0.125) * tau * 4.0)), 24.0);
                for (unsigned int i = 0; i < analysisHopFrames; ++i) {
                    const double t = fallbackPhase + i / 44100.0;
                    const double kick = (0.08 + 0.62 * kickPulse) * std::sin(t * tau * 62.0);
                    const double snare = 0.34 * snarePulse * std::sin(t * tau * 2100.0);
                    const double hat = 0.17 * hatPulse * std::sin(t * tau * 7000.0);
                    pcm[i * 2] = static_cast<float>(kick + snare + hat);
                    pcm[i * 2 + 1] = static_cast<float>(kick * 0.96 + snare * 1.04 + hat * 0.92);
                }
                fallbackPhase += analysisHopFrames / 44100.0;
            }
            delayedPcm.clear();
        } else {
            const std::size_t syncDelaySamples
                = static_cast<std::size_t>(44100 * syncDelayMs / 1000) * 2;
            // Analyze every complete 60 Hz hop that arrived since the last
            // video frame. Only discard audio after an exceptional stall, so
            // normal PipeWire packet bursts do not starve the beat clock.
            const auto prepared = prepareAudioHops(
                delayedPcm, syncDelaySamples, analysisHopSamples, maximumAnalysisHops);
            discardedAudioFrames += prepared.discardedSamples / 2;
            audioHops = static_cast<unsigned int>(prepared.readableSamples / analysisHopSamples);
        }
        for (unsigned int audioHop = 0; audioHop < audioHops; ++audioHop) {
            if (!fallback) {
                for (std::size_t i = 0; i < analysisHopSamples; ++i) {
                    pcm[i] = delayedPcm.front();
                    delayedPcm.pop_front();
                }
            }
            audioFeatures = featureBus.processStereo(pcm.data(), analysisHopFrames);
            const MusicalStructureState& structure = structureTracker.update(audioFeatures);
            musicFrame = musicFrameBuilder.update(
                audioFeatures, structure, 1.0f / 60.0f,
                syncDelayMs / 1000.0f);
            if (nativeEnabled) publishPairedMusic(musicFrame);
            structureClockLocked = structure.clockLocked;
            phraseBoundaryThisFrame = phraseBoundaryThisFrame || structure.phraseCrossed;
            sectionBoundaryThisFrame = sectionBoundaryThisFrame || structure.sectionCrossed;
            if (structure.phraseCrossed || structure.sectionCrossed) {
                structureNovelty = structure.novelty;
                if (structure.sectionCrossed) {
                    structureMotifIdentity = structure.motifIdentity;
                    structureMotifRecalled = structure.motifRecalled;
                }
                if (debugAudio) {
                    std::cerr << "structure bar=" << structure.barIndex
                              << " novelty=" << structure.novelty
                              << " threshold=" << structure.noveltyThreshold
                              << (structure.sectionCrossed ? " section" : " phrase") << "\n";
                }
            }
            bassImpact = std::max(bassImpact, audioFeatures.kickImpact);
            midImpact = std::max(midImpact, audioFeatures.snareImpact);
            trebleImpact = std::max(trebleImpact, audioFeatures.hatImpact);
            bassHitThisFrame = bassHitThisFrame || (!fallback && audioFeatures.kick);
            if (debugAudio && audioFeatures.kick) {
                std::cerr << "kick hit " << audioFeatures.kickImpact << "\n";
            }
            if (debugAudio && audioFeatures.snare) {
                std::cerr << "snare hit " << audioFeatures.snareImpact << "\n";
            }
            if (debugAudio && audioFeatures.hat) {
                std::cerr << "hat hit " << audioFeatures.hatImpact << "\n";
            }
            const float energySample = std::clamp(
                0.34f * (audioFeatures.level[0] + audioFeatures.level[1])
                + 0.20f * audioFeatures.level[3]
                + 0.12f * audioFeatures.level[5], 0.0f, 3.0f);
            musicalEnergy += (energySample - musicalEnergy) * 0.006f;
            if (debugAudio && now - lastClockLogAt >= 2000) {
                std::cerr << "clock bpm=" << audioFeatures.bpm
                          << " confidence=" << audioFeatures.beatConfidence
                          << " beat=" << audioFeatures.beatPhase
                          << " bar=" << audioFeatures.barPhase
                          << " phrase=" << audioFeatures.phrasePhase
                          << " dropped=" << discardedAudioFrames << "\n";
                lastClockLogAt = now;
            }

            // projectM derives bass/mid/treb and waveform motion from the PCM
            // it receives. Give it a visual-only sidechain whose transients
            // have more dynamic range, so each preset's own equations react.
            // This never touches the audio sent to the speakers.
            const float kickAccentPosition = std::clamp(
                (bassImpact - 0.52f) / (0.82f - 0.52f), 0.0f, 1.0f);
            const float kickAccent = kickAccentPosition * kickAccentPosition
                                   * (3.0f - 2.0f * kickAccentPosition);
            const float visualDrive = 1.0f + 0.40f * bassImpact
                                             + 0.45f * kickAccent
                                             + 0.28f * midImpact
                                             + 0.14f * trebleImpact;
            constexpr double tau = 6.28318530717958647692;
            for (unsigned int i = 0; i < analysisHopFrames; ++i) {
                // Short tone bursts put unmistakable energy into the same
                // bands exposed to MilkDrop equations. They are control
                // signals only, not audible output.
                const float bassBurst = (0.28f * bassImpact + 0.28f * kickAccent)
                    * static_cast<float>(std::sin(visualBassPhase));
                const float midBurst = 0.20f * midImpact
                    * static_cast<float>(std::sin(visualMidPhase));
                const float trebleBurst = 0.10f * trebleImpact
                    * static_cast<float>(std::sin(visualTreblePhase));
                visualBassPhase += tau * 62.0 / 44100.0;
                visualMidPhase += tau * 520.0 / 44100.0;
                visualTreblePhase += tau * 4200.0 / 44100.0;
                if (visualBassPhase >= tau) visualBassPhase -= tau;
                if (visualMidPhase >= tau) visualMidPhase -= tau;
                if (visualTreblePhase >= tau) visualTreblePhase -= tau;
                const float control = bassBurst + midBurst + trebleBurst;
                visualPcm[i * 2] = pcm[i * 2] * visualDrive + control;
                visualPcm[i * 2 + 1] = pcm[i * 2 + 1] * visualDrive + control;
            }
            float visualPeak = 0.0f;
            for (std::size_t i = 0; i < analysisHopSamples; ++i) {
                visualPeak = std::max(visualPeak, std::abs(visualPcm[i]));
            }
            const float visualScale = visualPeak > 0.95f ? 0.95f / visualPeak : 1.0f;
            for (std::size_t i = 0; i < analysisHopSamples; ++i) {
                visualPcm[i] *= visualScale;
            }
            // projectM stores at most 480 samples per update. Resample the
            // complete 735-sample video interval instead of letting it discard
            // the final third of every frame.
            const unsigned int forwardedFrames = std::min(analysisHopFrames, projectmSampleLimit);
            const float sourceSpan = static_cast<float>(analysisHopFrames - 1);
            const float targetSpan = static_cast<float>(std::max(1u, forwardedFrames - 1));
            for (unsigned int i = 0; i < forwardedFrames; ++i) {
                const float sourcePosition = sourceSpan * i / targetSpan;
                const unsigned int left = static_cast<unsigned int>(sourcePosition);
                const unsigned int right = std::min(left + 1, analysisHopFrames - 1);
                const float fraction = sourcePosition - left;
                for (unsigned int channel = 0; channel < 2; ++channel) {
                    projectmPcm[i * 2 + channel]
                        = visualPcm[left * 2 + channel] * (1.0f - fraction)
                        + visualPcm[right * 2 + channel] * fraction;
                }
            }
            if (!nativeEnabled) {
                projectm_pcm_add_float(
                    engines[0], projectmPcm.data(), forwardedFrames, PROJECTM_STEREO);
                projectm_pcm_add_float(
                    engines[1], projectmPcm.data(), forwardedFrames, PROJECTM_STEREO);
            }
        }

        if (nativeEnabled && pairedFollower) {
            std::ifstream pairedMusicInput(pairedMusicPath, std::ios::binary);
            const std::string pairedMusic{
                std::istreambuf_iterator<char>(pairedMusicInput),
                std::istreambuf_iterator<char>()};
            if (const auto synchronized = pairedMusicFollower.consume(pairedMusic)) {
                musicFrame = *synchronized;
                if (!reportedPairedMusic) {
                    std::cerr << "paired music: synchronized to leader\n";
                    reportedPairedMusic = true;
                }
            }
        }

        const bool timelineEligible = timeline
            && timeline->appliesTo(playbackClock.identity());
        if (timeline && !timelineEligible && playbackClock.hasState()
            && !timelineMismatchReported) {
            std::cerr << "timeline: track identity does not match "
                      << playbackClock.identity() << "; using live director\n";
            timelineMismatchReported = true;
        }
        if (timelineEligible && !pairedFollower) {
            const double timelinePosition = forcedTimelinePosition >= 0.0
                ? forcedTimelinePosition
                : playbackClock.hasState()
                    ? playbackClock.positionAt(now / 1000.0)
                    : (now - timelineClockStartedAt) / 1000.0;
            const auto decision = timelineDirector.sync(
                *timeline, timelinePosition, presetIndex, presets.size(), seekedThisFrame,
                [&](int preferredFamily) {
                    return chooseAutomaticPreset(energyForTrack(musicalEnergy), preferredFamily);
                },
                [&](std::size_t index) {
                    return visualFamily(profileForPreset(presets[index]));
                });
            if (decision) {
                const TimelineSection& section = timeline->sections()[decision->sectionIndex];
                if (decision->sectionChanged) {
                    std::cerr << "section: " << section.identity;
                    if (!section.label.empty()) std::cerr << " (" << section.label << ")";
                    std::cerr << " at " << timelinePosition << "s\n";
                }
                if (decision->hardSync
                    && (presetTransitionActive || decision->targetPreset != presetIndex)) {
                    presetIndex = decision->targetPreset;
                    presetTransitionActive = false;
                    pendingTimelinePreset.reset();
                    recentPresets.clear();
                    recentPresets.push_back(presetIndex);
                    loadPresetAtVisualTempo(engines[activeEngine], presets[presetIndex], false);
                    transitionWindowAt = UINT64_MAX;
                    transitionDeadlineAt = UINT64_MAX;
                    publishPairedState(presetIndex, 0, 0, true);
                    std::cerr << "preset: " << presets[presetIndex]
                              << " (timeline seek)\n";
                } else if (decision->sectionChanged && !decision->initial) {
                    pendingTimelinePreset = decision->targetPreset;
                }
            }
        }
        if (pendingTimelinePreset.value_or(presets.size()) == presetIndex) {
            pendingTimelinePreset.reset();
        }

        if (pairedFollower) {
            std::ifstream pairedInput(pairedStatePath);
            std::ostringstream pairedText;
            pairedText << pairedInput.rdbuf();
            const auto pairedState = pairedDisplayFollower.consume(
                pairedText.str(), presets.size(), nativeSceneCount);
            if (pairedState) {
                if (pairedState->asciiMode >= 0) {
                    const bool synchronizedAscii = pairedState->asciiMode == 1;
                    if (asciiEnabled != synchronizedAscii) {
                        asciiEnabled = synchronizedAscii;
                        std::cerr << "display: "
                                  << (asciiEnabled ? "Omadrop ASCII" : "continuous")
                                  << " (paired)\n";
                    }
                }
                if (pairedState->fullscreenMode >= 0) {
                    const bool synchronizedFullscreen
                        = pairedState->fullscreenMode == 1;
                    if (fullscreenEnabled != synchronizedFullscreen) {
                        fullscreenEnabled = synchronizedFullscreen;
                        SDL_SetWindowFullscreen(window, fullscreenEnabled
                            ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
                    }
                }
                if (pairedState->syncDelayMs >= 0
                    && syncDelayMs
                       != static_cast<unsigned int>(pairedState->syncDelayMs)) {
                    syncDelayMs = static_cast<unsigned int>(pairedState->syncDelayMs);
                    std::cerr << "audio sync delay: " << syncDelayMs
                              << " ms (paired)\n";
                }
                if (pairedState->closeMode == 1) closeRequested = true;
            }
            if (nativeEnabled && pairedState && pairedState->nativeScene >= 0) {
                const NativeSceneKind target = static_cast<NativeSceneKind>(
                    pairedState->nativeScene);
                if (pairedState->hardSync) nativeSceneDirector.selectScene(target);
                else {
                    if (pairedState->nativeSourceScene >= 0) {
                        const NativeSceneKind source = static_cast<NativeSceneKind>(
                            pairedState->nativeSourceScene);
                        const NativeSceneState& followerState
                            = nativeSceneDirector.state();
                        if (!followerState.transitioning
                            && followerState.currentScene != source) {
                            nativeSceneDirector.selectScene(source);
                        }
                    }
                    nativeSceneDirector.requestScene(target);
                }
            } else if (pairedState && pairedState->hardSync) {
                presetIndex = pairedState->presetIndex;
                presetTransitionActive = false;
                pendingTimelinePreset.reset();
                recentPresets.clear();
                recentPresets.push_back(presetIndex);
                loadPresetAtVisualTempo(engines[activeEngine], presets[presetIndex], false);
                transitionWindowAt = UINT64_MAX;
                transitionDeadlineAt = UINT64_MAX;
                std::cerr << "preset: " << presets[presetIndex]
                          << " (paired hard sync)\n";
            } else if (pairedState && pairedState->presetIndex != presetIndex
                       && !presetTransitionActive) {
                transitionOutgoingPresetIndex = presetIndex;
                presetIndex = pairedState->presetIndex;
                presetTransitionDuration = pairedState->durationMs;
                transitionMode = pairedState->transitionMode;
                recentPresets.push_back(presetIndex);
                const std::size_t historyLimit = presetHistoryLimit(presets.size());
                while (recentPresets.size() > historyLimit) recentPresets.pop_front();
                const int incomingEngine = 1 - activeEngine;
                if (!loadPresetAtVisualTempo(
                        engines[incomingEngine], presets[presetIndex], false)) {
                    std::cerr << "could not load paired preset: "
                              << presets[presetIndex] << "\n";
                }
                presetTransitionActive = true;
                presetTransitionStartedAt = now;
                transitionWindowAt = UINT64_MAX;
                transitionDeadlineAt = UINT64_MAX;
                std::cerr << "preset: " << presets[presetIndex]
                          << " (paired transition)\n";
            }
        }

        // Without an authored timeline, confident audio follows sustained
        // structural changes and phrase fallbacks. Uncertain audio keeps the
        // conservative bass-onset and wall-clock fallback.
        const bool fallbackOnset = !structureClockLocked && bassHitThisFrame;
        const bool confidentStructure = structureClockLocked;
        const bool musicalTransition = !timelineEligible && now >= transitionWindowAt
            && (confidentStructure
                    ? sectionBoundaryThisFrame
                      || (now >= transitionDeadlineAt && phraseBoundaryThisFrame)
                    : fallbackOnset);
        const bool deadlineTransition = !timelineEligible && !confidentStructure
                                     && now >= transitionDeadlineAt;
        const bool timelineTransition = pendingTimelinePreset.has_value();
        if (!nativeEnabled && !pairedFollower && !presetTransitionActive && presets.size() > 1
            && (skipPreset || previousPreset || timelineTransition
                || musicalTransition || deadlineTransition)) {
            const std::size_t outgoingPreset = presetIndex;
            transitionOutgoingPresetIndex = outgoingPreset;
            const PresetEnergy targetEnergy = energyForTrack(musicalEnergy);
            const bool nativeSectionTransition = musicalTransition
                                              && sectionBoundaryThisFrame
                                              && !skipPreset && !previousPreset;
            const int preferredFamily = nativeSectionTransition
                ? visualMotifs.familyFor(structureMotifIdentity).value_or(-1) : -1;
            presetIndex = previousPreset ? (presetIndex + presets.size() - 1) % presets.size()
                : skipPreset ? (presetIndex + 1) % presets.size()
                : timelineTransition ? *pendingTimelinePreset
                : chooseAutomaticPreset(targetEnergy, preferredFamily);
            if (nativeSectionTransition && structureMotifIdentity >= 0) {
                visualMotifs.remember(
                    structureMotifIdentity,
                    visualFamily(profileForPreset(presets[presetIndex])));
            }
            pendingTimelinePreset.reset();
            const PresetProfile& outgoing = profileForPreset(presets[outgoingPreset]);
            const PresetProfile& incoming = profileForPreset(presets[presetIndex]);
            auto musicalDuration = [&](float bars, uint64_t fallbackMs) {
                if (audioFeatures.beatConfidence < 0.35f) return fallbackMs;
                const float milliseconds = bars * 4.0f * 60000.0f
                                         / std::max(60.0f, audioFeatures.bpm);
                return static_cast<uint64_t>(std::clamp(milliseconds, 2500.0f, 7000.0f));
            };
            if (outgoing.family == incoming.family) {
                presetTransitionDuration = musicalDuration(1.0f, 4000);
                transitionMode = 0;
            }
            else if (directionsCompatible(outgoing.direction, incoming.direction)) {
                presetTransitionDuration = musicalDuration(1.5f, 5000);
                transitionMode = 1;
            } else if (outgoing.bridge == incoming.bridge) {
                presetTransitionDuration = musicalDuration(1.5f, 5500);
                transitionMode = 2;
            } else {
                presetTransitionDuration = musicalDuration(2.0f, 6000);
                transitionMode = 3;
            }
            recentPresets.push_back(presetIndex);
            const std::size_t historyLimit = presetHistoryLimit(presets.size());
            while (recentPresets.size() > historyLimit) recentPresets.pop_front();
            const int incomingEngine = 1 - activeEngine;
            if (!loadPresetAtVisualTempo(engines[incomingEngine], presets[presetIndex], false)) {
                std::cerr << "could not load preset: " << presets[presetIndex] << "\n";
            }
            presetTransitionActive = true;
            presetTransitionStartedAt = now;
            transitionWindowAt = UINT64_MAX;
            transitionDeadlineAt = UINT64_MAX;
            publishPairedState(
                presetIndex, presetTransitionDuration, transitionMode, false);
            const char* energyName = targetEnergy == PresetEnergy::Driving ? "driving"
                : targetEnergy == PresetEnergy::Medium ? "medium" : "calm";
            std::cerr << "preset: " << presets[presetIndex]
                      << (previousPreset ? " (previous)" : skipPreset ? " (manual)"
                          : timelineTransition ? " (timeline section)"
                          : musicalTransition ? " (automatic on boundary)"
                          : " (automatic on deadline)")
                      << ", energy: " << energyName << "\n";
            if (musicalTransition && confidentStructure) {
                std::cerr << "structure: "
                          << (sectionBoundaryThisFrame ? "section" : "phrase fallback")
                          << ", novelty=" << structureNovelty << "\n";
                if (nativeSectionTransition) {
                    std::cerr << "motif: " << structureMotifIdentity
                              << (structureMotifRecalled ? " recalled" : " new")
                              << ", family="
                              << visualFamily(profileForPreset(presets[presetIndex]))
                              << "\n";
                }
            }
        }

        if (presetTransitionActive
            && now - presetTransitionStartedAt >= presetTransitionDuration) {
            activeEngine = 1 - activeEngine;
            presetTransitionActive = false;
            if (pairedFollower || timelineEligible) {
                transitionWindowAt = UINT64_MAX;
                transitionDeadlineAt = UINT64_MAX;
            } else if (structureClockLocked) {
                const float barMilliseconds = 4.0f * 60000.0f
                                            / std::max(60.0f, audioFeatures.bpm);
                transitionWindowAt = now + static_cast<uint64_t>(6.0f * barMilliseconds);
                transitionDeadlineAt = now + static_cast<uint64_t>(12.0f * barMilliseconds);
            } else {
                const PresetProfile& profile = profileForPreset(presets[presetIndex]);
                std::uniform_int_distribution<uint64_t> dwellTime(profile.dwellMinMs,
                                                                   profile.dwellMaxMs);
                const uint64_t dwell = dwellTime(randomEngine);
                transitionWindowAt = now + dwell;
                transitionDeadlineAt = transitionWindowAt + 4000;
            }
        }

        const float frameSeconds = std::min(0.1f, (now - previousFrameAt) / 1000.0f);
        previousFrameAt = now;
        if (nativeEnabled) {
            if (skipPreset) nativeSceneDirector.requestNext();
            if (previousPreset) nativeSceneDirector.requestPrevious();
            const bool scriptedLeader = !scriptedScenes.empty() && !pairedFollower;
            const bool coverPresentationComplete = !hasCover
                || (now - coverStartedAt) / 1000.0f
                    >= coverHoldSeconds + coverDissolveSeconds;
            const bool scriptedBarBoundary
                = musicFrame.clockConfidence >= 0.35f
               && scriptedPreviousBarPhase > 0.72f
               && musicFrame.barPhase < 0.28f;
            if (scriptedLeader && windowShown && coverPresentationComplete
                && !nativeSceneDirector.state().transitioning) {
                if (scriptedSceneSettledAt == 0) {
                    scriptedSceneSettledAt = now;
                } else {
                    const float scriptedDwell
                        = (now - scriptedSceneSettledAt) / 1000.0f;
                    const bool scriptedCue = scriptedDwell
                            >= scriptedSceneMaximumSeconds
                        || (scriptedDwell >= scriptedSceneDwellSeconds
                            && scriptedBarBoundary);
                    if (scriptedCue) {
                        if (scriptedSceneIndex + 1 < scriptedScenes.size()) {
                            ++scriptedSceneIndex;
                            nativeSceneDirector.requestScene(
                                scriptedScenes[scriptedSceneIndex]);
                            scriptedSceneSettledAt = 0;
                        } else if (scriptedSequenceOnce) {
                            if (!recordStopSignaled) {
                                signalRecordingMarker(recordStopPath);
                                recordStopSignaled = true;
                            }
                            closeRequested = true;
                            publishPairedState(
                                presetIndex, 0, 6, false,
                                static_cast<int>(
                                    nativeSceneDirector.state().currentScene),
                                static_cast<int>(
                                    nativeSceneDirector.state().currentScene));
                        } else {
                            scriptedSceneIndex = 0;
                            nativeSceneDirector.requestScene(
                                scriptedScenes.front());
                            scriptedSceneSettledAt = 0;
                        }
                    }
                }
            }
            nativeSceneState = nativeSceneDirector.update(
                musicFrame, frameSeconds,
                !pairedFollower && scriptedScenes.empty());
            if (nativeSceneState.transitioning && !nativeTransitionWasActive) {
                std::cerr << "native scene: "
                          << nativeSceneName(nativeSceneState.currentScene) << " -> "
                          << nativeSceneName(nativeSceneState.incomingScene)
                          << (nativeSceneState.motifRecalled ? " (motif recall)" : "")
                          << "\n";
                publishPairedState(
                    presetIndex, 0, 6, false,
                    static_cast<int>(nativeSceneState.incomingScene),
                    static_cast<int>(nativeSceneState.currentScene));
            } else if (!nativeSceneState.transitioning && nativeTransitionWasActive) {
                std::cerr << "native scene: "
                          << nativeSceneName(nativeSceneState.currentScene) << "\n";
                if (scriptedLeader) scriptedSceneSettledAt = now;
            }
            nativeTransitionWasActive = nativeSceneState.transitioning;
            scriptedPreviousBarPhase = musicFrame.barPhase;
        }
        bassImpact *= std::exp(-6.5f * frameSeconds);
        midImpact *= std::exp(-8.0f * frameSeconds);
        trebleImpact *= std::exp(-13.0f * frameSeconds);
        const float normalizedBass = std::clamp(
            (std::max(audioFeatures.level[0], audioFeatures.level[1]) - 0.75f) / 1.75f,
            0.0f, 1.0f);
        const float normalizedMid = std::clamp(
            (std::max({audioFeatures.level[2], audioFeatures.level[3],
                       audioFeatures.level[4]}) - 0.75f) / 1.75f,
            0.0f, 1.0f);
        const float normalizedTreble = std::clamp(
            (audioFeatures.level[5] - 0.75f) / 1.75f, 0.0f, 1.0f);
        float coverBlend = 0.0f;
        float paletteInfluence = 0.0f;
        if (hasCover) {
            const float coverAge = (now - coverStartedAt) / 1000.0f;
            // Establish the artwork, then get into the live visual quickly.
            // A fifteen-second intro made every quick relaunch look stuck on
            // the cover and hid the music response users were trying to test.
            if (coverAge < coverHoldSeconds) coverBlend = 1.0f;
            else if (coverAge < coverHoldSeconds + coverDissolveSeconds) {
                const float x = (coverAge - coverHoldSeconds)
                              / coverDissolveSeconds;
                coverBlend = 1.0f - x * x * (3.0f - 2.0f * x);
            }
            paletteInfluence = 0.12f + 0.28f * std::exp(
                -std::max(0.0f, coverAge - coverHoldSeconds) / 22.0f);
        }

        int outputW = 0, outputH = 0;
        SDL_GL_GetDrawableSize(window, &outputW, &outputH);
        if (outputW > 0 && outputH > 0 && (outputW != sourceWidth || outputH != sourceHeight)) {
            sourceWidth = outputW;
            sourceHeight = outputH;
            for (int i = 0; i < 2; ++i) {
                projectm_set_window_size(engines[i], sourceWidth, sourceHeight);
                glBindTexture(GL_TEXTURE_2D, frameTextures[i]);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, sourceWidth, sourceHeight,
                             0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            }
        }
        auto renderEngine = [&](int index) {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glViewport(0, 0, sourceWidth, sourceHeight);
            projectm_opengl_render_frame(engines[index]);
            glReadBuffer(GL_BACK);
            glBindTexture(GL_TEXTURE_2D, frameTextures[index]);
            glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, 0, 0, sourceWidth, sourceHeight);
        };
        GLuint renderedSourceTexture = frameTextures[activeEngine];
        GLuint renderedNextTexture
            = frameTextures[presetTransitionActive ? 1 - activeEngine : activeEngine];
        if (nativeEnabled) {
            std::string error;
            if (!nativeRenderer->render(musicFrame, nativeSceneState,
                                        sourceWidth, sourceHeight, albumColor,
                                        hasCover ? coverTexture : 0, coverAspect,
                                        frameSeconds, error)) {
                std::cerr << "native renderer: " << error << "\n";
                running = false;
                continue;
            }
            renderedSourceTexture
                = nativeRenderer->texture(nativeSceneState.currentScene);
            renderedNextTexture = nativeSceneState.transitioning
                ? nativeRenderer->texture(nativeSceneState.incomingScene)
                : renderedSourceTexture;
        } else {
            renderEngine(activeEngine);
            if (presetTransitionActive) renderEngine(1 - activeEngine);
        }

        const float presetBlend = nativeEnabled ? nativeSceneState.transition
            : presetTransitionActive
                ? std::clamp((now - presetTransitionStartedAt)
                             / static_cast<float>(presetTransitionDuration), 0.0f, 1.0f)
                : 0.0f;

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, outputW, outputH);
        // projectM presets leave arbitrary fixed-function state behind for
        // their own composite pass. The Omadrop pass starts from a known state.
        glDisable(GL_BLEND);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_SCISSOR_TEST);
        glDisable(GL_CULL_FACE);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(program);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, renderedSourceTexture);
        glUniform1i(glGetUniformLocation(program, "sourceFrame"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, coverTexture);
        glUniform1i(glGetUniformLocation(program, "coverFrame"), 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, renderedNextTexture);
        glUniform1i(glGetUniformLocation(program, "nextFrame"), 2);
        glUniform1f(glGetUniformLocation(program, "presetMix"), presetBlend);
        glUniform1i(glGetUniformLocation(program, "transitionMode"),
                    nativeEnabled ? 6 : transitionMode);
        const PresetProfile& sourceProfile = profileForPreset(presets[
            presetTransitionActive ? transitionOutgoingPresetIndex : presetIndex]);
        const PresetProfile& nextProfile = profileForPreset(presets[presetIndex]);
        float displayAsciiExposure = sourceProfile.asciiExposure
                                   * (1.0f - presetBlend)
                                   + nextProfile.asciiExposure * presetBlend;
        float displayFieldExposure = 1.0f;
        if (nativeEnabled) {
            const NativeSceneMaterial sourceMaterial
                = nativeSceneMaterial(nativeSceneState.currentScene);
            const NativeSceneMaterial nextMaterial = nativeSceneMaterial(
                nativeSceneState.transitioning ? nativeSceneState.incomingScene
                                               : nativeSceneState.currentScene);
            displayAsciiExposure = sourceMaterial.asciiExposure
                                 * (1.0f - presetBlend)
                                 + nextMaterial.asciiExposure * presetBlend;
            displayFieldExposure = sourceMaterial.fieldExposure
                                 * (1.0f - presetBlend)
                                 + nextMaterial.fieldExposure * presetBlend;
        }
        glUniform1i(glGetUniformLocation(program, "sourceReactionMode"),
                    reactionMode(sourceProfile));
        glUniform1i(glGetUniformLocation(program, "nextReactionMode"),
                    reactionMode(nextProfile));
        glUniform3f(glGetUniformLocation(program, "sourceReactionGain"),
                    nativeEnabled ? 0.0f : sourceProfile.kickGain * reactionScale,
                    nativeEnabled ? 0.0f : sourceProfile.snareGain * reactionScale,
                    nativeEnabled ? 0.0f : sourceProfile.hatGain * reactionScale);
        glUniform3f(glGetUniformLocation(program, "nextReactionGain"),
                    nativeEnabled ? 0.0f : nextProfile.kickGain * reactionScale,
                    nativeEnabled ? 0.0f : nextProfile.snareGain * reactionScale,
                    nativeEnabled ? 0.0f : nextProfile.hatGain * reactionScale);
        glUniform1f(glGetUniformLocation(program, "asciiExposure"),
                    displayAsciiExposure);
        glUniform1f(glGetUniformLocation(program, "fieldExposure"),
                    displayFieldExposure);
        glUniform1i(glGetUniformLocation(program, "nativeRenderer"),
                    nativeEnabled ? 1 : 0);
        glUniform2f(glGetUniformLocation(program, "resolution"), static_cast<float>(outputW), static_cast<float>(outputH));
        glUniform1f(glGetUniformLocation(program, "coverAspect"), coverAspect);
        glUniform1f(glGetUniformLocation(program, "coverMix"), coverBlend);
        glUniform3f(glGetUniformLocation(program, "albumColor"),
                    albumColor[0], albumColor[1], albumColor[2]);
        glUniform1f(glGetUniformLocation(program, "paletteInfluence"),
                    nativeEnabled ? 0.0f : paletteInfluence);
        glUniform1f(glGetUniformLocation(program, "bassLevel"), normalizedBass);
        glUniform1f(glGetUniformLocation(program, "bassImpact"), bassImpact);
        glUniform1f(glGetUniformLocation(program, "midLevel"), normalizedMid);
        glUniform1f(glGetUniformLocation(program, "trebleLevel"), normalizedTreble);
        glUniform1f(glGetUniformLocation(program, "midImpact"), midImpact);
        glUniform1f(glGetUniformLocation(program, "trebleImpact"), trebleImpact);
        glUniform1i(glGetUniformLocation(program, "asciiEnabled"), asciiEnabled ? 1 : 0);
        const auto ease = [](float value) {
            const float position = std::clamp(value, 0.0f, 1.0f);
            return position * position * (3.0f - 2.0f * position);
        };
        const float entrance = startGateOpened && revealStartedAt > 0
            ? ease((now - revealStartedAt) / 480.0f) : 0.0f;
        const float exit = closing
            ? 1.0f - ease((now - closeStartedAt)
                / static_cast<float>(closeDurationMs)) : 1.0f;
        glUniform1f(glGetUniformLocation(program, "visibility"), entrance * exit);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        if (!startGateOpened) {
            glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }
        SDL_GL_SwapWindow(window);
        if (!windowShown && (hasCover || artLookupComplete)) {
            if (!startReadyPath.empty()) {
                std::ofstream ready(startReadyPath, std::ios::trunc);
                ready << getpid() << '\n';
            }
            SDL_ShowWindow(window);
            if (startGatePath.empty()) revealStartedAt = now;
            SDL_DisableScreenSaver();
            windowShown = true;
            std::cerr << "idle: inhibition requested while Omadrop is visible\n";
        }
        // SDL's swap interval is not reliably honored by every Wayland path.
        // MilkDrop presets contain equations that advance once per rendered
        // frame, so an uncapped 250-300 FPS loop looks roughly five times too
        // fast. Keep the engine on a real 60 Hz clock regardless of compositor.
        const auto afterSwap = FrameClock::now();
        if (nextFrame > afterSwap) std::this_thread::sleep_until(nextFrame);
        nextFrame += frameInterval;
        if (nextFrame < FrameClock::now() - frameInterval) {
            nextFrame = FrameClock::now() + frameInterval;
        }
    }

    if (mprisHelperPid > 0) {
        kill(mprisHelperPid, SIGTERM);
        waitpid(mprisHelperPid, nullptr, 0);
        close(mprisHelperFd);
    }
    stopAudioCapture();
    nativeRenderer.reset();
    projectm_destroy(engines[0]);
    projectm_destroy(engines[1]);
    glDeleteProgram(program);
    glDeleteShader(vertex);
    glDeleteShader(fragment);
    glDeleteTextures(2, frameTextures.data());
    glDeleteTextures(1, &coverTexture);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
