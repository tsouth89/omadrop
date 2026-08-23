#pragma once

#include <cstddef>
#include <deque>

struct AudioQueuePreparation {
    std::size_t readableSamples = 0;
    std::size_t discardedSamples = 0;
};

inline AudioQueuePreparation prepareAudioHops(
    std::deque<float>& samples, std::size_t delaySamples, std::size_t hopSamples,
    std::size_t maximumHops) {
    if (samples.size() <= delaySamples) return {};
    const std::size_t readySamples = samples.size() - delaySamples;
    const std::size_t maximumReadySamples = hopSamples * maximumHops;
    const std::size_t discardedSamples = readySamples > maximumReadySamples
        ? readySamples - maximumReadySamples : 0;
    if (discardedSamples > 0) {
        samples.erase(samples.begin(), samples.begin() + discardedSamples);
    }
    const std::size_t remainingReadySamples = samples.size() - delaySamples;
    return {
        remainingReadySamples / hopSamples * hopSamples,
        discardedSamples,
    };
}
