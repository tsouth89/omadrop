#include "audio_features.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr double tau = 6.28318530717958647692;

struct Counts { int kick = 0; int snare = 0; int hat = 0; };
struct ImpactResult {
    int hits = 0;
    float strongest = 0.0f;
    float peakLevel = 0.0f;
    float peakFlux = 0.0f;
};
struct TempoResult {
    float bpm = 0.0f;
    float confidence = 0.0f;
    int aligned = 0;
    int checked = 0;
    int bars = 0;
};

Counts runFixture(float frequency) {
    AudioFeatureBus bus;
    std::vector<float> mono(AudioFeatureBus::hopSize, 0.0f);
    Counts counts;
    double phase = 0.0;
    for (int frame = 0; frame < 300; ++frame) {
        const bool event = frame >= 60 && (frame - 60) % 60 == 0;
        for (int i = 0; i < AudioFeatureBus::hopSize; ++i) {
            mono[i] = event ? 0.72f * static_cast<float>(std::sin(phase)) : 0.0f;
            phase += tau * frequency / AudioFeatureBus::sampleRate;
            if (phase >= tau) phase -= tau;
        }
        const auto& features = bus.processMono(mono.data(), mono.size());
        counts.kick += features.kick;
        counts.snare += features.snare;
        counts.hat += features.hat;
    }
    return counts;
}

ImpactResult runMixedKickFixture(float kickAmplitude) {
    AudioFeatureBus bus;
    std::vector<float> mono(AudioFeatureBus::hopSize, 0.0f);
    ImpactResult result;
    double kickPhase = 0.0;
    double lowBedPhase = 0.0;
    double midBedPhase = 0.0;
    for (int frame = 0; frame < 360; ++frame) {
        const bool event = frame >= 60 && (frame - 60) % 60 == 0;
        for (int i = 0; i < AudioFeatureBus::hopSize; ++i) {
            const float bed = 0.035f * static_cast<float>(std::sin(lowBedPhase))
                            + 0.025f * static_cast<float>(std::sin(midBedPhase));
            mono[i] = bed + (event ? kickAmplitude
                * static_cast<float>(std::sin(kickPhase)) : 0.0f);
            kickPhase += tau * 62.0 / AudioFeatureBus::sampleRate;
            lowBedPhase += tau * 110.0 / AudioFeatureBus::sampleRate;
            midBedPhase += tau * 1800.0 / AudioFeatureBus::sampleRate;
            if (kickPhase >= tau) kickPhase -= tau;
            if (lowBedPhase >= tau) lowBedPhase -= tau;
            if (midBedPhase >= tau) midBedPhase -= tau;
        }
        const auto& features = bus.processMono(mono.data(), mono.size());
        if (frame >= 120 && features.kick) {
            ++result.hits;
            result.strongest = std::max(result.strongest, features.kickImpact);
            result.peakLevel = std::max(result.peakLevel,
                                        std::max(features.level[0], features.level[1]));
            result.peakFlux = std::max(result.peakFlux,
                                       std::max(features.flux[0], features.flux[1]));
        }
    }
    return result;
}

TempoResult runTempo(float bpm) {
    AudioFeatureBus bus;
    std::vector<float> mono(AudioFeatureBus::hopSize, 0.0f);
    TempoResult result;
    double tonePhase = 0.0;
    double beatPhase = 0.0;
    for (int frame = 0; frame < 720; ++frame) {
        beatPhase += bpm / 3600.0;
        const bool event = beatPhase >= 1.0;
        if (event) beatPhase -= 1.0;
        for (int i = 0; i < AudioFeatureBus::hopSize; ++i) {
            mono[i] = event ? 0.72f * static_cast<float>(std::sin(tonePhase)) : 0.0f;
            tonePhase += tau * 62.0 / AudioFeatureBus::sampleRate;
            if (tonePhase >= tau) tonePhase -= tau;
        }
        const auto& features = bus.processMono(mono.data(), mono.size());
        if (frame >= 480 && features.beatConfidence >= 0.30f && features.barCrossed) {
            ++result.bars;
        }
        if (event && frame >= 480 && features.beatConfidence >= 0.30f) {
            const float distance = std::min(features.beatPhase, 1.0f - features.beatPhase);
            result.aligned += distance <= 0.15f;
            ++result.checked;
        }
        result.bpm = features.bpm;
        result.confidence = features.beatConfidence;
    }
    return result;
}

TempoResult runMixedTempo120() {
    AudioFeatureBus bus;
    std::vector<float> mono(AudioFeatureBus::hopSize, 0.0f);
    TempoResult result;
    double kickPhase = 0.0;
    double snarePhase = 0.0;
    double hatPhase = 0.0;
    double bedPhase = 0.0;
    for (int frame = 0; frame < 720; ++frame) {
        const bool tick = frame >= 60 && (frame - 60) % 15 == 0;
        const int tickIndex = frame >= 60 ? (frame - 60) / 15 : 0;
        const bool beat = tick && tickIndex % 2 == 0;
        const bool snare = beat && (tickIndex / 2) % 4 % 2 == 1;
        for (int i = 0; i < AudioFeatureBus::hopSize; ++i) {
            float sample = 0.025f * static_cast<float>(std::sin(bedPhase));
            if (beat) sample += 0.58f * static_cast<float>(std::sin(kickPhase));
            if (snare) sample += 0.26f * static_cast<float>(std::sin(snarePhase));
            if (tick) sample += 0.12f * static_cast<float>(std::sin(hatPhase));
            mono[i] = sample;
            kickPhase += tau * 62.0 / AudioFeatureBus::sampleRate;
            snarePhase += tau * 2100.0 / AudioFeatureBus::sampleRate;
            hatPhase += tau * 7000.0 / AudioFeatureBus::sampleRate;
            bedPhase += tau * 240.0 / AudioFeatureBus::sampleRate;
            if (kickPhase >= tau) kickPhase -= tau;
            if (snarePhase >= tau) snarePhase -= tau;
            if (hatPhase >= tau) hatPhase -= tau;
            if (bedPhase >= tau) bedPhase -= tau;
        }
        const auto& features = bus.processMono(mono.data(), mono.size());
        if (frame >= 480 && features.beatConfidence >= 0.30f && features.barCrossed) {
            ++result.bars;
        }
        if (beat && frame >= 480 && features.beatConfidence >= 0.30f) {
            const float distance = std::min(features.beatPhase, 1.0f - features.beatPhase);
            result.aligned += distance <= 0.15f;
            ++result.checked;
        }
        result.bpm = features.bpm;
        result.confidence = features.beatConfidence;
    }
    return result;
}

