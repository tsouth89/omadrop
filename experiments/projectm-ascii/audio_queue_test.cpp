#include "audio_queue.h"

#include <algorithm>
#include <cassert>
#include <deque>
#include <iostream>
#include <vector>

namespace {
std::deque<float> sequence(std::size_t count) {
    std::deque<float> result;
    for (std::size_t i = 0; i < count; ++i) result.push_back(static_cast<float>(i));
    return result;
}

void replayPacketCadence() {
    constexpr std::size_t delay = 4;
    constexpr std::size_t hop = 6;
    constexpr std::size_t hops = 12;
    const std::vector<std::size_t> packetSizes{2, 9, 1, 17, 4, 8, 3, 14, 6, 12};
    const auto generated = sequence(delay + hop * hops);
    const std::vector<float> source(generated.begin(), generated.end());
    std::deque<float> queued;
    std::vector<float> replayed;
    std::size_t sourceOffset = 0;
    std::size_t packetIndex = 0;
    std::size_t discarded = 0;
    while (sourceOffset < source.size()) {
        const std::size_t packet = std::min(
            packetSizes[packetIndex++ % packetSizes.size()], source.size() - sourceOffset);
        queued.insert(queued.end(), source.begin() + sourceOffset,
                      source.begin() + sourceOffset + packet);
        sourceOffset += packet;
        const auto prepared = prepareAudioHops(queued, delay, hop, 8);
        discarded += prepared.discardedSamples;
        for (std::size_t i = 0; i < prepared.readableSamples; ++i) {
            replayed.push_back(queued.front());
            queued.pop_front();
        }
    }
    assert(discarded == 0);
    assert(replayed.size() == hop * hops);
    assert(std::equal(replayed.begin(), replayed.end(), source.begin()));
    assert(queued.size() == delay);
}
} // namespace

int main() {
    constexpr std::size_t delay = 200;
    constexpr std::size_t hop = 100;

    auto insufficient = sequence(delay + hop - 2);
    const auto waiting = prepareAudioHops(insufficient, delay, hop, 8);
    assert(waiting.readableSamples == 0);
    assert(waiting.discardedSamples == 0);

    auto exact = sequence(delay + hop);
    const auto ready = prepareAudioHops(exact, delay, hop, 8);
    assert(ready.readableSamples == hop);
    assert(ready.discardedSamples == 0);
    assert(exact.front() == 0.0f);

    auto partialExtra = sequence(delay + hop + hop / 2);
    const auto accumulated = prepareAudioHops(partialExtra, delay, hop, 8);
    assert(accumulated.readableSamples == hop);
    assert(accumulated.discardedSamples == 0);
    assert(partialExtra.size() == delay + hop + hop / 2);

    auto backlogged = sequence(delay + hop * 3);
    const auto caughtUp = prepareAudioHops(backlogged, delay, hop, 1);
    assert(caughtUp.readableSamples == hop);
    assert(caughtUp.discardedSamples == hop * 2);
    assert(backlogged.size() == delay + hop);
    assert(backlogged.front() == static_cast<float>(hop * 2));

    auto delayReduced = sequence(delay + hop);
    const auto recalibrated = prepareAudioHops(delayReduced, delay / 2, hop, 1);
    assert(recalibrated.readableSamples == hop);
    assert(recalibrated.discardedSamples == delay / 2);
    assert(delayReduced.size() == delay / 2 + hop);

    auto recoverable = sequence(delay + hop * 3);
    const auto recovering = prepareAudioHops(recoverable, delay, hop, 8);
    assert(recovering.readableSamples == hop * 3);
    assert(recovering.discardedSamples == 0);

    replayPacketCadence();

    std::cout << "audio queue passed\n";
}
