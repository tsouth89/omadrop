#include "preset_profiles.h"

#include <array>
#include <cassert>
#include <iostream>
#include <string>

int main() {
    const std::array<std::string, 6> curated{
        "Aderrasi - Contortion (Escher's Tunnel Mix).milk",
        "Martin - wire dance.milk",
        "Aderrasi - Halls Of Centrifuge.milk",
        "martin - night cathedral.milk",
        "Aderrasi - Bitterfeld (Crystal Border Mix).milk",
        "Aderrasi + Geiss - Airhandler (Kali Mix) - Painterly Kaleidoscope 2.milk",
    };

    for (std::size_t i = 0; i < curated.size(); ++i) {
        const PresetProfile& profile = profileForPreset(curated[i]);
        assert(profile.match == presetProfiles[i].match);
        assert(profile.dwellMinMs >= 9000);
        assert(profile.dwellMaxMs > profile.dwellMinMs);
        assert(profile.asciiDensity >= 0.4f && profile.asciiDensity <= 0.6f);
        assert(profile.asciiExposure >= 0.65f && profile.asciiExposure <= 1.5f);
        assert(profile.kickGain >= 0.5f && profile.kickGain <= 1.3f);
        assert(profile.snareGain >= 0.5f && profile.snareGain <= 1.3f);
        assert(profile.hatGain >= 0.5f && profile.hatGain <= 1.3f);
        assert(reactionMode(profile) == static_cast<int>(i));
    }
    assert(directionsCompatible(PresetDirection::Inward, PresetDirection::Outward));
    assert(!directionsCompatible(PresetDirection::Orbit, PresetDirection::Oscillate));
    std::cout << "preset profiles passed\n";
}
