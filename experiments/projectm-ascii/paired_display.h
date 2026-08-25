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
};

inline std::string encodePairedDisplayState(const PairedDisplayState& state) {
    std::ostringstream output;
    output << state.serial << ' ' << state.presetIndex << ' ' << state.durationMs
           << ' ' << state.transitionMode << ' ' << state.hardSync << '\n';
    return output.str();
}

inline std::optional<PairedDisplayState> decodePairedDisplayState(
    const std::string& input, std::size_t presetCount) {
    PairedDisplayState state;
    int hardSync = 0;
    std::istringstream stream(input);
    if (!(stream >> state.serial >> state.presetIndex >> state.durationMs
          >> state.transitionMode >> hardSync)
        || state.serial == 0 || state.presetIndex >= presetCount
        || state.transitionMode < 0 || state.transitionMode > 3
        || (hardSync != 0 && hardSync != 1)) return std::nullopt;
    state.hardSync = hardSync == 1;
    return state;
}

class PairedDisplayFollower {
public:
    std::optional<PairedDisplayState> consume(const std::string& input,
                                              std::size_t presetCount) {
        const auto state = decodePairedDisplayState(input, presetCount);
        if (!state || state->serial <= lastSerial_) return std::nullopt;
        lastSerial_ = state->serial;
        return state;
    }

private:
    std::uint64_t lastSerial_ = 0;
};
