#pragma once

#include "preset_profiles.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <deque>
#include <optional>
#include <random>
#include <string>
#include <vector>

inline std::size_t presetHistoryLimit(std::size_t presetCount) {
    return presetCount > 3 ? std::min<std::size_t>(8, presetCount - 3) : 0;
}

template <typename RandomEngine>
std::size_t choosePresetVariant(const std::vector<std::string>& presets,
                                std::size_t currentIndex,
                                const std::deque<std::size_t>& recentPresets,
                                PresetEnergy targetEnergy,
                                int preferredFamily,
                                RandomEngine& randomEngine) {
    if (presets.size() < 2) return currentIndex;

    struct Candidate {
        std::size_t index;
        float score;
        bool recent;
    };
    std::vector<Candidate> candidates;
    const PresetProfile& current = profileForPreset(presets[currentIndex]);
    auto recentlyUsed = [&](std::size_t index) {
        return std::find(recentPresets.begin(), recentPresets.end(), index)
            != recentPresets.end();
    };
    for (std::size_t index = 0; index < presets.size(); ++index) {
        if (index == currentIndex) continue;
        const PresetProfile& candidate = profileForPreset(presets[index]);
        const int energyDistance = std::abs(static_cast<int>(candidate.energy)
                                          - static_cast<int>(targetEnergy));
        float score = energyDistance == 0 ? 4.0f : energyDistance == 1 ? 1.0f : -4.0f;
        score += current.bridge == candidate.bridge ? 3.0f : -2.0f;
        score += directionsCompatible(current.direction, candidate.direction) ? 2.0f : -1.0f;
        score -= std::abs(current.asciiDensity - candidate.asciiDensity) * 5.0f;
        std::uniform_real_distribution<float> variation(-0.75f, 0.75f);
        candidates.push_back({index, score + variation(randomEngine), recentlyUsed(index)});
    }

    auto keepOnly = [&](auto predicate) {
        std::erase_if(candidates, [&](const Candidate& candidate) {
            return !predicate(candidate);
        });
    };
    auto any = [&](auto predicate) {
        return std::any_of(candidates.begin(), candidates.end(), predicate);
    };

    if (preferredFamily >= 0) {
        const auto familyIsPreferred = [&](const Candidate& candidate) {
            return visualFamily(profileForPreset(presets[candidate.index])) == preferredFamily;
        };
        const bool freshFamilyVariant = any([&](const Candidate& candidate) {
            return !candidate.recent && familyIsPreferred(candidate);
        });
        if (freshFamilyVariant) {
            keepOnly([&](const Candidate& candidate) {
                return !candidate.recent && familyIsPreferred(candidate);
            });
        } else {
            std::optional<BridgeGroup> preferredBridge;
            for (const auto& profile : presetProfiles) {
                if (visualFamily(profile) == preferredFamily) {
                    preferredBridge = profile.bridge;
                    break;
                }
            }
            const bool freshBridgeVariant = preferredBridge && any([&](const Candidate& candidate) {
                const PresetProfile& profile = profileForPreset(presets[candidate.index]);
                return !candidate.recent && profile.bridge == *preferredBridge;
            });
            if (freshBridgeVariant) {
                keepOnly([&](const Candidate& candidate) {
                    const PresetProfile& profile = profileForPreset(presets[candidate.index]);
                    return !candidate.recent && profile.bridge == *preferredBridge;
                });
            } else if (any([](const Candidate& candidate) { return !candidate.recent; })) {
                keepOnly([](const Candidate& candidate) { return !candidate.recent; });
            }
        }
    } else if (any([](const Candidate& candidate) { return !candidate.recent; })) {
        keepOnly([](const Candidate& candidate) { return !candidate.recent; });
    }

    if (candidates.empty()) return currentIndex;
    std::sort(candidates.begin(), candidates.end(), [](const Candidate& a, const Candidate& b) {
        return a.score > b.score;
    });
    const std::size_t finalistCount = std::min<std::size_t>(3, candidates.size());
    std::uniform_int_distribution<std::size_t> pick(0, finalistCount - 1);
    return candidates[pick(randomEngine)].index;
}
