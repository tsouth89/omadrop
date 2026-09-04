#include "preset_profiles.h"
#include "visual_motifs.h"

#include <array>
#include <cassert>
#include <filesystem>
#include <iostream>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>

int main(int argc, char** argv) {
    static_assert(presetProfiles.size() == 11);
    std::set<std::string_view> filenames;
    std::array<int, 6> familyCounts{};
    std::array<int, 3> energyCounts{};
    std::array<std::optional<BridgeGroup>, 6> familyBridges{};

    for (const auto& expected : presetProfiles) {
        const PresetProfile& profile = profileForPreset(
            "/tmp/presets/" + std::string(expected.filename));
        assert(&profile == &expected);
        assert(filenames.insert(profile.filename).second);
        assert(profile.dwellMinMs >= 9000);
        assert(profile.dwellMaxMs > profile.dwellMinMs);
        assert(profile.asciiDensity >= 0.4f && profile.asciiDensity <= 0.6f);
        assert(profile.asciiExposure >= 0.65f && profile.asciiExposure <= 1.5f);
        assert(profile.kickGain >= 0.5f && profile.kickGain <= 1.3f);
        assert(profile.snareGain >= 0.5f && profile.snareGain <= 1.3f);
        assert(profile.hatGain >= 0.5f && profile.hatGain <= 1.3f);
        assert(reactionMode(profile) == visualFamily(profile));
        const auto family = static_cast<std::size_t>(profile.family);
        ++familyCounts[family];
        ++energyCounts[static_cast<std::size_t>(profile.energy)];
        if (familyBridges[family]) assert(*familyBridges[family] == profile.bridge);
        else familyBridges[family] = profile.bridge;
    }

    for (int count : familyCounts) assert(count >= 1);
    for (int count : energyCounts) assert(count >= 1);
    if (argc == 2) {
        std::size_t fileCount = 0;
        for (const auto& entry : std::filesystem::directory_iterator(argv[1])) {
            if (entry.path().extension() != ".milk") continue;
            ++fileCount;
            assert(findProfileForPreset(entry.path().string()));
        }
        assert(fileCount == presetProfiles.size());
        for (const auto& profile : presetProfiles) {
            assert(std::filesystem::exists(std::filesystem::path(argv[1]) / profile.filename));
        }
    }
    assert(presetBasename("plain.milk") == "plain.milk");
    assert(findProfileForPreset("Aderrasi - Contortion (Escher's Tunnel Mix).milk")
           == &presetProfiles.front());
    assert(findProfileForPreset("Aderrasi - Halls Of Centrifuge.milk")
           == &presetProfiles[2]);
    assert(findProfileForPreset("Martin - wire dance.milk")
           == &presetProfiles[1]);
    assert(findProfileForPreset("missing.milk") == nullptr);
    bool rejected = false;
    try {
        profileForPreset("missing.milk");
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    assert(rejected);
    assert(directionsCompatible(PresetDirection::Inward, PresetDirection::Outward));
    assert(!directionsCompatible(PresetDirection::Orbit, PresetDirection::Oscillate));

    VisualMotifMemory motifs;
    assert(!motifs.familyFor(3));
    motifs.remember(3, 1);
    motifs.remember(3, 2);
    assert(motifs.familyFor(3) == 1);
    motifs.reset();
    assert(!motifs.familyFor(3));
    std::cout << "preset profiles passed\n";
}
