#pragma once

#include "music_frame.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <optional>
#include <string_view>
#include <unordered_map>

enum class NativeSceneKind : std::uint8_t {
    DepthTunnel = 0,
    Centrifuge = 1,
    WireOrganism = 2,
    PrismGarden = 3,
    OrbitalLoom = 4,
    TidalGrid = 5,
    PulseCathedral = 6,
    ConstellationField = 7,
    SpectralRibbons = 8,
    BloomEngine = 9,
};

inline constexpr std::size_t nativeSceneCount = 10;

inline const char* nativeSceneName(NativeSceneKind scene) {
    switch (scene) {
        case NativeSceneKind::DepthTunnel: return "Depth Tunnel";
        case NativeSceneKind::Centrifuge: return "Centrifuge";
        case NativeSceneKind::WireOrganism: return "Wire Organism";
        case NativeSceneKind::PrismGarden: return "Prism Garden";
        case NativeSceneKind::OrbitalLoom: return "Orbital Loom";
        case NativeSceneKind::TidalGrid: return "Tidal Grid";
        case NativeSceneKind::PulseCathedral: return "Pulse Cathedral";
        case NativeSceneKind::ConstellationField: return "Constellation Field";
        case NativeSceneKind::SpectralRibbons: return "Spectral Ribbons";
        case NativeSceneKind::BloomEngine: return "Bloom Engine";
    }
    return "Unknown";
}

inline NativeSceneKind nativeSceneOffset(NativeSceneKind scene, int direction) {
    const int count = static_cast<int>(nativeSceneCount);
    const int index = static_cast<int>(scene);
    return static_cast<NativeSceneKind>((index + direction % count + count) % count);
}

inline bool nativeSceneFromName(std::string_view name, NativeSceneKind& scene) {
    if (name == "depth" || name == "tunnel" || name == "depth-tunnel") {
        scene = NativeSceneKind::DepthTunnel;
        return true;
    }
    if (name == "centrifuge") {
        scene = NativeSceneKind::Centrifuge;
        return true;
    }
    if (name == "wire" || name == "wire-organism") {
        scene = NativeSceneKind::WireOrganism;
        return true;
    }
    if (name == "prism" || name == "garden" || name == "prism-garden") {
        scene = NativeSceneKind::PrismGarden;
        return true;
    }
    if (name == "orbit" || name == "loom" || name == "orbital-loom") {
        scene = NativeSceneKind::OrbitalLoom;
        return true;
    }
    if (name == "tide" || name == "grid" || name == "tidal-grid") {
        scene = NativeSceneKind::TidalGrid;
        return true;
    }
    if (name == "cathedral" || name == "pulse-cathedral") {
        scene = NativeSceneKind::PulseCathedral;
        return true;
    }
    if (name == "stars" || name == "constellation"
        || name == "constellation-field") {
        scene = NativeSceneKind::ConstellationField;
        return true;
    }
    if (name == "ribbons" || name == "spectral-ribbons") {
        scene = NativeSceneKind::SpectralRibbons;
        return true;
    }
    if (name == "bloom" || name == "bloom-engine") {
        scene = NativeSceneKind::BloomEngine;
        return true;
    }
    return false;
}

struct NativeSceneMaterial {
    float fieldExposure;
    float asciiExposure;
};

inline NativeSceneMaterial nativeSceneMaterial(NativeSceneKind scene) {
    switch (scene) {
        case NativeSceneKind::DepthTunnel: return {1.05f, 1.08f};
        case NativeSceneKind::Centrifuge: return {0.92f, 1.00f};
        case NativeSceneKind::WireOrganism: return {1.26f, 1.16f};
        case NativeSceneKind::PrismGarden: return {1.10f, 1.08f};
        case NativeSceneKind::OrbitalLoom: return {1.02f, 1.06f};
        case NativeSceneKind::TidalGrid: return {1.12f, 1.10f};
        case NativeSceneKind::PulseCathedral: return {1.08f, 1.05f};
        case NativeSceneKind::ConstellationField: return {1.22f, 1.16f};
        case NativeSceneKind::SpectralRibbons: return {1.04f, 1.06f};
        case NativeSceneKind::BloomEngine: return {1.00f, 1.04f};
    }
    return {1.0f, 1.0f};
}

