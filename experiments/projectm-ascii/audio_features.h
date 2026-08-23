#pragma once

#include <fftw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

struct AudioFeatures {
    static constexpr std::size_t roleCount = 6;
    std::array<float, roleCount> level{};
    std::array<float, roleCount> flux{};
    bool kick = false;
    bool snare = false;
    bool hat = false;
    float kickImpact = 0.0f;
    float snareImpact = 0.0f;
    float hatImpact = 0.0f;
    float bpm = 120.0f;
    float beatPhase = 0.0f;
    float beatConfidence = 0.0f;
    float barPhase = 0.0f;
    float phrasePhase = 0.0f;
    bool beatCrossed = false;
    bool barCrossed = false;
};

class AudioFeatureBus {
public:
    static constexpr int sampleRate = 44100;
    static constexpr int windowSize = 2048;
    static constexpr int hopSize = sampleRate / 60;

    AudioFeatureBus()
        : samples_(windowSize, 0.0f), fftInput_(windowSize, 0.0f),
          fftOutput_(windowSize / 2 + 1) {
        constexpr float pi = 3.14159265358979323846f;
        for (int i = 0; i < windowSize; ++i) {
            window_[i] = 0.5f - 0.5f * std::cos(2.0f * pi * i / (windowSize - 1));
        }
        plan_ = fftwf_plan_dft_r2c_1d(windowSize, fftInput_.data(),
            reinterpret_cast<fftwf_complex*>(fftOutput_.data()), FFTW_ESTIMATE);
    }

    ~AudioFeatureBus() {
        if (plan_) fftwf_destroy_plan(plan_);
    }

    AudioFeatureBus(const AudioFeatureBus&) = delete;
    AudioFeatureBus& operator=(const AudioFeatureBus&) = delete;

    const AudioFeatures& processStereo(const float* stereo, std::size_t frames) {
        const std::size_t accepted = std::min<std::size_t>(frames, windowSize);
        std::move(samples_.begin() + accepted, samples_.end(), samples_.begin());
        const std::size_t start = windowSize - accepted;
        for (std::size_t i = 0; i < accepted; ++i) {
            samples_[start + i] = 0.5f * (stereo[i * 2] + stereo[i * 2 + 1]);
        }
        return analyze();
    }

    const AudioFeatures& processMono(const float* mono, std::size_t frames) {
        const std::size_t accepted = std::min<std::size_t>(frames, windowSize);
        std::move(samples_.begin() + accepted, samples_.end(), samples_.begin());
        std::copy_n(mono, accepted, samples_.begin() + (windowSize - accepted));
        return analyze();
    }

private:
    const AudioFeatures& analyze() {
        for (int i = 0; i < windowSize; ++i) fftInput_[i] = samples_[i] * window_[i];
        fftwf_execute(plan_);

        constexpr std::array<float, AudioFeatures::roleCount + 1> edges{
            25.0f, 70.0f, 150.0f, 400.0f, 1500.0f, 4000.0f, 12000.0f};
        std::array<float, AudioFeatures::roleCount> magnitude{};
        for (std::size_t role = 0; role < AudioFeatures::roleCount; ++role) {
            const int first = std::max(1, static_cast<int>(edges[role] * windowSize / sampleRate));
            const int last = std::min(windowSize / 2,
                static_cast<int>(edges[role + 1] * windowSize / sampleRate));
            float sum = 0.0f;
            for (int bin = first; bin <= last; ++bin) {
                const float re = fftOutput_[bin][0];
                const float im = fftOutput_[bin][1];
                sum += std::log1p(std::sqrt(re * re + im * im));
            }
            magnitude[role] = sum / std::max(1, last - first + 1);
        }

        features_.kick = features_.snare = features_.hat = false;
        std::array<float, AudioFeatures::roleCount> positiveFlux{};
        for (std::size_t role = 0; role < AudioFeatures::roleCount; ++role) {
            const float positive = std::max(0.0f, magnitude[role] - previous_[role]);
            positiveFlux[role] = positive;
            fluxMean_[role] = fluxMean_[role] * 0.94f + positive * 0.06f;
            levelMean_[role] = levelMean_[role] * 0.992f + magnitude[role] * 0.008f;
            features_.flux[role] = positive / std::max(0.015f, fluxMean_[role]);
            features_.level[role] = magnitude[role] / std::max(0.02f, levelMean_[role]);
            previous_[role] = magnitude[role];
        }

        const float kickFlux = std::max(features_.flux[0], features_.flux[1]);
        const float snareFlux = 0.25f * features_.flux[2] + 0.35f * features_.flux[3]
                              + 0.40f * features_.flux[4];
        const float hatFlux = features_.flux[5];
        const float lowChange = std::max(positiveFlux[0], positiveFlux[1]);
        const float middleChange = 0.25f * positiveFlux[2] + 0.35f * positiveFlux[3]
                                 + 0.40f * positiveFlux[4];
        const float highChange = positiveFlux[5];
        const float lowEnergy = std::max(magnitude[0], magnitude[1]);
        const float middleEnergy = 0.35f * magnitude[2] + 0.35f * magnitude[3]
                                 + 0.30f * magnitude[4];
        const float highEnergy = magnitude[5];
        const bool warmedUp = frame_ >= 12;
        if (warmedUp && kickCooldown_ == 0 && kickFlux > 2.25f
            && std::max(features_.level[0], features_.level[1]) > 1.08f
            && lowChange > middleChange * 1.25f
            && lowChange > highChange * 1.45f
            && lowEnergy > middleEnergy * 1.20f
            && lowEnergy > highEnergy * 1.40f) {
            features_.kick = true;
            features_.kickImpact = 1.0f;
            kickCooldown_ = 16;
        }
        if (warmedUp && snareCooldown_ == 0 && snareFlux > 2.15f
            && std::max(features_.level[3], features_.level[4]) > 1.06f
            && middleChange > lowChange * 0.72f
            && middleChange > highChange * 0.92f) {
            features_.snare = true;
            features_.snareImpact = 1.0f;
            snareCooldown_ = 10;
        }
        if (warmedUp && hatCooldown_ == 0 && hatFlux > 2.0f
            && features_.level[5] > 1.05f
            && highChange > lowChange * 0.55f
            && highChange > middleChange * 0.88f) {
            features_.hat = true;
            features_.hatImpact = 1.0f;
            hatCooldown_ = 5;
        }
        if (kickCooldown_ > 0) --kickCooldown_;
        if (snareCooldown_ > 0) --snareCooldown_;
        if (hatCooldown_ > 0) --hatCooldown_;
        features_.kickImpact *= 0.88f;
        features_.snareImpact *= 0.84f;
        features_.hatImpact *= 0.72f;
        updateClock();
        ++frame_;
        return features_;
    }

