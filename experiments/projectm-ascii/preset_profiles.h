#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

enum class PresetTopology : uint8_t {
    Tunnel, Crystal, Radial, Organic, Interference, Multipole
};
enum class PresetDirection : uint8_t { Inward, Outward, Orbit, Oscillate };
enum class PresetEnergy : uint8_t { Calm, Medium, Driving };

struct PresetProfile {
    std::string_view match;
    PresetTopology topology;
    PresetDirection direction;
    PresetEnergy energy;
    uint16_t dwellMinMs;
    uint16_t dwellMaxMs;
    float asciiDensity;
    float reactionGain;
};

inline constexpr std::array<PresetProfile, 5> presetProfiles{{
    {"Contortion (Escher's Tunnel Mix)", PresetTopology::Tunnel,
     PresetDirection::Inward, PresetEnergy::Driving, 10000, 14000, 0.46f, 1.00f},
    {"Bitterfeld (Crystal Border Mix)", PresetTopology::Crystal,
     PresetDirection::Orbit, PresetEnergy::Driving, 9000, 13000, 0.55f, 1.15f},
    {"Halls Of Centrifuge", PresetTopology::Radial,
     PresetDirection::Orbit, PresetEnergy::Driving, 9000, 12000, 0.48f, 1.18f},
    {"Songflower (Hybrid Plant)", PresetTopology::Organic,
     PresetDirection::Oscillate, PresetEnergy::Medium, 12000, 16000, 0.52f, 0.86f},
    {"Morat's Final Voyage", PresetTopology::Interference,
     PresetDirection::Oscillate, PresetEnergy::Medium, 10000, 14000, 0.58f, 1.04f},
}};

inline const PresetProfile& profileForPreset(const std::string& filename) {
    for (const auto& profile : presetProfiles) {
        if (filename.find(profile.match) != std::string::npos) return profile;
    }
    return presetProfiles.front();
}

inline int topologyFamily(PresetTopology topology) {
    switch (topology) {
        case PresetTopology::Tunnel:
        case PresetTopology::Radial:
        case PresetTopology::Multipole: return 0;
        case PresetTopology::Crystal:
        case PresetTopology::Interference: return 1;
        case PresetTopology::Organic: return 2;
    }
    return 0;
}

inline int reactionModeForTopology(PresetTopology topology) {
    switch (topology) {
        case PresetTopology::Tunnel:
        case PresetTopology::Radial: return 0;
        case PresetTopology::Crystal:
        case PresetTopology::Interference: return 1;
        case PresetTopology::Organic: return 2;
        case PresetTopology::Multipole: return 3;
    }
    return 0;
}

inline bool directionsCompatible(PresetDirection a, PresetDirection b) {
    if (a == b) return true;
    return (a == PresetDirection::Inward && b == PresetDirection::Outward)
        || (a == PresetDirection::Outward && b == PresetDirection::Inward);
}
