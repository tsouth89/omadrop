#include <GL/glew.h>
#include <SDL2/SDL.h>
#include <projectM-4/projectM.h>
#include <png.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <deque>
#include <fcntl.h>
#include <fstream>
#include <filesystem>
#include <iostream>
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
#include "preset_adapters.h"
#include "preset_profiles.h"

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
uniform int reactionMode;
uniform float reactionGain;

float luminance(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }

vec3 sceneSample(vec2 sampleUv) {
    vec2 coverSampleUv = sampleUv;
    vec2 p = sampleUv - 0.5;
    vec2 movement = vec2(0.0);
    if (reactionMode == 0) {
        // Radial presets breathe through their existing vanishing point.
        movement = p * (0.020 * bassImpact * reactionGain)
                 + vec2(-p.y, p.x) * (0.006 * midImpact * reactionGain);
    } else if (reactionMode == 1) {
        // Structured fields flex in broad opposing planes.
        movement.x = sin(sampleUv.y * 9.0) * (0.016 * bassImpact * reactionGain);
        movement.y = sin(sampleUv.x * 7.0) * (0.006 * midImpact * reactionGain);
    } else if (reactionMode == 2) {
        // Organic subjects bend as connected ribbons, preserving their silhouette.
        movement.x = sin(sampleUv.y * 8.0 + sampleUv.x * 2.0)
                   * (0.018 * bassImpact + 0.005 * midImpact) * reactionGain;
        movement.y = sin(sampleUv.x * 6.0) * (0.005 * midImpact * reactionGain);
    } else {
        // Multipole scenes pull their existing lobes apart on a kick.
        movement = vec2(sign(p.x), sign(p.y)) * (0.010 * bassImpact * reactionGain)
                 * smoothstep(0.06, 0.42, length(p));
        movement += vec2(-p.y, p.x) * (0.005 * midImpact * reactionGain);
    }
    sampleUv = clamp(sampleUv + movement, vec2(0.002), vec2(0.998));
    float easedPresetMix = presetMix * presetMix * (3.0 - 2.0 * presetMix);
    float bridge = sin(3.14159265 * easedPresetMix);
    vec2 outgoingUv = (sampleUv - 0.5) * (1.0 - 0.025 * bridge) + 0.5;
    vec2 incomingUv = (sampleUv - 0.5) * (1.025 - 0.025 * easedPresetMix) + 0.5;
    vec3 outgoing = texture(sourceFrame, outgoingUv).rgb;
    vec3 incoming = texture(nextFrame, incomingUv).rgb;

    // Do not reveal the incoming composition as one rectangular layer. Broad
    // connected flow bands let its geometry form inside the outgoing feedback
    // while a temporary luminance match prevents a sudden palette block.
    float flow;
    if (transitionMode == 0) {
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
    float outgoingLight = luminance(outgoing);
    float incomingLight = max(0.08, luminance(incoming));
    vec3 matchedIncoming = incoming * clamp((0.35 + outgoingLight * 0.9) / incomingLight,
                                             0.55, 1.35);
    incoming = mix(incoming, matchedIncoming, (1.0 - easedPresetMix) * 0.42);
    vec3 visual = mix(outgoing, incoming, localMix);
    float visualLight = luminance(visual);
    vec3 albumGrade = visual * mix(vec3(1.0), albumColor * 1.65, 0.58)
                    + albumColor * visualLight * 0.16;
    visual = mix(visual, albumGrade, paletteInfluence);
    if (coverMix <= 0.0) return visual;
    p = coverSampleUv - 0.5;
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
        color = vec4(sceneSample(uv), 1.0);
        return;
    }
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
    float glyphRadius = 1.90 + 0.55 * bassImpact + 0.09 * midImpact;
    float glyphMask = 1.0 - smoothstep(glyphRadius - 0.48, glyphRadius + 0.48,
                                      glyphDistance);
    if (glyphMask <= 0.01) {
        color = vec4(0.0, 0.0, 0.0, 1.0); return;
    }

    vec2 dotOrigin = cell * cellSize + vec2(float(dx) * 6.0, float(dy) * 6.0);
    vec3 brightest = vec3(0.0);
    float best = 0.0;
    for (int oy = 0; oy < 6; ++oy) {
        for (int ox = 0; ox < 6; ++ox) {
            vec2 sampleUv = (dotOrigin + vec2(float(ox) + 0.5, float(oy) + 0.5)) / resolution;
            // Keep the viewpoint stable. Bass affects glyph weight and light,
            // never the camera, so the eye can continue following the subject.
            vec3 candidate = sceneSample(sampleUv);
            float level = luminance(candidate);
            if (level > best) { best = level; brightest = candidate; }
        }
    }

    const float threshold[8] = float[8](0.08, 0.58, 0.33, 0.83, 0.70, 0.20, 0.95, 0.45);
    float level = min(1.0, sqrt(max(best, 0.0)) * 1.25);
    level = floor(level * 5.0 + 0.5) / 5.0;
    // Bass briefly reveals more of the preset's own dim structure. This makes
    // the active subject feel heavier without adding a ring or moving the
    // camera independently of the composition.
    // Treble exposes the smallest source details while mids enrich the color
    // already present in the preset. Each band changes a different material
    // property, so the result reads as music rather than one global pulse.
    float activeThreshold = threshold[dy * 2 + dx]
                          - 0.06 * bassImpact - 0.025 * trebleLevel
                          - 0.045 * trebleImpact;
    if (level < activeThreshold) { color = vec4(0.0, 0.0, 0.0, 1.0); return; }
    vec3 energized = mix(brightest, brightest * brightest * 1.12, 0.16 * bassImpact);
    float grey = luminance(energized);
    energized = mix(vec3(grey), energized, 1.0 + 0.08 * midLevel + 0.12 * midImpact);
    // The source preset owns luminance. Audio changes glyph weight, detail,
    // and chroma, but must never globally flash the frame toward white.
    color = vec4(energized * glyphMask
                 * (0.80 + level * 0.32 + 0.035 * bassLevel
                    + 0.025 * trebleLevel), 1.0);
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
    const std::string scale = std::to_string(visualTempo);
    preset = std::regex_replace(preset, std::regex(R"(\btime\b)"), "(time*" + scale + ")");
    preset = std::regex_replace(preset, std::regex(R"(\bframe\b)"), "(frame*" + scale + ")");
    applyOmadropAdapter(filename, preset);
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
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
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

std::filesystem::path syncSettingsPath() {
    if (const char* configHome = std::getenv("XDG_CONFIG_HOME")) {
        return std::filesystem::path(configHome) / "omadrop" / "sync-ms";
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home) / ".config" / "omadrop" / "sync-ms";
    }
    return {};
}

