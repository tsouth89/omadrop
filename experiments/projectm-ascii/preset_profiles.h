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
enum class PresetReaction : uint8_t {
    TunnelBreath,
    WireLobes,
    CentrifugeDepth,
    CathedralPlanes,
    CrystalFracture,
    AirhandlerTendrils,
};

struct PresetProfile {
    std::string_view match;
    PresetTopology topology;
    PresetDirection direction;
    PresetEnergy energy;
    PresetReaction reaction;
    uint16_t dwellMinMs;
    uint16_t dwellMaxMs;
    float asciiDensity;
    float asciiExposure;
    float kickGain;
    float snareGain;
    float hatGain;
};

inline constexpr std::array<PresetProfile, 6> presetProfiles{{
    {"Contortion (Escher's Tunnel Mix)", PresetTopology::Tunnel,
     PresetDirection::Inward, PresetEnergy::Driving, PresetReaction::TunnelBreath,
     10000, 14000, 0.46f, 1.16f, 1.00f, 0.82f, 0.56f},
    {"wire dance", PresetTopology::Multipole,
     PresetDirection::Orbit, PresetEnergy::Driving, PresetReaction::WireLobes,
     10000, 14000, 0.50f, 0.88f, 1.16f, 0.96f, 0.74f},
    {"Halls Of Centrifuge", PresetTopology::Radial,
     PresetDirection::Orbit, PresetEnergy::Driving, PresetReaction::CentrifugeDepth,
     9000, 12000, 0.48f, 1.42f, 1.22f, 0.88f, 0.62f},
    {"night cathedral", PresetTopology::Interference,
     PresetDirection::Inward, PresetEnergy::Medium, PresetReaction::CathedralPlanes,
     11000, 15000, 0.50f, 0.94f, 1.10f, 1.02f, 0.70f},
    {"Bitterfeld (Crystal Border Mix)", PresetTopology::Crystal,
     PresetDirection::Orbit, PresetEnergy::Driving, PresetReaction::CrystalFracture,
     9000, 13000, 0.55f, 1.05f, 1.15f, 0.92f, 0.82f},
    {"Airhandler (Kali Mix) - Painterly Kaleidoscope 2", PresetTopology::Organic,
     PresetDirection::Oscillate, PresetEnergy::Medium, PresetReaction::AirhandlerTendrils,
     11000, 15000, 0.52f, 1.18f, 0.98f, 1.08f, 0.90f},
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

inline int reactionMode(const PresetProfile& profile) {
    return static_cast<int>(profile.reaction);
}

inline bool directionsCompatible(PresetDirection a, PresetDirection b) {
    if (a == b) return true;
    return (a == PresetDirection::Inward && b == PresetDirection::Outward)
        || (a == PresetDirection::Outward && b == PresetDirection::Inward);
}
