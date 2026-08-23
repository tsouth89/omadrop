#include "preset_profiles.h"

#include <array>
#include <cassert>
#include <iostream>
#include <string>

int main() {
    const std::array<std::string, 5> curated{
        "Aderrasi - Contortion (Escher's Tunnel Mix).milk",
        "Aderrasi - Bitterfeld (Crystal Border Mix).milk",
        "Aderrasi - Halls Of Centrifuge.milk",
        "Aderrasi - Songflower (Hybrid Plant).milk",
        "Unchained - Morat's Final Voyage.milk",
    };

    for (std::size_t i = 0; i < curated.size(); ++i) {
        const PresetProfile& profile = profileForPreset(curated[i]);
        assert(profile.match == presetProfiles[i].match);
        assert(profile.dwellMinMs >= 9000);
        assert(profile.dwellMaxMs > profile.dwellMinMs);
        assert(profile.asciiDensity >= 0.4f && profile.asciiDensity <= 0.6f);
        assert(profile.reactionGain >= 0.8f && profile.reactionGain <= 1.25f);
        assert(reactionModeForTopology(profile.topology) >= 0);
        assert(reactionModeForTopology(profile.topology) <= 3);
    }
    assert(directionsCompatible(PresetDirection::Inward, PresetDirection::Outward));
    assert(!directionsCompatible(PresetDirection::Orbit, PresetDirection::Oscillate));
    std::cout << "preset profiles passed\n";
}
