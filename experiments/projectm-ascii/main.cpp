#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <projectM-4/projectM.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "preset_adapters.h"
#include "preset_profiles.h"

namespace {
constexpr int width = 960;
constexpr int height = 540;
constexpr int fps = 60;
constexpr int sampleRate = 44100;
constexpr double pi = 3.14159265358979323846;

void writePpm(const std::string& path, const std::vector<uint8_t>& rgb) {
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << width << " " << height << "\n255\n";
    out.write(reinterpret_cast<const char*>(rgb.data()), static_cast<std::streamsize>(rgb.size()));
}

std::vector<uint8_t> flipRgb(const std::vector<uint8_t>& rgba) {
    std::vector<uint8_t> rgb(width * height * 3);
    for (int y = 0; y < height; ++y) {
        const int sourceY = height - 1 - y;
        for (int x = 0; x < width; ++x) {
            const size_t source = (sourceY * width + x) * 4;
            const size_t target = (y * width + x) * 3;
            std::copy_n(rgba.data() + source, 3, rgb.data() + target);
        }
    }
    return rgb;
}

double meanDifference(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0;
    double sum = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        sum += std::abs(static_cast<int>(a[i]) - static_cast<int>(b[i]));
    }
    return sum / (a.size() * 255.0);
}

std::vector<uint8_t> coarseLuminance(const std::vector<uint8_t>& rgb) {
    constexpr int columns = 60;
    constexpr int rows = 30;
    constexpr int cellWidth = width / columns;
    constexpr int cellHeight = height / rows;
    std::vector<uint8_t> result(columns * rows);
    for (int row = 0; row < rows; ++row) {
        for (int column = 0; column < columns; ++column) {
            unsigned int sum = 0;
            for (int y = 0; y < cellHeight; ++y) {
                for (int x = 0; x < cellWidth; ++x) {
                    const std::size_t at = ((row * cellHeight + y) * width
                                            + column * cellWidth + x) * 3;
                    sum += static_cast<unsigned int>(0.299f * rgb[at]
                           + 0.587f * rgb[at + 1] + 0.114f * rgb[at + 2]);
                }
            }
            result[row * columns + column]
                = static_cast<uint8_t>(sum / (cellWidth * cellHeight));
        }
    }
    return result;
}

