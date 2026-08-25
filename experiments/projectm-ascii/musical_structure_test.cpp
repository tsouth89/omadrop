#include "musical_structure.h"

#include <cassert>
#include <iostream>

namespace {
struct Events {
    int phrases = 0;
    int sections = 0;
    float lastNovelty = 0.0f;
    int lastMotif = -1;
    bool lastMotifRecalled = false;
};

void pushFrames(MusicalStructureTracker& tracker, AudioFeatures features,
                int frames, Events& events) {
    for (int frame = 0; frame < frames; ++frame) {
        features.barCrossed = frame == 0 && features.barCrossed;
        const auto& state = tracker.update(features);
        events.phrases += state.phraseCrossed;
        events.sections += state.sectionCrossed;
        if (state.phraseCrossed) events.lastNovelty = state.novelty;
        if (state.sectionCrossed) {
            events.lastMotif = state.motifIdentity;
            events.lastMotifRecalled = state.motifRecalled;
        }
    }
}

AudioFeatures features(float level, bool barCrossed, float confidence = 0.8f) {
    AudioFeatures value;
    value.level.fill(level);
    value.barCrossed = barCrossed;
    value.beatConfidence = confidence;
    return value;
}

AudioFeatures features(const std::array<float, AudioFeatures::roleCount>& levels,
                       bool barCrossed, float confidence = 0.8f) {
    AudioFeatures value;
    value.level = levels;
    value.barCrossed = barCrossed;
    value.beatConfidence = confidence;
    return value;
}
} // namespace

int main() {
    MusicalStructureTracker tracker;
    Events events;
    pushFrames(tracker, features(1.0f, false), 60, events);
    for (int bar = 1; bar <= 7; ++bar) {
        const float openingLevel = bar == 6 ? 2.8f : 1.0f;
        pushFrames(tracker, features(openingLevel, true), 6, events);
        pushFrames(tracker, features(1.0f, false), 54, events);
    }
    assert(events.phrases == 1);
    assert(events.sections == 0);

    pushFrames(tracker, features(2.8f, true), 18, events);
    pushFrames(tracker, features(2.8f, false), 42, events);
    assert(events.phrases == 2);
    assert(events.sections == 1);
    assert(events.lastNovelty > 0.28f);
    assert(events.lastMotif == 0);
    assert(!events.lastMotifRecalled);

    for (int bar = 9; bar <= 11; ++bar) {
        pushFrames(tracker, features(2.8f, true), 60, events);
    }
    pushFrames(tracker, features(0.1f, true), 60, events);
    assert(events.sections == 1);
    for (int bar = 13; bar <= 19; ++bar) {
        pushFrames(tracker, features(0.1f, true), 60, events);
    }
    pushFrames(tracker, features(2.8f, true), 60, events);
    assert(events.sections == 2);
    assert(events.lastMotif == 0);
    assert(events.lastMotifRecalled);

    tracker.reset();
    events = {};
    pushFrames(tracker, features(1.0f, false, 0.4f), 60, events);
    for (int bar = 1; bar <= 8; ++bar) {
        const float confidence = bar == 4 ? 0.2f : 0.4f;
        pushFrames(tracker, features(1.0f, true, confidence), 60, events);
    }
    assert(events.phrases == 2);
    assert(events.sections == 0);

    tracker.reset();
    events = {};
    const std::array<float, AudioFeatures::roleCount> neutral{1, 1, 1, 1, 1, 1};
    const std::array<float, AudioFeatures::roleCount> bassHeavy{3, 3, 1, 1, 1, 1};
    const std::array<float, AudioFeatures::roleCount> bright{1, 1, 1, 1, 3, 3};
    pushFrames(tracker, features(neutral, false), 60, events);
    for (int bar = 1; bar <= 7; ++bar) {
        pushFrames(tracker, features(neutral, true), 60, events);
    }
    pushFrames(tracker, features(bassHeavy, true), 60, events);
    assert(events.lastMotif == 0 && !events.lastMotifRecalled);
    for (int bar = 9; bar <= 19; ++bar) {
        pushFrames(tracker, features(bassHeavy, true), 60, events);
    }
    pushFrames(tracker, features(bright, true), 60, events);
    assert(events.lastMotif == 1 && !events.lastMotifRecalled);

    tracker.reset();
    events = {};
    pushFrames(tracker, features(1.0f, false, 0.1f), 60, events);
    for (int bar = 1; bar <= 4; ++bar) {
        pushFrames(tracker, features(bar == 4 ? 3.0f : 1.0f, true, 0.1f), 6, events);
        pushFrames(tracker, features(1.0f, false, 0.1f), 54, events);
    }
    assert(events.phrases == 0);
    assert(events.sections == 0);

    std::cout << "musical structure passed\n";
}