    void updateClock() {
        std::move(onsetHistory_.begin() + 1, onsetHistory_.end(), onsetHistory_.begin());
        onsetHistory_.back() = features_.kick ? 1.0f
                             : features_.snare ? 0.62f
                             : features_.hat ? 0.28f : 0.0f;
        historyFrames_ = std::min<int>(historyFrames_ + 1, onsetHistory_.size());

        if (historyFrames_ >= 240 && frame_ % 16 == 0) {
            constexpr int minimumLag = 19; // 190 BPM at 60 fps
            constexpr int maximumLag = 58; // 62 BPM at 60 fps
            float bestScore = 0.0f;
            int bestLag = static_cast<int>(std::round(beatPeriod_));
            std::array<float, maximumLag + 1> lagScores{};
            const int start = static_cast<int>(onsetHistory_.size()) - historyFrames_;
            for (int lag = minimumLag; lag <= maximumLag; ++lag) {
                float correlation = 0.0f;
                float leftEnergy = 0.0f;
                float rightEnergy = 0.0f;
                for (int i = start + lag; i < static_cast<int>(onsetHistory_.size()); ++i) {
                    const float a = onsetHistory_[i];
                    const float b = onsetHistory_[i - lag];
                    correlation += a * b;
                    leftEnergy += a * a;
                    rightEnergy += b * b;
                }
                const float normalized = correlation
                    / std::sqrt(std::max(1e-8f, leftEnergy * rightEnergy));
                const float bpm = 3600.0f / lag;
                const float octaveDistance = std::log2(std::max(1.0f, bpm) / 120.0f);
                const float prior = std::exp(-0.5f * octaveDistance * octaveDistance
                                             / (1.25f * 1.25f));
                const float score = normalized * prior;
                lagScores[lag] = score;
                if (score > bestScore) {
                    bestScore = score;
                    bestLag = lag;
                }
            }
            if (bestScore > 0.08f) {
                float estimatedLag = static_cast<float>(bestLag);
                if (bestLag > minimumLag && bestLag < maximumLag) {
                    const float y0 = lagScores[bestLag - 1];
                    const float y1 = lagScores[bestLag];
                    const float y2 = lagScores[bestLag + 1];
                    const float denominator = y0 - 2.0f * y1 + y2;
                    if (std::abs(denominator) > 1e-6f) {
                        estimatedLag += std::clamp(0.5f * (y0 - y2) / denominator,
                                                   -0.5f, 0.5f);
                    }
                }
                beatPeriod_ += (estimatedLag - beatPeriod_) * 0.22f;
                features_.beatConfidence += (std::min(1.0f, bestScore * 1.35f)
                                             - features_.beatConfidence) * 0.25f;
            } else {
                features_.beatConfidence *= 0.92f;
            }
        }

        features_.beatCrossed = false;
        features_.barCrossed = false;
        const float previousPhase = features_.beatPhase;
        features_.beatPhase = std::fmod(features_.beatPhase
                                        + 1.0f / std::max(1.0f, beatPeriod_), 1.0f);
        if (features_.beatPhase < previousPhase) {
            features_.beatCrossed = true;
            ++beatIndex_;
        }
        if (features_.kick && features_.beatConfidence >= 0.30f) {
            if (features_.beatPhase > 0.55f) {
                features_.beatPhase = 0.0f;
                if (!features_.beatCrossed) {
                    features_.beatCrossed = true;
                    ++beatIndex_;
                }
            } else {
                features_.beatPhase *= 0.45f;
            }
        }
        features_.bpm = 3600.0f / std::max(1.0f, beatPeriod_);
        features_.barPhase = ((beatIndex_ % 4) + features_.beatPhase) / 4.0f;
        features_.phrasePhase = ((beatIndex_ % 16) + features_.beatPhase) / 16.0f;
        features_.barCrossed = features_.beatCrossed && beatIndex_ % 4 == 0;
    }

    std::vector<float> samples_;
    std::vector<float> fftInput_;
    std::vector<std::array<float, 2>> fftOutput_;
    std::array<float, windowSize> window_{};
    std::array<float, AudioFeatures::roleCount> previous_{};
    std::array<float, AudioFeatures::roleCount> fluxMean_{};
    std::array<float, AudioFeatures::roleCount> levelMean_{};
    fftwf_plan plan_ = nullptr;
    AudioFeatures features_{};
    int frame_ = 0;
    int kickCooldown_ = 0;
    int snareCooldown_ = 0;
    int hatCooldown_ = 0;
    std::array<float, 480> onsetHistory_{};
    int historyFrames_ = 0;
    float beatPeriod_ = 30.0f;
    int beatIndex_ = 0;
};