struct NativeSceneState {
    float development = 0.0f;
    float drive = 0.0f;
    float peak = 0.0f;
    float release = 0.0f;
    float sceneBeats = 0.0f;
    NativeSceneKind currentScene = NativeSceneKind::DepthTunnel;
    NativeSceneKind incomingScene = NativeSceneKind::Centrifuge;
    float transition = 0.0f;
    bool transitioning = false;
    bool motifRecalled = false;
};

// Turns musical measurements into a slow scene lifecycle. The renderer gets
// deliberate compositional state instead of deriving long-form behavior from
// momentary audio levels.
class NativeSceneDirector {
public:
    const NativeSceneState& update(const MusicFrame& music, float seconds,
                                   bool allowAutomaticTransitions = true) {
        const float dt = std::clamp(seconds, 1.0f / 240.0f, 0.1f);
        const bool sectionStarted = music.section > 0.72f && previousSection_ <= 0.72f;
        const bool phraseStarted = previousPhrasePhase_ > 0.78f
                                && music.phrasePhase < 0.22f;
        const bool barStarted = previousBarPhase_ > 0.72f
                             && music.barPhase < 0.28f;
        const bool musicActive = music.energyFast > 0.018f
                              || music.percussive > 0.035f
                              || music.harmonic > 0.035f;
        const bool sectionTransition = sectionStarted && dwellBeats_ >= 16.0f;
        const bool phraseFallback = phraseStarted && dwellBeats_ >= 31.5f;
        const bool barFallback = barStarted && dwellBeats_ >= 47.5f;
        const bool maximumDwell = dwellBeats_ >= 64.0f;
        const bool automaticTransition = allowAutomaticTransitions
                                      && musicActive
                                      && (sectionTransition || phraseFallback
                                          || barFallback || maximumDwell);
        state_.motifRecalled = false;
        bool hasRecalledScene = false;
        NativeSceneKind recalledScene = state_.currentScene;
        if (sectionStarted && music.motifIdentity >= 0) {
            const auto remembered = motifScenes_.find(music.motifIdentity);
            if (remembered != motifScenes_.end()) {
                state_.motifRecalled = true;
                hasRecalledScene = true;
                recalledScene = remembered->second;
            }
        }
        const bool recallTransition = allowAutomaticTransitions && hasRecalledScene
                                   && recalledScene != state_.currentScene;
        if (pendingScene_ && *pendingScene_ == state_.currentScene
            && !state_.transitioning) {
            pendingScene_.reset();
        }
        std::optional<NativeSceneKind> automaticScene;
        if (automaticTransition) automaticScene = chooseAutomaticScene(music);
        if (!state_.transitioning
            && (pendingScene_ || pendingDirection_ != 0
                || recallTransition || automaticTransition)) {
            state_.incomingScene = pendingScene_ ? *pendingScene_
                : pendingDirection_ != 0
                ? nativeSceneOffset(state_.currentScene, pendingDirection_)
                : recallTransition ? recalledScene
                : *automaticScene;
            state_.transitioning = true;
            transitionElapsed_ = 0.0f;
            state_.sceneBeats = 0.0f;
            dwellBeats_ = 0.0f;
            transitionDuration_ = transitionSecondsOverride_ > 0.0f
                ? transitionSecondsOverride_
                : std::clamp(
                    4.0f * 60.0f / std::clamp(music.bpm, 60.0f, 190.0f),
                    2.0f, 5.0f);
            pendingDirection_ = 0;
            pendingScene_.reset();
            rememberSceneUse(state_.incomingScene);
        }
        if (sectionStarted && music.motifIdentity >= 0 && !hasRecalledScene) {
            motifScenes_[music.motifIdentity] = state_.transitioning
                ? state_.incomingScene : state_.currentScene;
        }
        const float elapsedBeats
            = dt * std::clamp(music.bpm, 60.0f, 190.0f) / 60.0f;
        state_.sceneBeats += elapsedBeats;
        if (!state_.transitioning) dwellBeats_ += elapsedBeats;

        const float developmentTarget = smoothstep(1.0f, 12.0f, state_.sceneBeats);
        const float driveTarget = smoothstep(0.28f, 0.62f,
            0.55f * music.energyFast + 0.45f * music.energySlow);
        const float peakTarget = smoothstep(0.58f, 0.84f, music.energyFast)
                               * smoothstep(0.18f, 0.62f, music.percussive);
        const float releaseTarget = smoothstep(0.025f, 0.22f, -music.energySlope);

        state_.development = smooth(state_.development, developmentTarget, 0.65f, dt);
        state_.drive = smooth(state_.drive, driveTarget, 1.2f, dt);
        state_.peak = smooth(state_.peak, peakTarget, 1.8f, dt);
        state_.release = smooth(state_.release, releaseTarget,
                                releaseTarget > state_.release ? 2.4f : 0.7f, dt);
        if (state_.transitioning) {
            transitionElapsed_ += dt;
            state_.transition = smoothstep(
                0.0f, 1.0f, transitionElapsed_ / transitionDuration_);
            if (transitionElapsed_ >= transitionDuration_) {
                state_.currentScene = state_.incomingScene;
                state_.incomingScene = nativeSceneOffset(state_.currentScene, 1);
                state_.transitioning = false;
                state_.transition = 0.0f;
                dwellBeats_ = 0.0f;
            }
        }
        previousSection_ = music.section;
        previousPhrasePhase_ = music.phrasePhase;
        previousBarPhase_ = music.barPhase;
        return state_;
    }

