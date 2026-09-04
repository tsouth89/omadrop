#pragma once

#include "music_frame.h"

#include <cmath>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <type_traits>

struct PairedMusicState {
    std::uint32_t magic = 0x4f4d4d46u;
    std::uint32_t version = 3;
    std::uint64_t serial = 0;
    MusicFrame frame;
};

static_assert(std::is_trivially_copyable_v<MusicFrame>);
static_assert(std::is_trivially_copyable_v<PairedMusicState>);

inline std::string encodePairedMusicState(const PairedMusicState& state) {
    return std::string(reinterpret_cast<const char*>(&state), sizeof(state));
}

inline std::optional<PairedMusicState> decodePairedMusicState(
    const std::string& input) {
    if (input.size() != sizeof(PairedMusicState)) return std::nullopt;
    PairedMusicState state{};
    std::memcpy(&state, input.data(), sizeof(state));
    if (state.magic != 0x4f4d4d46u || state.version != 3 || state.serial == 0
        || !std::isfinite(state.frame.audioTimeSeconds)
        || !std::isfinite(state.frame.bpm)
        || state.frame.bpm < 0.0f) {
        return std::nullopt;
    }
    return state;
}

class PairedMusicFollower {
public:
    std::optional<MusicFrame> consume(const std::string& input) {
        const auto state = decodePairedMusicState(input);
        if (!state || state->serial <= lastSerial_) return std::nullopt;
        lastSerial_ = state->serial;
        return state->frame;
    }

private:
    std::uint64_t lastSerial_ = 0;
};
