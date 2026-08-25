#pragma once

#include "audio_features.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

struct MusicalStructureState {
    bool barAnalyzed = false;
    bool phraseCrossed = false;
    bool sectionCrossed = false;
    bool clockLocked = false;
    float novelty = 0.0f;
    float noveltyThreshold = 0.10f;
    int barIndex = 0;
    int motifIdentity = -1;
    bool motifRecalled = false;
};

class MusicalStructureTracker {
public:
    const MusicalStructureState& update(const AudioFeatures& features) {
        state_.phraseCrossed = false;
        state_.sectionCrossed = false;
        state_.barAnalyzed = false;
        state_.motifIdentity = -1;
        state_.motifRecalled = false;

        if (features.beatConfidence >= 0.32f) clockLocked_ = true;
        else if (features.beatConfidence <= 0.12f) clockLocked_ = false;
        state_.clockLocked = clockLocked_;

        const Signature signature = signatureFor(features);
        if (features.barCrossed) {
            if (barFrames_ > 0) {
                for (std::size_t index = 0; index < signatureSize; ++index) {
                    const float completedBar = barSum_[index] / barFrames_;
                    if (haveBaseline_) {
                        baseline_[index] += (completedBar - baseline_[index]) * 0.25f;
                    } else {
                        baseline_[index] = completedBar;
                    }
                }
                haveBaseline_ = true;
            }
            barSum_.fill(0.0f);
            barFrames_ = 0;
            openingSum_.fill(0.0f);
            openingFrames_ = 0;
            openingSampleFrames_ = 0;
            openingPending_ = true;
            ++state_.barIndex;
            pendingPhrase_ = state_.barIndex % 4 == 0 && clockLocked_;
            pendingBpm_ = features.bpm;
        }

        for (std::size_t index = 0; index < signatureSize; ++index) {
            barSum_[index] += signature[index];
            if (openingPending_ && openingFrames_ >= transientFrameCount) {
                openingSum_[index] += signature[index];
            }
        }
        if (openingPending_ && openingFrames_ >= transientFrameCount) {
            ++openingSampleFrames_;
        }
        ++barFrames_;

        if (openingPending_ && ++openingFrames_ >= openingFrameCount) {
            classifyOpening();
            openingPending_ = false;
        }
        return state_;
    }

    void reset() { *this = MusicalStructureTracker{}; }

private:
    static constexpr std::size_t signatureSize = AudioFeatures::roleCount;
    static constexpr int transientFrameCount = 6;
    static constexpr int openingFrameCount = 18;
    using Signature = std::array<float, signatureSize>;

    static Signature signatureFor(const AudioFeatures& features) {
        Signature signature{};
        for (std::size_t index = 0; index < signatureSize; ++index) {
            const float level = std::clamp(features.level[index], 0.0f, 3.0f);
            signature[index] = std::log1p(level) / std::log(4.0f);
        }
        return signature;
    }

    void classifyOpening() {
        state_.barAnalyzed = true;
        state_.phraseCrossed = pendingPhrase_;
        if (!haveBaseline_) return;

        Signature fingerprint{};
        float squaredDistance = 0.0f;
        for (std::size_t index = 0; index < signatureSize; ++index) {
            const float opening = openingSum_[index] / openingSampleFrames_;
            const float difference = opening - baseline_[index];
            fingerprint[index] = difference;
            squaredDistance += difference * difference;
        }
        state_.novelty = std::sqrt(squaredDistance / signatureSize);
        state_.noveltyThreshold = std::max(0.10f,
                                           noveltyMean_ + 0.50f * noveltyDeviation_);
        const int minimumSectionBars = std::max(
            8, static_cast<int>(std::ceil(pendingBpm_ / 10.0f)));
        state_.sectionCrossed = clockLocked_ && state_.barIndex >= 8
                             && state_.barIndex - lastSectionBar_ >= minimumSectionBars
                             && state_.novelty >= state_.noveltyThreshold;
        if (state_.sectionCrossed) {
            lastSectionBar_ = state_.barIndex;
            classifyMotif(fingerprint);
        }

        const float difference = std::abs(state_.novelty - noveltyMean_);
        noveltyMean_ += (state_.novelty - noveltyMean_) * 0.12f;
        noveltyDeviation_ += (difference - noveltyDeviation_) * 0.12f;
    }

    static Signature normalized(Signature value) {
        float squaredLength = 0.0f;
        for (float component : value) squaredLength += component * component;
        const float length = std::sqrt(std::max(1e-8f, squaredLength));
        for (float& component : value) component /= length;
        return value;
    }

    void classifyMotif(const Signature& fingerprint) {
        const Signature candidate = normalized(fingerprint);
        float bestSimilarity = -1.0f;
        std::size_t bestIndex = 0;
        for (std::size_t motif = 0; motif < motifFingerprints_.size(); ++motif) {
            float similarity = 0.0f;
            for (std::size_t index = 0; index < signatureSize; ++index) {
                similarity += candidate[index] * motifFingerprints_[motif][index];
            }
            if (similarity > bestSimilarity) {
                bestSimilarity = similarity;
                bestIndex = motif;
            }
        }
        if (bestSimilarity >= 0.90f) {
            state_.motifIdentity = static_cast<int>(bestIndex);
            state_.motifRecalled = true;
            for (std::size_t index = 0; index < signatureSize; ++index) {
                motifFingerprints_[bestIndex][index]
                    = motifFingerprints_[bestIndex][index] * 0.85f
                    + candidate[index] * 0.15f;
            }
            motifFingerprints_[bestIndex] = normalized(motifFingerprints_[bestIndex]);
            return;
        }
        state_.motifIdentity = static_cast<int>(motifFingerprints_.size());
        motifFingerprints_.push_back(candidate);
    }

    MusicalStructureState state_{};
    Signature barSum_{};
    Signature baseline_{};
    Signature openingSum_{};
    int barFrames_ = 0;
    int openingFrames_ = 0;
    int openingSampleFrames_ = 0;
    bool haveBaseline_ = false;
    bool openingPending_ = false;
    bool pendingPhrase_ = false;
    float pendingBpm_ = 120.0f;
    bool clockLocked_ = false;
    int lastSectionBar_ = -100;
    float noveltyMean_ = 0.08f;
    float noveltyDeviation_ = 0.05f;
    std::vector<Signature> motifFingerprints_;
};