    void requestNext() { pendingDirection_ = 1; }
    void requestPrevious() { pendingDirection_ = -1; }
    void requestScene(NativeSceneKind scene) { pendingScene_ = scene; }
    void setTransitionDuration(float seconds) {
        transitionSecondsOverride_ = seconds > 0.0f
            ? std::clamp(seconds, 0.6f, 8.0f) : -1.0f;
    }
    const NativeSceneState& state() const { return state_; }
    void selectScene(NativeSceneKind scene) {
        state_ = NativeSceneState{};
        state_.currentScene = scene;
        state_.incomingScene = nativeSceneOffset(scene, 1);
        recentScenes_.clear();
        sceneUseCount_.fill(0);
        rememberSceneUse(scene);
        pendingDirection_ = 0;
        pendingScene_.reset();
        transitionElapsed_ = 0.0f;
        dwellBeats_ = 0.0f;
        previousSection_ = 0.0f;
        previousPhrasePhase_ = 0.0f;
        previousBarPhase_ = 0.0f;
    }
    void resetForTrack() {
        const NativeSceneKind retainedScene
            = state_.transitioning
                ? state_.incomingScene : state_.currentScene;
        const float retainedTransitionOverride = transitionSecondsOverride_;
        *this = NativeSceneDirector{};
        transitionSecondsOverride_ = retainedTransitionOverride;
        selectScene(retainedScene);
    }
    void reset() { *this = NativeSceneDirector{}; }

private:
    struct SceneTraits {
        float energy;
        float percussive;
        float harmonic;
        float centroid;
        float stereo;
    };

