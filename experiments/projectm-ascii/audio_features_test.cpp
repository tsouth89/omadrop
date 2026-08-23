#include "audio_features.h"

#include <cmath>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr double tau = 6.28318530717958647692;

struct Counts { int kick = 0; int snare = 0; int hat = 0; };
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
    const TempoResult tempo90 = runTempo(90.0f);
    const TempoResult tempo120 = runTempo(120.0f);
    const TempoResult tempo140 = runTempo(140.0f);
    const TempoResult tempo174 = runTempo(174.0f);

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

    if (!ok) return 1;
    std::cout << "audio fixtures passed"
              << " | kick=" << kick.kick
              << " snare=" << snare.snare
              << " hat=" << hat.hat
              << " tempos=" << tempo90.bpm << "," << tempo120.bpm << ","
              << tempo140.bpm << "," << tempo174.bpm << "\n";
    return 0;
}
