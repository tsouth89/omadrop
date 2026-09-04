#include "audio_features.h"
#include "music_frame.h"
#include "musical_structure.h"
#include "native_renderer.h"
#include "native_scene_state.h"

#include <GL/glew.h>
#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int width = 480;
constexpr int height = 270;

std::vector<float> readTexture(GLuint texture) {
    std::vector<float> pixels(width * height * 4);
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_FLOAT, pixels.data());
    return pixels;
}

bool writePpm(const std::filesystem::path& path,
              const std::vector<float>& source,
              const std::vector<float>& incoming, float transition,
              float exposure) {
    std::ofstream output(path, std::ios::binary);
    if (!output) return false;
    output << "P6\n" << width << " " << height << "\n255\n";
    const float mix = std::clamp(transition, 0.0f, 1.0f);
    for (int y = height - 1; y >= 0; --y) {
        for (int x = 0; x < width; ++x) {
            const std::size_t offset = static_cast<std::size_t>(y * width + x) * 4;
            for (int channel = 0; channel < 3; ++channel) {
                const float linear = source[offset + channel] * (1.0f - mix)
                                   + incoming[offset + channel] * mix;
                const float mapped = linear / (1.0f + linear * 0.85f);
                const float exposed = std::clamp(mapped * exposure, 0.0f, 1.0f);
                const auto value = static_cast<unsigned char>(
                    std::pow(exposed, 1.0f / 2.2f) * 255.0f + 0.5f);
                output.write(reinterpret_cast<const char*>(&value), 1);
            }
        }
    }
    return static_cast<bool>(output);
}

std::string sceneSlug(NativeSceneKind scene) {
    switch (scene) {
        case NativeSceneKind::DepthTunnel: return "depth";
        case NativeSceneKind::Centrifuge: return "centrifuge";
        case NativeSceneKind::WireOrganism: return "wire";
        case NativeSceneKind::PrismGarden: return "prism";
        case NativeSceneKind::OrbitalLoom: return "orbital";
        case NativeSceneKind::TidalGrid: return "tidal";
        case NativeSceneKind::PulseCathedral: return "cathedral";
        case NativeSceneKind::ConstellationField: return "constellation";
        case NativeSceneKind::SpectralRibbons: return "ribbons";
        case NativeSceneKind::BloomEngine: return "bloom";
    }
    return "unknown";
}

float meanFrameMotion(const std::vector<float>& before,
                      const std::vector<float>& after) {
    if (before.size() != after.size() || before.empty()) return 0.0f;
    float total = 0.0f;
    for (std::size_t index = 0; index < before.size(); index += 4) {
        const float beforeLight = 0.299f * before[index]
                                + 0.587f * before[index + 1]
                                + 0.114f * before[index + 2];
        const float afterLight = 0.299f * after[index]
                               + 0.587f * after[index + 1]
                               + 0.114f * after[index + 2];
        total += std::abs(afterLight - beforeLight);
    }
    return total / static_cast<float>(before.size() / 4);
}

struct MotionBucket {
    float total = 0.0f;
    int count = 0;
    void add(float motion) { total += motion; ++count; }
    float mean() const { return count > 0 ? total / count : 0.0f; }
};
}