    static constexpr std::array<SceneTraits, nativeSceneCount> sceneTraits{{
        {0.72f, 0.58f, 0.42f, 0.24f, 0.34f}, // Depth Tunnel
        {0.82f, 0.86f, 0.28f, 0.66f, 0.44f}, // Centrifuge
        {0.46f, 0.34f, 0.82f, 0.44f, 0.62f}, // Wire Organism
        {0.54f, 0.28f, 0.88f, 0.78f, 0.48f}, // Prism Garden
        {0.60f, 0.44f, 0.74f, 0.54f, 0.92f}, // Orbital Loom
        {0.34f, 0.24f, 0.84f, 0.20f, 0.66f}, // Tidal Grid
        {0.48f, 0.26f, 0.96f, 0.36f, 0.34f}, // Pulse Cathedral
        {0.24f, 0.18f, 0.74f, 0.72f, 0.76f}, // Constellation Field
        {0.64f, 0.56f, 0.64f, 0.62f, 0.72f}, // Spectral Ribbons
        {0.74f, 0.66f, 0.66f, 0.48f, 0.54f}, // Bloom Engine
    }};

    NativeSceneKind chooseAutomaticScene(const MusicFrame& music) const {
        const float energy = std::clamp(
            0.58f * music.energyFast + 0.42f * music.energySlow, 0.0f, 1.0f);
        const float percussive = std::clamp(music.percussive, 0.0f, 1.0f);
        const float harmonic = std::clamp(music.harmonic, 0.0f, 1.0f);
        const float centroid = std::clamp(music.spectralCentroid, 0.0f, 1.0f);
        const float stereo = std::clamp(music.stereoWidth, 0.0f, 1.0f);
        NativeSceneKind best = nativeSceneOffset(state_.currentScene, 1);
        float bestScore = std::numeric_limits<float>::max();
        for (std::size_t index = 0; index < nativeSceneCount; ++index) {
            const NativeSceneKind candidate = static_cast<NativeSceneKind>(index);
            if (candidate == state_.currentScene) continue;
            const SceneTraits& traits = sceneTraits[index];
            const auto square = [](float value) { return value * value; };
            float score = 1.55f * square(energy - traits.energy)
                        + 1.30f * square(percussive - traits.percussive)
                        + 1.10f * square(harmonic - traits.harmonic)
                        + 0.72f * square(centroid - traits.centroid)
                        + 0.55f * square(stereo - traits.stereo)
                        + 0.055f * sceneUseCount_[index];
            for (std::size_t age = 0; age < recentScenes_.size(); ++age) {
                if (recentScenes_[recentScenes_.size() - 1 - age] == candidate) {
                    score += age == 0 ? 4.0f : age == 1 ? 1.5f : 0.55f;
                    break;
                }
            }
            if (score < bestScore) {
                bestScore = score;
                best = candidate;
            }
        }
        return best;
    }

    void rememberSceneUse(NativeSceneKind scene) {
        recentScenes_.push_back(scene);
        while (recentScenes_.size() > 4) recentScenes_.pop_front();
        ++sceneUseCount_[static_cast<std::size_t>(scene)];
    }

    static float smooth(float current, float target, float speed, float seconds) {
        return current + (target - current) * (1.0f - std::exp(-speed * seconds));
    }

    static float smoothstep(float lower, float upper, float value) {
        const float position = std::clamp((value - lower) / (upper - lower), 0.0f, 1.0f);
        return position * position * (3.0f - 2.0f * position);
    }

    NativeSceneState state_{};
    float previousSection_ = 0.0f;
    float previousPhrasePhase_ = 0.0f;
    float previousBarPhase_ = 0.0f;
    float dwellBeats_ = 0.0f;
    int pendingDirection_ = 0;
    std::optional<NativeSceneKind> pendingScene_;
    float transitionElapsed_ = 0.0f;
    float transitionDuration_ = 4.0f;
    float transitionSecondsOverride_ = -1.0f;
    std::unordered_map<int, NativeSceneKind> motifScenes_;
    std::deque<NativeSceneKind> recentScenes_;
    std::array<unsigned int, nativeSceneCount> sceneUseCount_{};
};