std::vector<uint8_t> braillePass(const std::vector<uint8_t>& source, float exposure) {
    std::vector<uint8_t> result(width * height * 3, 0);
    constexpr int cellW = 12;
    constexpr int cellH = 24;
    constexpr float thresholds[8] = {0.08f, 0.58f, 0.33f, 0.83f,
                                      0.70f, 0.20f, 0.95f, 0.45f};

    for (int cy = 0; cy + cellH <= height; cy += cellH) {
        for (int cx = 0; cx + cellW <= width; cx += cellW) {
            for (int dy = 0; dy < 4; ++dy) {
                for (int dx = 0; dx < 2; ++dx) {
                    // A braille dot represents an area, not a single source
                    // pixel. Taking the brightest sample preserves MilkDrop's
                    // thin custom waves and shape outlines at HiDPI.
                    const int x0 = cx + dx * cellW / 2;
                    const int x1 = cx + (dx + 1) * cellW / 2;
                    const int y0 = cy + dy * cellH / 4;
                    const int y1 = cy + (dy + 1) * cellH / 4;
                    size_t sample = (y0 * width + x0) * 3;
                    float maxLuminance = -1.0f;
                    for (int sy = y0; sy < y1; ++sy) {
                        for (int sx = x0; sx < x1; ++sx) {
                            const size_t candidate = (sy * width + sx) * 3;
                            const float r = source[candidate] / 255.0f;
                            const float g = source[candidate + 1] / 255.0f;
                            const float b = source[candidate + 2] / 255.0f;
                            const float luminance = 0.299f * r + 0.587f * g + 0.114f * b;
                            if (luminance > maxLuminance) {
                                maxLuminance = luminance;
                                sample = candidate;
                            }
                        }
                    }
                    float level = std::min(1.0f, std::sqrt(std::max(0.0f, maxLuminance))
                                                * 1.25f * std::sqrt(exposure));
                    level = std::floor(level * 5.0f + 0.5f) / 5.0f;
                    if (level < thresholds[dy * 2 + dx]) continue;

                    const int px = cx + (dx == 0 ? 1 : 7);
                    const int py = cy + 1 + dy * 6;
                    const float outputScale = (0.80f + level * 0.32f) * exposure;
                    for (int oy = 0; oy < 4; ++oy) {
                        for (int ox = 0; ox < 4; ++ox) {
                            const float dotX = ox - 1.5f;
                            const float dotY = oy - 1.5f;
                            if (dotX * dotX + dotY * dotY > 4.5f) continue;
                            const size_t target = ((py + oy) * width + px + ox) * 3;
                            for (int channel = 0; channel < 3; ++channel) {
                                result[target + channel] = static_cast<uint8_t>(std::min(
                                    255.0f, source[sample + channel] * outputScale));
                            }
                        }
                    }
                }
            }
        }
    }
    return result;
}
} // namespace

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cerr << "usage: projectm-ascii PRESET.milk NATIVE.ppm ASCII.ppm\n";
        return 2;
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = SDL_CreateWindow("Omadrop projectM spike", SDL_WINDOWPOS_UNDEFINED,
                                           SDL_WINDOWPOS_UNDEFINED, width, height,
                                           SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) {
        std::cerr << "window creation failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) {
        std::cerr << "OpenGL context failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    projectm_handle projectm = projectm_create();
    if (!projectm) {
        std::cerr << "projectM initialization failed\n";
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    projectm_set_window_size(projectm, width, height);
    projectm_set_mesh_size(projectm, 48, 36);
    projectm_set_fps(projectm, fps);
    projectm_set_preset_locked(projectm, true);
    const char* texturePaths[] = {"/usr/share/projectM/textures", "/usr/share/projectM/presets"};
    projectm_set_texture_search_paths(projectm, texturePaths, 2);
    // Load from memory like the live renderer. The installed projectM build
    // can silently leave its default preset active when this review harness
    // uses projectm_load_preset_file(), producing false curation screenshots.
    std::ifstream presetFile(argv[1], std::ios::binary);
    if (!presetFile) {
        std::cerr << "could not open preset: " << argv[1] << "\n";
        projectm_destroy(projectm);
        SDL_GL_DeleteContext(context);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }
    std::ostringstream presetContents;
    presetContents << presetFile.rdbuf();
    std::string preset = presetContents.str();
    if (std::getenv("OMADROP_DISABLE_ADAPTERS") == nullptr) {
        applyOmadropAdapter(argv[1], preset);
    }
    projectm_load_preset_data(projectm, preset.c_str(), false);
    const auto* profile = findProfileForPreset(argv[1]);
    const float asciiExposure = profile ? profile->asciiExposure : 1.0f;

    const int samplesPerFrame = sampleRate / fps;
    std::vector<float> pcm(samplesPerFrame * 2);
    const unsigned int projectmFrames = projectm_pcm_get_max_samples();
    std::vector<float> projectmPcm(projectmFrames * 2);
    std::vector<uint8_t> rgba(width * height * 4);
    std::vector<uint8_t> previousNative;
    std::vector<uint8_t> previousAscii;
    std::vector<uint8_t> previousNativeCoarse;
    std::vector<uint8_t> previousAsciiCoarse;
    double nativeHitMotion = 0.0;
    double nativeIdleMotion = 0.0;
    double asciiHitMotion = 0.0;
    double asciiIdleMotion = 0.0;
    double nativeCoarseHitMotion = 0.0;
    double nativeCoarseIdleMotion = 0.0;
    double asciiCoarseHitMotion = 0.0;
    double asciiCoarseIdleMotion = 0.0;
    int hitFrames = 0;
    int idleFrames = 0;
    double phase = 0.0;
    for (int frame = 0; frame < fps * 8; ++frame) {
        const double seconds = frame / static_cast<double>(fps);
        const double beat = std::pow(std::max(0.0, std::sin(seconds * pi * 2.0)), 12.0);
        for (int i = 0; i < samplesPerFrame; ++i) {
            const double t = phase + i / static_cast<double>(sampleRate);
            const float left = static_cast<float>(0.42 * std::sin(2.0 * pi * 55.0 * t) * (0.45 + beat)
                                                  + 0.20 * std::sin(2.0 * pi * 233.0 * t)
                                                  + 0.10 * std::sin(2.0 * pi * 3100.0 * t));
            const float right = static_cast<float>(0.40 * std::sin(2.0 * pi * 61.0 * t) * (0.45 + beat)
                                                   + 0.22 * std::sin(2.0 * pi * 311.0 * t)
                                                   + 0.09 * std::sin(2.0 * pi * 4200.0 * t));
            pcm[i * 2] = left;
            pcm[i * 2 + 1] = right;
        }
        phase += samplesPerFrame / static_cast<double>(sampleRate);
        for (unsigned int i = 0; i < projectmFrames; ++i) {
            const float position = (samplesPerFrame - 1.0f) * i
                                 / std::max(1.0f, projectmFrames - 1.0f);
            const unsigned int leftIndex = static_cast<unsigned int>(position);
            const unsigned int rightIndex = std::min<unsigned int>(leftIndex + 1,
                                                                    samplesPerFrame - 1);
            const float fraction = position - leftIndex;
            for (unsigned int channel = 0; channel < 2; ++channel) {
                projectmPcm[i * 2 + channel]
                    = pcm[leftIndex * 2 + channel] * (1.0f - fraction)
                    + pcm[rightIndex * 2 + channel] * fraction;
            }
        }
        projectm_pcm_add_float(projectm, projectmPcm.data(), projectmFrames, PROJECTM_STEREO);
        projectm_opengl_render_frame(projectm);
        glFinish();
        glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        const auto currentNative = flipRgb(rgba);
        const auto currentAscii = braillePass(currentNative, asciiExposure);
        const auto currentNativeCoarse = coarseLuminance(currentNative);
        const auto currentAsciiCoarse = coarseLuminance(currentAscii);
        if (frame >= fps * 2 && !previousNative.empty()) {
            const int beatFrame = frame % fps;
            if (beatFrame >= 12 && beatFrame <= 20) {
                nativeHitMotion += meanDifference(previousNative, currentNative);
                asciiHitMotion += meanDifference(previousAscii, currentAscii);
                nativeCoarseHitMotion += meanDifference(previousNativeCoarse,
                                                         currentNativeCoarse);
                asciiCoarseHitMotion += meanDifference(previousAsciiCoarse,
                                                        currentAsciiCoarse);
                ++hitFrames;
            } else if (beatFrame >= 35 && beatFrame <= 55) {
                nativeIdleMotion += meanDifference(previousNative, currentNative);
                asciiIdleMotion += meanDifference(previousAscii, currentAscii);
                nativeCoarseIdleMotion += meanDifference(previousNativeCoarse,
                                                          currentNativeCoarse);
                asciiCoarseIdleMotion += meanDifference(previousAsciiCoarse,
                                                         currentAsciiCoarse);
                ++idleFrames;
            }
        }
        previousNative = currentNative;
        previousAscii = currentAscii;
        previousNativeCoarse = currentNativeCoarse;
        previousAsciiCoarse = currentAsciiCoarse;
    }

    const auto native = flipRgb(rgba);
    writePpm(argv[2], native);
    writePpm(argv[3], braillePass(native, asciiExposure));
    const double nativeHit = nativeHitMotion / std::max(1, hitFrames);
    const double nativeIdle = nativeIdleMotion / std::max(1, idleFrames);
    const double asciiHit = asciiHitMotion / std::max(1, hitFrames);
    const double asciiIdle = asciiIdleMotion / std::max(1, idleFrames);
    const double nativeCoarseHit = nativeCoarseHitMotion / std::max(1, hitFrames);
    const double nativeCoarseIdle = nativeCoarseIdleMotion / std::max(1, idleFrames);
    const double asciiCoarseHit = asciiCoarseHitMotion / std::max(1, hitFrames);
    const double asciiCoarseIdle = asciiCoarseIdleMotion / std::max(1, idleFrames);
    std::cout << "motion native=" << nativeHit / std::max(1e-9, nativeIdle)
              << " ascii=" << asciiHit / std::max(1e-9, asciiIdle)
              << " retention=" << (asciiHit / std::max(1e-9, asciiIdle))
                                      / std::max(1e-9, nativeHit / std::max(1e-9, nativeIdle))
              << " coarse_native=" << nativeCoarseHit / std::max(1e-9, nativeCoarseIdle)
              << " coarse_ascii=" << asciiCoarseHit / std::max(1e-9, asciiCoarseIdle)
              << " idle_native=" << nativeCoarseIdle
              << " idle_ascii=" << asciiCoarseIdle
              << "\n";

    projectm_destroy(projectm);
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
