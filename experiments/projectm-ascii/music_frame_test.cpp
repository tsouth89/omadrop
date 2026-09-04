#include "music_frame.h"

#include <cassert>
#include <iostream>

int main() {
    MusicFrameBuilder builder;
    AudioFeatures features;
    features.level = {2.1f, 1.8f, 1.2f, 1.4f, 1.1f, 0.9f};
    features.flux = {5.0f, 4.4f, 2.0f, 2.6f, 2.0f, 1.5f};
    features.spectrumLevel[17] = 1.7f;
    features.percussiveEnergy = 0.72f;
    features.harmonicEnergy = 0.54f;
    features.spectralCentroid = 0.63f;
    features.stereoWidth = 0.28f;
    features.audioTimeSeconds = 14.5;
    features.kick = true;
    features.kickImpact = 1.1f;
    features.snare = true;
    features.snareImpact = 0.8f;
    features.hat = true;
    features.hatImpact = 0.6f;
    features.bpm = 128.0f;
    features.beatPhase = 0.9f;
    features.beatConfidence = 0.8f;
    features.beatCrossed = true;
    features.barCrossed = true;
    features.barPhase = 0.0f;
    features.phrasePhase = 0.25f;

    MusicalStructureState structure;
    structure.sectionCrossed = true;
    structure.novelty = 0.42f;
    structure.motifIdentity = 3;

    const MusicFrame first = builder.update(features, structure, 1.0f / 60.0f, 0.18f);
    assert(first.kick == 1.1f);
    assert(first.snare == 0.8f);
    assert(first.hat == 0.6f);
    assert(first.spectrumLevel[17] == 1.7f);
    assert(first.beatAnticipation > 0.5f);
    assert(first.beatPulse > 0.8f);
    assert(first.onsetPulse > 0.1f);
    assert(first.downbeat > 0.8f);
    assert(first.section > 0.9f);
    assert(first.percussive > 0.0f);
    assert(first.harmonic > 0.0f);
    assert(first.spectralCentroid == 0.63f);
    assert(first.stereoWidth == 0.28f);
    assert(first.audioTimeSeconds == 14.5);
    assert(first.presentationDelaySeconds == 0.18f);
    assert(first.energyFast > 0.0f);
    assert(first.motifIdentity == 3);

    features.barCrossed = false;
    features.beatCrossed = false;
    features.kick = false;
    features.snare = false;
    features.hat = false;
    features.flux.fill(0.0f);
    // The analyzer may keep reporting its longer internal tails. Native
    // visual envelopes must decay from the discrete event instead.
    features.kickImpact = 1.0f;
    features.snareImpact = 0.7f;
    features.hatImpact = 0.5f;
    structure.sectionCrossed = false;
    const MusicFrame later = builder.update(features, structure, 0.1f);
    assert(later.downbeat < first.downbeat);
    assert(later.beatPulse < first.beatPulse);
    assert(later.onsetPulse < first.onsetPulse);
    assert(later.section < first.section);
    assert(later.kick < first.kick);
    assert(later.snare < first.snare);
    assert(later.hat < first.hat);

    builder.reset();
    const MusicFrame reset = builder.update(AudioFeatures{}, MusicalStructureState{},
                                            1.0f / 60.0f);
    assert(reset.downbeat == 0.0f);
    assert(reset.beatPulse == 0.0f);
    assert(reset.onsetPulse == 0.0f);
    assert(reset.section == 0.0f);
    std::cout << "music frame passed\n";
}
