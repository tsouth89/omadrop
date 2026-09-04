#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

enum class VisualFamily : uint8_t {
    DepthCorridor,
    MultipoleWire,
    RadialOrnament,
    InterferencePlanes,
    CrystalLattice,
    OrganicTendrils,
};
enum class BridgeGroup : uint8_t { Spatial, Linework, Organic };
enum class PresetDirection : uint8_t { Inward, Outward, Orbit, Oscillate };
enum class PresetEnergy : uint8_t { Calm, Medium, Driving };

struct PresetProfile {
    std::string_view filename;
    VisualFamily family;
    BridgeGroup bridge;
    PresetDirection direction;
    PresetEnergy energy;
    uint16_t dwellMinMs;
    uint16_t dwellMaxMs;
    float asciiDensity;
    float asciiExposure;
    float kickGain;
    float snareGain;
    float hatGain;
};

inline constexpr std::array<PresetProfile, 11> presetProfiles{{
    {"Omadrop + Aderrasi - Contortion (Reactive Tunnel Edition).milk",
     VisualFamily::DepthCorridor, BridgeGroup::Spatial,
     PresetDirection::Inward, PresetEnergy::Driving,
     12000, 18000, 0.46f, 1.22f, 1.08f, 0.92f, 0.72f},
    {"Omadrop + Martin - Wire Dance (Reactive Wire Edition).milk",
     VisualFamily::MultipoleWire, BridgeGroup::Organic,
     PresetDirection::Orbit, PresetEnergy::Driving,
     12000, 18000, 0.50f, 0.92f, 1.10f, 1.02f, 0.86f},
    {"Omadrop + Aderrasi - Halls Of Centrifuge (Reactive Orbit Edition).milk",
     VisualFamily::RadialOrnament, BridgeGroup::Spatial,
     PresetDirection::Orbit, PresetEnergy::Driving,
     12000, 18000, 0.48f, 1.38f, 1.16f, 1.04f, 0.88f},
    {"martin - night cathedral.milk",
     VisualFamily::InterferencePlanes, BridgeGroup::Linework,
     PresetDirection::Inward, PresetEnergy::Medium,
     11000, 15000, 0.50f, 0.94f, 1.10f, 1.02f, 0.70f},
    {"Aderrasi - Bitterfeld (Crystal Border Mix).milk",
     VisualFamily::CrystalLattice, BridgeGroup::Linework,
     PresetDirection::Orbit, PresetEnergy::Driving,
     9000, 13000, 0.55f, 1.05f, 1.15f, 0.92f, 0.82f},
    {"Aderrasi + Geiss - Airhandler (Kali Mix) - Painterly Kaleidoscope 2.milk",
     VisualFamily::OrganicTendrils, BridgeGroup::Organic,
     PresetDirection::Oscillate, PresetEnergy::Medium,
     11000, 15000, 0.52f, 1.18f, 0.98f, 1.08f, 0.90f},
    {"Tokyo corridor (shifter tumbling cubes remix).milk",
     VisualFamily::DepthCorridor, BridgeGroup::Spatial,
     PresetDirection::Outward, PresetEnergy::Driving,
     10000, 14000, 0.50f, 0.96f, 1.04f, 0.86f, 0.66f},
    {"Unchained & Rovastar - Wormhole Pillars (Hall of Shadows mix).milk",
     VisualFamily::DepthCorridor, BridgeGroup::Spatial,
     PresetDirection::Inward, PresetEnergy::Driving,
     10000, 14000, 0.52f, 0.90f, 1.08f, 0.84f, 0.62f},
    {"Rovastar - VooV's Organic Light.milk",
     VisualFamily::OrganicTendrils, BridgeGroup::Organic,
     PresetDirection::Inward, PresetEnergy::Calm,
     12000, 16000, 0.48f, 1.14f, 0.92f, 0.92f, 0.72f},
    {"fiShbRaiN - crystal glasses.milk",
     VisualFamily::RadialOrnament, BridgeGroup::Spatial,
     PresetDirection::Orbit, PresetEnergy::Driving,
     10000, 14000, 0.50f, 1.08f, 1.10f, 0.90f, 0.78f},
    {"Krash & Rovastar - Cerebral Demons - Phat + EoS Moire Remix.milk",
     VisualFamily::InterferencePlanes, BridgeGroup::Linework,
     PresetDirection::Oscillate, PresetEnergy::Medium,
     11000, 15000, 0.48f, 1.12f, 0.96f, 1.04f, 0.80f},
}};

inline std::string_view presetBasename(std::string_view path) {
    const auto slash = path.find_last_of("/\\");
    return slash == std::string_view::npos ? path : path.substr(slash + 1);
}

inline const PresetProfile* findProfileForPreset(std::string_view path) {
    const std::string_view filename = presetBasename(path);
    if (filename == "Aderrasi - Contortion (Escher's Tunnel Mix).milk") {
        return &presetProfiles.front();
    }
    if (filename == "Aderrasi - Halls Of Centrifuge.milk") {
        return &presetProfiles[2];
    }
    if (filename == "Martin - wire dance.milk") {
        return &presetProfiles[1];
    }
    for (const auto& profile : presetProfiles) {
        if (filename == profile.filename) return &profile;
    }
    return nullptr;
}

inline const PresetProfile& profileForPreset(std::string_view path) {
    if (const auto* profile = findProfileForPreset(path)) return *profile;
    throw std::invalid_argument("unprofiled preset: " + std::string(presetBasename(path)));
}

inline int visualFamily(const PresetProfile& profile) {
    return static_cast<int>(profile.family);
}

inline int reactionMode(const PresetProfile& profile) {
    return visualFamily(profile);
}

inline bool directionsCompatible(PresetDirection a, PresetDirection b) {
    if (a == b) return true;
    return (a == PresetDirection::Inward && b == PresetDirection::Outward)
        || (a == PresetDirection::Outward && b == PresetDirection::Inward);
}