bool expect(const std::string& name, int actual, int minimum, int maximum) {
    if (actual >= minimum && actual <= maximum) return true;
    std::cerr << name << ": expected " << minimum << ".." << maximum
              << ", got " << actual << "\n";
    return false;
}
} // namespace

int main() {
    const Counts silence = runFixture(0.0f);
    const Counts kick = runFixture(62.0f);
    const Counts snare = runFixture(2200.0f);
    const Counts hat = runFixture(7000.0f);
    const ImpactResult softKick = runMixedKickFixture(0.16f);
    const ImpactResult mediumKick = runMixedKickFixture(0.34f);
    const ImpactResult hardKick = runMixedKickFixture(0.72f);
    const TempoResult tempo90 = runTempo(90.0f);
    const TempoResult tempo120 = runTempo(120.0f);
    const TempoResult tempo140 = runTempo(140.0f);
    const TempoResult tempo174 = runTempo(174.0f);
    const TempoResult mixedTempo120 = runMixedTempo120();

    bool ok = true;
    ok &= expect("silence kick", silence.kick, 0, 0);
    ok &= expect("silence snare", silence.snare, 0, 0);
    ok &= expect("silence hat", silence.hat, 0, 0);
    ok &= expect("62 Hz kick", kick.kick, 3, 4);
    ok &= expect("62 Hz snare leakage", kick.snare, 0, 1);
    ok &= expect("62 Hz hat leakage", kick.hat, 0, 0);
    ok &= expect("2.2 kHz snare", snare.snare, 3, 4);
    ok &= expect("2.2 kHz kick leakage", snare.kick, 0, 0);
    ok &= expect("7 kHz hat", hat.hat, 3, 4);
    ok &= expect("7 kHz kick leakage", hat.kick, 0, 0);
    if (softKick.hits == 0 || mediumKick.hits == 0 || hardKick.hits == 0) {
        std::cerr << "mixed kick fixture: missing detections "
                  << softKick.hits << "," << mediumKick.hits << "," << hardKick.hits << "\n";
        ok = false;
    }
    if (!(softKick.strongest + 0.04f < mediumKick.strongest
          && mediumKick.strongest + 0.04f < hardKick.strongest)) {
        std::cerr << "mixed kick strength: expected soft < medium < hard, got "
                  << softKick.strongest << "," << mediumKick.strongest << ","
                  << hardKick.strongest << " levels=" << softKick.peakLevel << ","
                  << mediumKick.peakLevel << "," << hardKick.peakLevel
                  << " flux=" << softKick.peakFlux << "," << mediumKick.peakFlux
                  << "," << hardKick.peakFlux << "\n";
        ok = false;
    }
    for (const auto [expected, result] : {
             std::pair{90.0f, tempo90}, std::pair{120.0f, tempo120},
             std::pair{140.0f, tempo140}, std::pair{174.0f, tempo174}}) {
        if (std::abs(result.bpm - expected) > 4.0f || result.confidence < 0.30f) {
            std::cerr << "tempo " << expected << ": got " << result.bpm
                      << " confidence " << result.confidence << "\n";
            ok = false;
        }
        if (result.checked == 0 || result.aligned * 4 < result.checked * 3) {
            std::cerr << "phase " << expected << ": aligned " << result.aligned
                      << "/" << result.checked << "\n";
            ok = false;
        }
        if (result.bars == 0) {
            std::cerr << "bar clock " << expected << ": no bar crossings\n";
            ok = false;
        }
    }
    if (std::abs(mixedTempo120.bpm - 120.0f) > 4.0f
        || mixedTempo120.confidence < 0.30f
        || mixedTempo120.checked == 0
        || mixedTempo120.aligned * 4 < mixedTempo120.checked * 3
        || mixedTempo120.bars == 0) {
        std::cerr << "mixed tempo 120: bpm=" << mixedTempo120.bpm
                  << " confidence=" << mixedTempo120.confidence
                  << " aligned=" << mixedTempo120.aligned << "/" << mixedTempo120.checked
                  << " bars=" << mixedTempo120.bars << "\n";
        ok = false;
    }

    if (!ok) return 1;
    std::cout << "audio fixtures passed"
              << " | kick=" << kick.kick
              << " snare=" << snare.snare
              << " hat=" << hat.hat
              << " impact=" << softKick.strongest << ","
              << mediumKick.strongest << "," << hardKick.strongest
              << " tempos=" << tempo90.bpm << "," << tempo120.bpm << ","
              << tempo140.bpm << "," << tempo174.bpm << "\n";
    return 0;
}
