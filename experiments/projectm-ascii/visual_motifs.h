#pragma once

#include <optional>
#include <unordered_map>

class VisualMotifMemory {
public:
    std::optional<int> familyFor(int motifIdentity) const {
        const auto match = families_.find(motifIdentity);
        if (match == families_.end()) return std::nullopt;
        return match->second;
    }

    void remember(int motifIdentity, int visualFamily) {
        families_.try_emplace(motifIdentity, visualFamily);
    }

    void reset() { families_.clear(); }

private:
    std::unordered_map<int, int> families_;
};
