#include "audio_features.h"
#include "music_frame.h"
#include "musical_structure.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
struct EventSummary {
    int count = 0;
    int above055 = 0;
    int above075 = 0;
    int above100 = 0;
    float strongest = 0.0f;

    void add(bool detected, float impact) {
        if (!detected) return;
        ++count;
        above055 += impact >= 0.55f;
        above075 += impact >= 0.75f;
        above100 += impact >= 1.00f;
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
    MusicalStructureTracker structureTracker;
    MusicFrameBuilder musicFrameBuilder;
    MusicFrame musicFrame;
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
    int kickSnareOverlaps = 0;
    int kickHatOverlaps = 0;
    int snareHatOverlaps = 0;
    int phrases = 0;
    int sections = 0;
    while (input.read(reinterpret_cast<char*>(pcm.data()),
                      static_cast<std::streamsize>(pcm.size() * sizeof(float)))) {
        latest = bus.processStereo(pcm.data(), AudioFeatureBus::hopSize);
        const MusicalStructureState& structure = structureTracker.update(latest);
        musicFrame = musicFrameBuilder.update(
            latest, structure, 1.0f / 60.0f);
        if (structure.barAnalyzed) {
            const double eventSeconds = hops * AudioFeatureBus::hopSize
                                      / static_cast<double>(AudioFeatureBus::sampleRate);
            std::cout << "structure " << eventSeconds << " sec"
                      << " bar=" << structure.barIndex
                      << " novelty=" << structure.novelty
                      << " threshold=" << structure.noveltyThreshold
                      << (structure.clockLocked ? " locked" : " unlocked")
                      << (structure.sectionCrossed ? " section"
                          : structure.phraseCrossed ? " phrase" : " bar");
            if (structure.sectionCrossed) {
                std::cout << " motif=" << structure.motifIdentity
                          << (structure.motifRecalled ? " recalled" : " new");
            }
            std::cout << "\n";
        }
        if (structure.phraseCrossed) {
            ++phrases;
            if (structure.sectionCrossed) ++sections;
        } else if (structure.sectionCrossed) ++sections;
        kicks.add(latest.kick, latest.kickImpact);
        snares.add(latest.snare, latest.snareImpact);
        hats.add(latest.hat, latest.hatImpact);
        kickSnareOverlaps += latest.kick && latest.snare;
        kickHatOverlaps += latest.kick && latest.hat;
        snareHatOverlaps += latest.snare && latest.hat;
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
              << kicks.strongest << " max, accents " << kicks.above055 << "/"
              << kicks.above075 << "/" << kicks.above100 << ")"
              << " snare=" << snares.count << " (" << rate(snares) << "/min, "
              << snares.strongest << " max, candidates " << snareCandidates
              << "/" << relaxedSnareCandidates << ")"
              << " hat=" << hats.count << " (" << rate(hats) << "/min, "
              << hats.strongest << " max, candidates " << hatCandidates
              << "/" << relaxedHatCandidates << ")"
              << " | bpm=" << latest.bpm
              << " confidence=" << latest.beatConfidence
              << " audio_time=" << musicFrame.audioTimeSeconds
              << " percussive=" << musicFrame.percussive
              << " harmonic=" << musicFrame.harmonic
              << " centroid=" << musicFrame.spectralCentroid
              << " stereo_width=" << musicFrame.stereoWidth
              << " overlaps=" << kickSnareOverlaps << "/"
              << kickHatOverlaps << "/" << snareHatOverlaps
              << " phrases=" << phrases
              << " sections=" << sections << "\n";
}
