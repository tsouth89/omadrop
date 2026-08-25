#include "preset_selector.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <random>
#include <string>
#include <vector>

int main() {
    std::vector<std::string> presets;
    for (const auto& profile : presetProfiles) presets.emplace_back(profile.filename);

    std::mt19937 randomEngine(17);
    std::size_t current = 0;
    std::deque<std::size_t> recent{current};
    for (int transition = 0; transition < 200; ++transition) {
        const std::size_t next = choosePresetVariant(
            presets, current, recent, PresetEnergy::Medium, -1, randomEngine);
        assert(next != current);
        assert(std::find(recent.begin(), recent.end(), next) == recent.end());
        current = next;
        recent.push_back(current);
        while (recent.size() > presetHistoryLimit(presets.size())) recent.pop_front();
    }

    const int organicFamily = static_cast<int>(VisualFamily::OrganicTendrils);
    std::deque<std::size_t> oneOrganicRecent{5};
    current = 5;
    const std::size_t freshOrganic = choosePresetVariant(
        presets, current, oneOrganicRecent, PresetEnergy::Calm, organicFamily, randomEngine);
    assert(visualFamily(profileForPreset(presets[freshOrganic])) == organicFamily);
    assert(freshOrganic == 8);

    std::deque<std::size_t> organicFamilyExhausted{5, 8};
    const std::size_t relatedFallback = choosePresetVariant(
        presets, current, organicFamilyExhausted, PresetEnergy::Medium,
        organicFamily, randomEngine);
    assert(std::find(organicFamilyExhausted.begin(), organicFamilyExhausted.end(), relatedFallback)
           == organicFamilyExhausted.end());
    assert(profileForPreset(presets[relatedFallback]).bridge == BridgeGroup::Organic);

    std::cout << "preset selector passed\n";
}