int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "usage: native-song-replay SHADER_DIRECTORY "
                     "RAW_F32_STEREO OUTPUT_DIRECTORY\n";
        return 2;
    }
    std::ifstream input(argv[2], std::ios::binary);
    if (!input) {
        std::cerr << "could not open: " << argv[2] << "\n";
        return 1;
    }
    const std::filesystem::path outputDirectory = argv[3];
    std::filesystem::create_directories(outputDirectory);
    std::ofstream timeline(outputDirectory / "timeline.tsv");
    timeline << "seconds\tscene\tkick\tsnare\that\tonset_pulse\tbeat_pulse\tbeat_phase"
                "\tbar\tsection\tflux_sub\tflux_bass\tflux_low_mid\tflux_mid"
                "\tflux_presence\tflux_high\tmotion\n";

    if (SDL_Init(SDL_INIT_VIDEO) != 0) return 1;
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_Window* window = SDL_CreateWindow("Omadrop native song replay", 0, 0,
        width, height, SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
    if (!window) return 1;
    SDL_GLContext context = SDL_GL_CreateContext(window);
    if (!context) return 1;
    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) return 1;

    NativeRenderer renderer;
    std::string error;
    if (!renderer.initialize(argv[1], error)) {
        std::cerr << error << "\n";
        return 1;
    }
    AudioFeatureBus bus;
    MusicalStructureTracker structureTracker;
    MusicFrameBuilder musicFrameBuilder;
    NativeSceneDirector sceneDirector;
    bool fixedScene = false;
    if (const char* requestedScene = std::getenv("OMADROP_REPLAY_SCENE")) {
        NativeSceneKind selectedScene;
        if (!nativeSceneFromName(requestedScene, selectedScene)) {
            std::cerr << "unknown OMADROP_REPLAY_SCENE: "
                      << requestedScene << "\n";
            return 2;
        }
        sceneDirector.selectScene(selectedScene);
        fixedScene = true;
    }
    const bool measureMotion = std::getenv("OMADROP_REPLAY_MEASURE") != nullptr;
    std::vector<float> pcm(AudioFeatureBus::hopSize * 2);
    std::size_t hops = 0;
    int captures = 0;
    int sections = 0;
    NativeSceneKind reportedScene = sceneDirector.state().currentScene;
    std::vector<float> previousFrame;
    MotionBucket quietMotion;
    MotionBucket beatMotion;
    MotionBucket kickMotion;
    MotionBucket snareMotion;
    MotionBucket hatMotion;
    MotionBucket onsetMotion;
    float previousBeat = 0.0f;
    float previousKick = 0.0f;
    float previousSnare = 0.0f;
    float previousHat = 0.0f;
    float previousOnset = 0.0f;
    const std::array<float, 3> color{0.44f, 0.70f, 1.0f};
    const std::size_t captureInterval = std::getenv("OMADROP_REPLAY_CAPTURE_HOPS")
        ? static_cast<std::size_t>(std::max(
            1, std::atoi(std::getenv("OMADROP_REPLAY_CAPTURE_HOPS"))))
        : 120;
    const std::size_t maximumHops = std::getenv("OMADROP_REPLAY_MAX_SECONDS")
        ? static_cast<std::size_t>(std::max(
            1, std::atoi(std::getenv("OMADROP_REPLAY_MAX_SECONDS")))) * 60
        : std::numeric_limits<std::size_t>::max();

    while (hops < maximumHops
           && input.read(reinterpret_cast<char*>(pcm.data()),
                      static_cast<std::streamsize>(pcm.size() * sizeof(float)))) {
        const AudioFeatures features = bus.processStereo(
            pcm.data(), AudioFeatureBus::hopSize);
        const MusicalStructureState& structure = structureTracker.update(features);
        const MusicFrame& music = musicFrameBuilder.update(
            features, structure, 1.0f / 60.0f);
        const NativeSceneState& scene = sceneDirector.update(
            music, 1.0f / 60.0f, !fixedScene);
        if (!renderer.render(music, scene, width, height, color,
                             0, 1.0f, 1.0f / 60.0f, error)) {
            std::cerr << error << "\n";
            return 1;
        }

        const double seconds = hops * AudioFeatureBus::hopSize
                             / static_cast<double>(AudioFeatureBus::sampleRate);
        if (scene.currentScene != reportedScene) {
            reportedScene = scene.currentScene;
            std::cout << "scene " << seconds << " sec "
                      << nativeSceneName(reportedScene) << "\n";
        }
        if (structure.sectionCrossed) ++sections;

        float frameMotion = -1.0f;
        if (measureMotion) {
            std::vector<float> currentFrame = readTexture(
                renderer.texture(scene.currentScene));
            if (!previousFrame.empty()) {
                frameMotion = meanFrameMotion(previousFrame, currentFrame);
                if (music.beatPulse > previousBeat + 0.20f) beatMotion.add(frameMotion);
                if (music.kick > previousKick + 0.15f) kickMotion.add(frameMotion);
                if (music.snare > previousSnare + 0.15f) snareMotion.add(frameMotion);
                if (music.hat > previousHat + 0.15f) hatMotion.add(frameMotion);
                if (music.onsetPulse > previousOnset + 0.15f) {
                    onsetMotion.add(frameMotion);
                }
                if (music.beatPulse < 0.035f && music.kick < 0.035f
                    && music.snare < 0.035f && music.hat < 0.035f
                    && music.onsetPulse < 0.035f) {
                    quietMotion.add(frameMotion);
                }
            }
            previousFrame = std::move(currentFrame);
            previousBeat = music.beatPulse;
            previousKick = music.kick;
            previousSnare = music.snare;
            previousHat = music.hat;
            previousOnset = music.onsetPulse;
        }
        timeline << seconds << '\t' << nativeSceneName(scene.currentScene)
                 << '\t' << music.kick << '\t' << music.snare << '\t' << music.hat
                 << '\t' << music.onsetPulse << '\t' << music.beatPulse
                 << '\t' << music.beatPhase
                 << '\t' << music.barPhase << '\t' << music.section
                 << '\t' << music.bandFlux[0] << '\t' << music.bandFlux[1]
                 << '\t' << music.bandFlux[2] << '\t' << music.bandFlux[3]
                 << '\t' << music.bandFlux[4] << '\t' << music.bandFlux[5]
                 << '\t' << frameMotion << '\n';

        const bool periodicCapture = hops % captureInterval == 0;
        const bool structuralCapture = structure.sectionCrossed;
        if (periodicCapture || structuralCapture) {
            const std::vector<float> source = readTexture(
                renderer.texture(scene.currentScene));
            const std::vector<float> incoming = scene.transitioning
                ? readTexture(renderer.texture(scene.incomingScene)) : source;
            std::ostringstream filename;
            filename << std::setw(6) << std::setfill('0') << hops << '-'
                     << sceneSlug(scene.currentScene)
                     << (structuralCapture ? "-section" : "") << ".ppm";
            const NativeSceneMaterial sourceMaterial
                = nativeSceneMaterial(scene.currentScene);
            const NativeSceneMaterial incomingMaterial = nativeSceneMaterial(
                scene.transitioning ? scene.incomingScene : scene.currentScene);
            const float transition = scene.transitioning ? scene.transition : 0.0f;
            const float exposure = sourceMaterial.fieldExposure * (1.0f - transition)
                                 + incomingMaterial.fieldExposure * transition;
            if (!writePpm(outputDirectory / filename.str(), source, incoming,
                          transition, exposure)) {
                std::cerr << "could not write replay frame\n";
                return 1;
            }
            ++captures;
        }
        ++hops;
    }

    renderer.shutdown();
    SDL_GL_DeleteContext(context);
    SDL_DestroyWindow(window);
    SDL_Quit();
    const double duration = hops * AudioFeatureBus::hopSize
                          / static_cast<double>(AudioFeatureBus::sampleRate);
    std::cout << "native song replay " << duration << " sec, " << captures
              << " frames, " << sections << " sections\n";
    if (measureMotion) {
        const float quiet = std::max(1e-7f, quietMotion.mean());
        std::cout << "motion " << nativeSceneName(sceneDirector.state().currentScene)
                  << " quiet=" << quietMotion.mean() << " (" << quietMotion.count
                  << ") beat=" << beatMotion.mean() / quiet << "x ("
                  << beatMotion.count << ") kick=" << kickMotion.mean() / quiet
                  << "x (" << kickMotion.count << ") snare="
                  << snareMotion.mean() / quiet << "x (" << snareMotion.count
                  << ") hat=" << hatMotion.mean() / quiet << "x ("
                  << hatMotion.count << ") onset=" << onsetMotion.mean() / quiet
                  << "x (" << onsetMotion.count << ")\n";
    }
    return hops > 0 && captures > 0 ? 0 : 1;
}