void saveSyncDelay(unsigned int milliseconds) {
    const auto path = syncSettingsPath();
    if (path.empty()) return;
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    if (error) return;
    std::ofstream output(path);
    if (output) output << milliseconds << "\n";
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "usage: projectm-ascii-live PRESET.milk [PRESET.milk ...]\n";
        return 2;
    }
    signal(SIGTERM, requestStop);
    signal(SIGINT, requestStop);
    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    Uint32 windowFlags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
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
    if (!window) return 1;
    SDL_GLContext context = SDL_GL_CreateContext(window);
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
    std::size_t presetIndex = 0;
    std::mt19937 randomEngine(std::random_device{}());
    std::deque<std::size_t> recentPresets{presetIndex};
    auto chooseAutomaticPreset = [&](PresetEnergy targetEnergy) {
        struct Candidate { std::size_t index; float score; };
        std::vector<Candidate> candidates;
        auto recentlyUsed = [&](std::size_t index) {
            return std::find(recentPresets.begin(), recentPresets.end(), index) != recentPresets.end();
        };
        const PresetProfile& current = profileForPreset(presets[presetIndex]);
        for (std::size_t i = 0; i < presets.size(); ++i) {
            if (i == presetIndex || recentlyUsed(i)) continue;
            const PresetProfile& candidate = profileForPreset(presets[i]);
            const int energyDistance = std::abs(static_cast<int>(candidate.energy)
                                              - static_cast<int>(targetEnergy));
            float score = energyDistance == 0 ? 4.0f : energyDistance == 1 ? 1.0f : -4.0f;
            score += topologyFamily(current.topology) == topologyFamily(candidate.topology)
                ? 3.0f : -2.0f;
            score += directionsCompatible(current.direction, candidate.direction) ? 2.0f : -1.0f;
            score -= std::abs(current.asciiDensity - candidate.asciiDensity) * 5.0f;
            std::uniform_real_distribution<float> variation(-0.75f, 0.75f);
            candidates.push_back({i, score + variation(randomEngine)});
        }
        if (candidates.empty()) {
            for (std::size_t i = 0; i < presets.size(); ++i) {
                if (i != presetIndex && !recentlyUsed(i)) candidates.push_back({i, 0.0f});
            }
        }
        if (candidates.empty()) {
            for (std::size_t i = 0; i < presets.size(); ++i) {
                if (i != presetIndex) candidates.push_back({i, 0.0f});
            }
        }
        std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
            return a.score > b.score;
        });
        const std::size_t finalistCount = std::min<std::size_t>(3, candidates.size());
        std::uniform_int_distribution<std::size_t> pick(0, finalistCount - 1);
        return candidates[pick(randomEngine)].index;
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
    int reactionMode = reactionModeForTopology(profileForPreset(presets[presetIndex]).topology);
    float reactionGain = profileForPreset(presets[presetIndex]).reactionGain;
    if (!loadPresetAtVisualTempo(engines[activeEngine], presets[presetIndex], false)) return 1;
    std::cerr << "preset: " << presets[presetIndex] << "\n";
    uint64_t transitionWindowAt = SDL_GetTicks64() + 9000;
    uint64_t transitionDeadlineAt = SDL_GetTicks64() + 13000;

    std::string sink = commandOutput("pactl get-default-sink");
    unsigned int syncDelayMs = sink.rfind("bluez_", 0) == 0 ? 180u : 35u;
    if (const char* configuredDelay = std::getenv("OMADROP_SYNC_MS")) {
        syncDelayMs = static_cast<unsigned int>(std::clamp(std::atoi(configuredDelay), 0, 500));
    } else {
        std::ifstream savedDelay(syncSettingsPath());
        int milliseconds = 0;
        if (savedDelay >> milliseconds) {
            syncDelayMs = static_cast<unsigned int>(std::clamp(milliseconds, 0, 500));
        }
    }
    std::deque<float> delayedPcm;
    std::cerr << "audio sync delay: " << syncDelayMs << " ms\n";
    int audioPipe[2];
    if (pipe2(audioPipe, O_CLOEXEC) != 0) return 1;
    const pid_t audioPid = fork();
    if (audioPid == 0) {
        dup2(audioPipe[1], STDOUT_FILENO);
        const int nullFd = open("/dev/null", O_WRONLY);
        if (nullFd >= 0) dup2(nullFd, STDERR_FILENO);
        close(audioPipe[0]);
        close(audioPipe[1]);
        if (sink.empty()) {
            execlp("pw-record", "pw-record", "--raw", "--rate", "44100",
                   "--channels", "2", "--format", "f32", "--latency", "20ms",
                   "-P", "{ stream.capture.sink=true }", "-", static_cast<char*>(nullptr));
        } else {
            execlp("pw-record", "pw-record", "--raw", "--rate", "44100",
                   "--channels", "2", "--format", "f32", "--latency", "20ms",
                   "-P", "{ stream.capture.sink=true }", "--target", sink.c_str(), "-",
                   static_cast<char*>(nullptr));
        }
        _exit(127);
    }
    close(audioPipe[1]);
    if (audioPid < 0) {
        close(audioPipe[0]);
        return 1;
    }
    const int audioFd = audioPipe[0];
    fcntl(audioFd, F_SETFL, fcntl(audioFd, F_GETFL) | O_NONBLOCK);
    std::vector<float> pcm(4096 * 2);
    std::vector<float> visualPcm(4096 * 2);
    const unsigned int projectmSampleLimit = projectm_pcm_get_max_samples();
    std::vector<float> projectmPcm(projectmSampleLimit * 2);
    double visualBassPhase = 0.0;
    double visualMidPhase = 0.0;
    double visualTreblePhase = 0.0;
    double fallbackPhase = 0.0;
    const bool syntheticFallback = std::getenv("OMADROP_SYNTHETIC_AUDIO") != nullptr;
    bool lastFallback = false;
    bool reportedAudioMode = false;
    uint64_t lastNonSilentAudioAt = SDL_GetTicks64();
    uint64_t previousFrameAt = SDL_GetTicks64();
    AudioFeatureBus featureBus;
    AudioFeatures audioFeatures;
    float bassImpact = 0.0f;
    float midImpact = 0.0f;
    float trebleImpact = 0.0f;
    float musicalEnergy = 1.0f;
    const bool debugAudio = std::getenv("OMADROP_DEBUG_AUDIO") != nullptr;
    uint64_t lastClockLogAt = 0;
    float coverAspect = 1.0f;
    std::array<float, 3> albumColor{0.72f, 0.82f, 1.0f};
    bool hasCover = false;
    std::string currentArtPath;
    uint64_t coverStartedAt = 0;
    uint64_t nextArtPollAt = 0;
    const std::filesystem::path artHelper = std::filesystem::canonical(argv[0]).parent_path()
        / ".." / ".." / "bin" / "mpris-art";
    pid_t artHelperPid = -1;
    int artHelperFd = -1;
    std::string artHelperOutput;

    auto startArtPoll = [&]() {
        int pipeFds[2];
        if (pipe2(pipeFds, O_CLOEXEC | O_NONBLOCK) != 0) return;
        const pid_t pid = fork();
        if (pid == 0) {
            dup2(pipeFds[1], STDOUT_FILENO);
            const int nullFd = open("/dev/null", O_WRONLY);
            if (nullFd >= 0) dup2(nullFd, STDERR_FILENO);
            close(pipeFds[0]);
            close(pipeFds[1]);
            execl(artHelper.c_str(), artHelper.c_str(), static_cast<char*>(nullptr));
            _exit(127);
        }
        close(pipeFds[1]);
        if (pid < 0) {
            close(pipeFds[0]);
            return;
        }
        artHelperPid = pid;
        artHelperFd = pipeFds[0];
        artHelperOutput.clear();
    };

    bool running = true;
    bool asciiEnabled = true;
    bool windowShown = false;
    bool artLookupComplete = false;
    const uint64_t automaticQuitAt = std::getenv("OMADROP_AUTO_QUIT_MS")
        ? SDL_GetTicks64() + static_cast<uint64_t>(std::max(0, std::atoi(std::getenv("OMADROP_AUTO_QUIT_MS"))))
        : 0;
    using FrameClock = std::chrono::steady_clock;
    constexpr auto frameInterval = std::chrono::nanoseconds(1000000000 / 60);
    auto nextFrame = FrameClock::now() + frameInterval;
    while (running && !stopRequested) {
        bool skipPreset = false;
        bool previousPreset = false;
        bool bassHitThisFrame = false;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT ||
                (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)) running = false;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_n) skipPreset = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_p) previousPreset = true;
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_a) {
                asciiEnabled = !asciiEnabled;
                std::cerr << "display: " << (asciiEnabled ? "Omadrop ASCII" : "original MilkDrop") << "\n";
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_F11) {
                const bool fullscreen = (SDL_GetWindowFlags(window) & SDL_WINDOW_FULLSCREEN_DESKTOP) != 0;
                SDL_SetWindowFullscreen(window, fullscreen ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_LEFTBRACKET) {
                syncDelayMs = syncDelayMs >= 10 ? syncDelayMs - 10 : 0;
                saveSyncDelay(syncDelayMs);
                std::cerr << "audio sync delay: " << syncDelayMs << " ms\n";
            }
            if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_RIGHTBRACKET) {
                syncDelayMs = std::min(500u, syncDelayMs + 10);
                saveSyncDelay(syncDelayMs);
                std::cerr << "audio sync delay: " << syncDelayMs << " ms\n";
            }
        }
        const uint64_t now = SDL_GetTicks64();
        if (automaticQuitAt > 0 && now >= automaticQuitAt) running = false;
        if (now >= nextArtPollAt && artHelperPid < 0) startArtPoll();
        if (artHelperPid > 0) {
            std::array<char, 1024> artBuffer{};
            ssize_t artBytes = 0;
            while ((artBytes = read(artHelperFd, artBuffer.data(), artBuffer.size())) > 0) {
                artHelperOutput.append(artBuffer.data(), static_cast<std::size_t>(artBytes));
            }
            int artStatus = 0;
            if (waitpid(artHelperPid, &artStatus, WNOHANG) == artHelperPid) {
                close(artHelperFd);
                artHelperFd = -1;
                artHelperPid = -1;
                artLookupComplete = true;
                nextArtPollAt = now + 1000;
                while (!artHelperOutput.empty()
                       && (artHelperOutput.back() == '\n' || artHelperOutput.back() == '\r')) {
                    artHelperOutput.pop_back();
                }
                const std::string artPath = artHelperOutput;
                if (!artPath.empty() && artPath != currentArtPath
                    && loadPngTexture(artPath, coverTexture, coverAspect)) {
                currentArtPath = artPath;
                albumColor = loadPaletteColor(artPath);
                hasCover = true;
                coverStartedAt = now;
                presetIndex = 0;
                recentPresets.clear();
                recentPresets.push_back(presetIndex);
                presetTransitionActive = false;
                loadPresetAtVisualTempo(engines[activeEngine], presets[presetIndex], false);
                const PresetProfile& openingProfile = profileForPreset(presets[presetIndex]);
                reactionMode = reactionModeForTopology(openingProfile.topology);
                reactionGain = openingProfile.reactionGain;
                // Let the first preset breathe after the ten-second cover
                // sequence, then enter the same shorter scene cadence.
                transitionWindowAt = now + 19000;
                transitionDeadlineAt = now + 23000;
                std::cerr << "cover: " << artPath << "\n";
                }
            }
        }
        const ssize_t bytes = read(audioFd, pcm.data(), pcm.size() * sizeof(float));
        unsigned int capturedFrames = bytes > 0
            ? static_cast<unsigned int>(bytes / (sizeof(float) * 2)) : 0;
        float peak = 0.0f;
        for (unsigned int i = 0; i < capturedFrames * 2; ++i) peak = std::max(peak, std::abs(pcm[i]));
        if (capturedFrames > 0 && peak >= 1e-5f) lastNonSilentAudioAt = now;
        // A nonblocking PipeWire fd normally has empty reads between packets.
        // Only call it silence after a sustained gap, or the analyzer receives
        // alternating real and synthetic audio and visibly loses the music.
        const bool fallback = now - lastNonSilentAudioAt >= 250;
        if (!reportedAudioMode || fallback != lastFallback) {
            std::cerr << "audio: "
                      << (fallback ? (syntheticFallback ? "synthetic test" : "silence")
                                   : "PipeWire") << "\n";
            lastFallback = fallback;
            reportedAudioMode = true;
        }
        unsigned int stereoFrames = 0;
        if (fallback) {
            stereoFrames = 735;
            std::fill_n(pcm.begin(), stereoFrames * 2, 0.0f);
            if (syntheticFallback) {
                const double seconds = SDL_GetTicks64() / 1000.0;
                const double beat = std::pow(std::max(0.0, std::sin(seconds * 6.283185307)), 12.0);
                for (unsigned int i = 0; i < stereoFrames; ++i) {
                    const double t = fallbackPhase + i / 44100.0;
                    pcm[i * 2] = static_cast<float>(0.38 * std::sin(t * 345.575) * (0.5 + beat)
                                                   + 0.18 * std::sin(t * 1463.0));
                    pcm[i * 2 + 1] = static_cast<float>(0.36 * std::sin(t * 383.274) * (0.5 + beat)
                                                       + 0.20 * std::sin(t * 1954.0));
                }
                fallbackPhase += stereoFrames / 44100.0;
            }
            delayedPcm.clear();
        } else {
            for (unsigned int i = 0; i < capturedFrames * 2; ++i) delayedPcm.push_back(pcm[i]);
            const std::size_t syncDelaySamples
                = static_cast<std::size_t>(44100 * syncDelayMs / 1000) * 2;
            const std::size_t readySamples = delayedPcm.size() > syncDelaySamples
                ? delayedPcm.size() - syncDelaySamples : 0;
            // Analyze one exact 60 Hz hop. PipeWire read sizes vary, and
            // comparing RMS across differently sized packets makes onset
            // timing depend on packet boundaries instead of the music.
            constexpr std::size_t analysisHopFrames = 44100 / 60;
            stereoFrames = readySamples / 2 >= analysisHopFrames
                ? static_cast<unsigned int>(analysisHopFrames) : 0;
            for (unsigned int i = 0; i < stereoFrames * 2; ++i) {
                pcm[i] = delayedPcm.front();
                delayedPcm.pop_front();
            }
        }
        if (stereoFrames > 0) {
            audioFeatures = featureBus.processStereo(pcm.data(), stereoFrames);
            bassImpact = std::max(bassImpact, audioFeatures.kickImpact);
            midImpact = std::max(midImpact, audioFeatures.snareImpact);
            trebleImpact = std::max(trebleImpact, audioFeatures.hatImpact);
            bassHitThisFrame = !fallback && audioFeatures.kick;
            if (debugAudio && audioFeatures.kick) std::cerr << "kick hit\n";
            if (debugAudio && audioFeatures.snare) std::cerr << "snare hit\n";
            if (debugAudio && audioFeatures.hat) std::cerr << "hat hit\n";
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
                          << " phrase=" << audioFeatures.phrasePhase << "\n";
                lastClockLogAt = now;
            }

            // projectM derives bass/mid/treb and waveform motion from the PCM
            // it receives. Give it a visual-only sidechain whose transients
            // have more dynamic range, so each preset's own equations react.
            // This never touches the audio sent to the speakers.
            const float visualDrive = 1.0f + 0.55f * bassImpact
                                             + 0.28f * midImpact
                                             + 0.14f * trebleImpact;
            constexpr double tau = 6.28318530717958647692;
            for (unsigned int i = 0; i < stereoFrames; ++i) {
                // Short tone bursts put unmistakable energy into the same
                // bands exposed to MilkDrop equations. They are control
                // signals only, not audible output.
                const float bassBurst = 0.42f * bassImpact
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
            for (unsigned int i = 0; i < stereoFrames * 2; ++i) {
                visualPeak = std::max(visualPeak, std::abs(visualPcm[i]));
            }
            const float visualScale = visualPeak > 0.95f ? 0.95f / visualPeak : 1.0f;
            for (unsigned int i = 0; i < stereoFrames * 2; ++i) {
                visualPcm[i] *= visualScale;
            }
            // projectM stores at most 480 samples per update. Resample the
            // complete 735-sample video interval instead of letting it discard
            // the final third of every frame.
            const unsigned int forwardedFrames = std::min(stereoFrames, projectmSampleLimit);
            const float sourceSpan = static_cast<float>(stereoFrames - 1);
            const float targetSpan = static_cast<float>(std::max(1u, forwardedFrames - 1));
            for (unsigned int i = 0; i < forwardedFrames; ++i) {
                const float sourcePosition = sourceSpan * i / targetSpan;
                const unsigned int left = static_cast<unsigned int>(sourcePosition);
                const unsigned int right = std::min(left + 1, stereoFrames - 1);
                const float fraction = sourcePosition - left;
                for (unsigned int channel = 0; channel < 2; ++channel) {
                    projectmPcm[i * 2 + channel]
                        = visualPcm[left * 2 + channel] * (1.0f - fraction)
                        + visualPcm[right * 2 + channel] * fraction;
                }
            }
            projectm_pcm_add_float(engines[0], projectmPcm.data(), forwardedFrames, PROJECTM_STEREO);
            projectm_pcm_add_float(engines[1], projectmPcm.data(), forwardedFrames, PROJECTM_STEREO);
        }

        // Scene changes are musical events. After a minimum dwell, wait for a
        // real bass onset and begin projectM's smooth blend on that boundary.
        // The deadline still advances ambient or unusually quiet tracks.
        const bool confidentBar = audioFeatures.beatConfidence >= 0.35f
                               && audioFeatures.barCrossed;
        const bool fallbackOnset = audioFeatures.beatConfidence < 0.35f
                                && bassHitThisFrame;
        const bool musicalTransition = now >= transitionWindowAt
                                    && (confidentBar || fallbackOnset);
        const bool deadlineTransition = now >= transitionDeadlineAt;
        if (!presetTransitionActive && presets.size() > 1
            && (skipPreset || previousPreset || musicalTransition || deadlineTransition)) {
            const std::size_t outgoingPreset = presetIndex;
            const PresetEnergy targetEnergy = energyForTrack(musicalEnergy);
            presetIndex = previousPreset ? (presetIndex + presets.size() - 1) % presets.size()
                : skipPreset ? (presetIndex + 1) % presets.size()
                : chooseAutomaticPreset(targetEnergy);
            const PresetProfile& outgoing = profileForPreset(presets[outgoingPreset]);
            const PresetProfile& incoming = profileForPreset(presets[presetIndex]);
            if (outgoing.topology == incoming.topology) {
                presetTransitionDuration = 6000;
                transitionMode = 0;
            }
            else if (outgoing.topology == PresetTopology::Organic
                     || incoming.topology == PresetTopology::Organic) {
                presetTransitionDuration = 7000;
                transitionMode = 2;
            } else if (topologyFamily(outgoing.topology) == topologyFamily(incoming.topology)) {
                presetTransitionDuration = 6500;
                transitionMode = 1;
            } else {
                presetTransitionDuration = 7500;
                transitionMode = 3;
            }
            recentPresets.push_back(presetIndex);
            const std::size_t historyLimit = std::min<std::size_t>(5, presets.size() - 1);
            while (recentPresets.size() > historyLimit) recentPresets.pop_front();
            const int incomingEngine = 1 - activeEngine;
            if (!loadPresetAtVisualTempo(engines[incomingEngine], presets[presetIndex], false)) {
                std::cerr << "could not load preset: " << presets[presetIndex] << "\n";
            }
            presetTransitionActive = true;
            presetTransitionStartedAt = now;
            transitionWindowAt = UINT64_MAX;
            transitionDeadlineAt = UINT64_MAX;
            const char* energyName = targetEnergy == PresetEnergy::Driving ? "driving"
                : targetEnergy == PresetEnergy::Medium ? "medium" : "calm";
            std::cerr << "preset: " << presets[presetIndex]
                      << (previousPreset ? " (previous)" : skipPreset ? " (manual)"
                          : musicalTransition ? " (automatic on boundary)"
                          : " (automatic on deadline)")
                      << ", energy: " << energyName << "\n";
        }

        if (presetTransitionActive
            && now - presetTransitionStartedAt >= presetTransitionDuration) {
            activeEngine = 1 - activeEngine;
            presetTransitionActive = false;
            const PresetProfile& profile = profileForPreset(presets[presetIndex]);
            reactionMode = reactionModeForTopology(profile.topology);
            reactionGain = profile.reactionGain;
            std::uniform_int_distribution<uint64_t> dwellTime(profile.dwellMinMs,
                                                               profile.dwellMaxMs);
            const uint64_t dwell = dwellTime(randomEngine);
            transitionWindowAt = now + dwell;
            transitionDeadlineAt = transitionWindowAt + 4000;
        }

        const float frameSeconds = std::min(0.1f, (now - previousFrameAt) / 1000.0f);
        previousFrameAt = now;
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
            if (coverAge < 5.0f) coverBlend = 1.0f;
            else if (coverAge < 10.0f) {
                const float x = (coverAge - 5.0f) / 5.0f;
                coverBlend = 1.0f - x * x * (3.0f - 2.0f * x);
            }
            paletteInfluence = 0.12f + 0.28f * std::exp(-std::max(0.0f, coverAge - 5.0f) / 22.0f);
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
        renderEngine(activeEngine);
        if (presetTransitionActive) renderEngine(1 - activeEngine);

        const float presetBlend = presetTransitionActive
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
        glBindTexture(GL_TEXTURE_2D, frameTextures[activeEngine]);
        glUniform1i(glGetUniformLocation(program, "sourceFrame"), 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, coverTexture);
        glUniform1i(glGetUniformLocation(program, "coverFrame"), 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, frameTextures[presetTransitionActive ? 1 - activeEngine : activeEngine]);
        glUniform1i(glGetUniformLocation(program, "nextFrame"), 2);
        glUniform1f(glGetUniformLocation(program, "presetMix"), presetBlend);
        glUniform1i(glGetUniformLocation(program, "transitionMode"), transitionMode);
        glUniform1i(glGetUniformLocation(program, "reactionMode"), reactionMode);
        glUniform1f(glGetUniformLocation(program, "reactionGain"), reactionGain);
        glUniform2f(glGetUniformLocation(program, "resolution"), static_cast<float>(outputW), static_cast<float>(outputH));
        glUniform1f(glGetUniformLocation(program, "coverAspect"), coverAspect);
        glUniform1f(glGetUniformLocation(program, "coverMix"), coverBlend);
        glUniform3f(glGetUniformLocation(program, "albumColor"),
                    albumColor[0], albumColor[1], albumColor[2]);
        glUniform1f(glGetUniformLocation(program, "paletteInfluence"), paletteInfluence);
        glUniform1f(glGetUniformLocation(program, "bassLevel"), normalizedBass);
        glUniform1f(glGetUniformLocation(program, "bassImpact"), bassImpact);
        glUniform1f(glGetUniformLocation(program, "midLevel"), normalizedMid);
        glUniform1f(glGetUniformLocation(program, "trebleLevel"), normalizedTreble);
        glUniform1f(glGetUniformLocation(program, "midImpact"), midImpact);
        glUniform1f(glGetUniformLocation(program, "trebleImpact"), trebleImpact);
        glUniform1i(glGetUniformLocation(program, "asciiEnabled"), asciiEnabled ? 1 : 0);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        SDL_GL_SwapWindow(window);
        if (!windowShown && (hasCover || artLookupComplete)) {
            SDL_ShowWindow(window);
            windowShown = true;
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

    if (artHelperPid > 0) {
        kill(artHelperPid, SIGTERM);
        waitpid(artHelperPid, nullptr, 0);
        close(artHelperFd);
    }
    kill(audioPid, SIGTERM);
    waitpid(audioPid, nullptr, 0);
    close(audioFd);
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
