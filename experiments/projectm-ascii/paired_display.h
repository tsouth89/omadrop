#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <sstream>
#include <string>

struct PairedDisplayState {
    std::uint64_t serial = 0;
    std::size_t presetIndex = 0;
    std::uint64_t durationMs = 0;
    int transitionMode = 0;
    bool hardSync = false;
    int nativeScene = -1;
    int nativeSourceScene = -1;
    int asciiMode = -1;
    int fullscreenMode = -1;
    int syncDelayMs = -1;
    int closeMode = -1;
};

inline std::string encodePairedDisplayState(const PairedDisplayState& state) {
    std::ostringstream output;
    output << state.serial << ' ' << state.presetIndex << ' ' << state.durationMs
           << ' ' << state.transitionMode << ' ' << state.hardSync
           << ' ' << state.nativeScene << ' ' << state.nativeSourceScene
           << ' ' << state.asciiMode << ' ' << state.fullscreenMode
           << ' ' << state.syncDelayMs << ' ' << state.closeMode << '\n';
    return output.str();
}

inline std::optional<PairedDisplayState> decodePairedDisplayState(
    const std::string& input, std::size_t presetCount,
    std::size_t nativeSceneCount = 0) {
    PairedDisplayState state;
    int hardSync = 0;
    std::istringstream stream(input);
    if (!(stream >> state.serial >> state.presetIndex >> state.durationMs
          >> state.transitionMode >> hardSync)
        || state.serial == 0 || state.presetIndex >= presetCount
        || state.transitionMode < 0
        || (state.transitionMode > 3 && state.transitionMode != 6)
        || (hardSync != 0 && hardSync != 1)) return std::nullopt;
    state.hardSync = hardSync == 1;
    int nativeScene = -1;
    if (stream >> nativeScene) {
        if (nativeScene < -1
            || (nativeScene >= 0
                && (nativeSceneCount == 0
                    || static_cast<std::size_t>(nativeScene) >= nativeSceneCount))) {
            return std::nullopt;
        }
        state.nativeScene = nativeScene;
        int nativeSourceScene = -1;
        if (stream >> nativeSourceScene) {
            if (nativeSourceScene < -1
                || (nativeSourceScene >= 0
                    && (nativeSceneCount == 0
                        || static_cast<std::size_t>(nativeSourceScene)
                           >= nativeSceneCount))) {
                return std::nullopt;
            }
            state.nativeSourceScene = nativeSourceScene;
            int asciiMode = -1;
            int fullscreenMode = -1;
            int syncDelayMs = -1;
            int closeMode = -1;
            if (stream >> asciiMode >> fullscreenMode >> syncDelayMs) {
                stream >> closeMode;
                if ((asciiMode < -1 || asciiMode > 1)
                    || (fullscreenMode < -1 || fullscreenMode > 1)
                    || syncDelayMs < -1 || syncDelayMs > 500
                    || closeMode < -1 || closeMode > 1) {
                    return std::nullopt;
                }
                state.asciiMode = asciiMode;
                state.fullscreenMode = fullscreenMode;
                state.syncDelayMs = syncDelayMs;
                state.closeMode = closeMode;
            }
        }
    }
    return state;
}

class PairedDisplayFollower {
public:
    std::optional<PairedDisplayState> consume(const std::string& input,
                                              std::size_t presetCount,
                                              std::size_t nativeSceneCount = 0) {
        const auto state = decodePairedDisplayState(
            input, presetCount, nativeSceneCount);
        if (!state || state->serial <= lastSerial_) return std::nullopt;
        lastSerial_ = state->serial;
        return state;
    }

private:
    std::uint64_t lastSerial_ = 0;
};
