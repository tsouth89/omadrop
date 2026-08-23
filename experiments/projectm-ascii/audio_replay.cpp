#include "audio_features.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct EventSummary {
    int count = 0;
    float strongest = 0.0f;

    void add(bool detected, float impact) {
        if (!detected) return;
        ++count;
        strongest = std::max(strongest, impact);
    }
};
} // namespace

int main(int argc, char** argv) {
    if (argc != 2) {
        std::cerr << "usage: audio-feature-replay RAW_F32_STEREO\n";
        return 2;
    }
    std::ifstream input(argv[1], std::ios::binary);
    if (!input) {
        std::cerr << "could not open: " << argv[1] << "\n";
        return 1;
    }

    AudioFeatureBus bus;
    std::vector<float> pcm(AudioFeatureBus::hopSize * 2);
    EventSummary kicks;
    EventSummary snares;
    EventSummary hats;
    AudioFeatures latest;
    std::size_t hops = 0;
    int snareCandidates = 0;
    int hatCandidates = 0;
    int relaxedSnareCandidates = 0;
    int relaxedHatCandidates = 0;
    while (input.read(reinterpret_cast<char*>(pcm.data()),
                      static_cast<std::streamsize>(pcm.size() * sizeof(float)))) {
        latest = bus.processStereo(pcm.data(), AudioFeatureBus::hopSize);
        kicks.add(latest.kick, latest.kickImpact);
        snares.add(latest.snare, latest.snareImpact);
        hats.add(latest.hat, latest.hatImpact);
        const float snareFlux = 0.25f * latest.flux[2] + 0.35f * latest.flux[3]
                              + 0.40f * latest.flux[4];
        snareCandidates += snareFlux > 2.15f
                        && std::max(latest.level[3], latest.level[4]) > 1.06f;
        hatCandidates += latest.flux[5] > 2.0f && latest.level[5] > 1.05f;
        relaxedSnareCandidates += snareFlux > 1.45f
                               && std::max(latest.level[3], latest.level[4]) > 1.03f;
        relaxedHatCandidates += latest.flux[5] > 1.35f && latest.level[5] > 1.02f;
        ++hops;
    }
    if (hops == 0) {
        std::cerr << "recording contains no complete audio hops\n";
        return 1;
    }

    const double seconds = hops * AudioFeatureBus::hopSize
                         / static_cast<double>(AudioFeatureBus::sampleRate);
    auto rate = [seconds](const EventSummary& events) {
        return events.count * 60.0 / seconds;
    };
    std::cout << "audio replay " << seconds << " sec"
              << " | kick=" << kicks.count << " (" << rate(kicks) << "/min, "
              << kicks.strongest << " max)"
              << " snare=" << snares.count << " (" << rate(snares) << "/min, "
              << snares.strongest << " max, candidates " << snareCandidates
              << "/" << relaxedSnareCandidates << ")"
              << " hat=" << hats.count << " (" << rate(hats) << "/min, "
              << hats.strongest << " max, candidates " << hatCandidates
              << "/" << relaxedHatCandidates << ")"
              << " | bpm=" << latest.bpm
              << " confidence=" << latest.beatConfidence << "\n";
}
