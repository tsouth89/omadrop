#pragma once

#include <fftw3.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

struct AudioFeatures {
    static constexpr std::size_t roleCount = 6;
    static constexpr std::size_t spectrumCount = 32;
    std::array<float, roleCount> level{};
    std::array<float, roleCount> flux{};
    std::array<float, spectrumCount> spectrumLevel{};
    std::array<float, spectrumCount> spectrumFlux{};
    bool kick = false;
    bool snare = false;
    bool hat = false;
    float kickImpact = 0.0f;
    float snareImpact = 0.0f;
    float hatImpact = 0.0f;
    float percussiveEnergy = 0.0f;
    float harmonicEnergy = 0.0f;
    float spectralCentroid = 0.0f;
    float stereoWidth = 0.0f;
    double audioTimeSeconds = 0.0;
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

    void resetClock() {
        onsetHistory_.fill(0.0f);
        tempoVotes_.fill(0.0f);
        historyFrames_ = 0;
        beatPeriod_ = 30.0f;
        beatIndex_ = 0;
        features_.bpm = 120.0f;
        features_.beatPhase = 0.0f;
        features_.barPhase = 0.0f;
        features_.phrasePhase = 0.0f;
        features_.beatConfidence = 0.0f;
        features_.beatCrossed = false;
        features_.barCrossed = false;
        spectrumHistory_ = {};
        spectrumHistoryCursor_ = 0;
        spectrumHistoryFrames_ = 0;
    }

    const AudioFeatures& processStereo(const float* stereo, std::size_t frames) {
        const std::size_t accepted = std::min<std::size_t>(frames, windowSize);
        std::move(samples_.begin() + accepted, samples_.end(), samples_.begin());
        const std::size_t start = windowSize - accepted;
        float middleEnergy = 0.0f;
        float sideEnergy = 0.0f;
        for (std::size_t i = 0; i < accepted; ++i) {
            const float middle = 0.5f * (stereo[i * 2] + stereo[i * 2 + 1]);
            const float side = 0.5f * (stereo[i * 2] - stereo[i * 2 + 1]);
            samples_[start + i] = middle;
            middleEnergy += middle * middle;
            sideEnergy += side * side;
        }
        pendingStereoWidth_ = std::clamp(
            std::sqrt(sideEnergy / std::max(1e-8f, middleEnergy + sideEnergy)),
            0.0f, 1.0f);
        processedFrames_ += accepted;
        features_.audioTimeSeconds
            = processedFrames_ / static_cast<double>(sampleRate);
        return analyze();
    }

    const AudioFeatures& processMono(const float* mono, std::size_t frames) {
        const std::size_t accepted = std::min<std::size_t>(frames, windowSize);
        std::move(samples_.begin() + accepted, samples_.end(), samples_.begin());
        std::copy_n(mono, accepted, samples_.begin() + (windowSize - accepted));
        pendingStereoWidth_ = 0.0f;
        processedFrames_ += accepted;
        features_.audioTimeSeconds
            = processedFrames_ / static_cast<double>(sampleRate);
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

        std::array<float, AudioFeatures::spectrumCount> spectrumMagnitude{};
        constexpr float minimumSpectrumHz = 25.0f;
        constexpr float maximumSpectrumHz = 12000.0f;
        constexpr float spectrumRatio = maximumSpectrumHz / minimumSpectrumHz;
        for (std::size_t band = 0; band < AudioFeatures::spectrumCount; ++band) {
            const float lowerHz = minimumSpectrumHz * std::pow(
                spectrumRatio,
                static_cast<float>(band) / AudioFeatures::spectrumCount);
            const float upperHz = minimumSpectrumHz * std::pow(
                spectrumRatio,
                static_cast<float>(band + 1) / AudioFeatures::spectrumCount);
            const int first = std::max(
                1, static_cast<int>(lowerHz * windowSize / sampleRate));
            const int last = std::min(
                windowSize / 2,
                std::max(first, static_cast<int>(upperHz * windowSize / sampleRate)));
            float sum = 0.0f;
            for (int bin = first; bin <= last; ++bin) {
                const float re = fftOutput_[bin][0];
                const float im = fftOutput_[bin][1];
                sum += std::log1p(std::sqrt(re * re + im * im));
            }
            spectrumMagnitude[band] = sum / std::max(1, last - first + 1);
        }

        spectrumHistory_[spectrumHistoryCursor_] = spectrumMagnitude;
        spectrumHistoryCursor_ = (spectrumHistoryCursor_ + 1) % spectrumHistory_.size();
        spectrumHistoryFrames_ = std::min<std::size_t>(
            spectrumHistoryFrames_ + 1, spectrumHistory_.size());
        float harmonicSum = 0.0f;
        float percussiveSum = 0.0f;
        float spectrumSum = 0.0f;
        float centroidSum = 0.0f;
        for (std::size_t band = 0; band < AudioFeatures::spectrumCount; ++band) {
            std::array<float, 17> temporalValues{};
            for (std::size_t frame = 0; frame < spectrumHistoryFrames_; ++frame) {
                temporalValues[frame] = spectrumHistory_[frame][band];
            }
            const auto temporalMiddle = temporalValues.begin()
                + static_cast<std::ptrdiff_t>(spectrumHistoryFrames_ / 2);
            std::nth_element(temporalValues.begin(), temporalMiddle,
                             temporalValues.begin()
                                 + static_cast<std::ptrdiff_t>(spectrumHistoryFrames_));
            const float harmonic = *temporalMiddle;

            std::array<float, 5> frequencyValues{};
            for (int offset = -2; offset <= 2; ++offset) {
                const std::size_t neighbor = static_cast<std::size_t>(std::clamp(
                    static_cast<int>(band) + offset, 0,
                    static_cast<int>(AudioFeatures::spectrumCount) - 1));
                frequencyValues[offset + 2] = spectrumMagnitude[neighbor];
            }
            std::nth_element(frequencyValues.begin(), frequencyValues.begin() + 2,
                             frequencyValues.end());
            const float percussive = frequencyValues[2];
            const float harmonicPower = harmonic * harmonic;
            const float percussivePower = percussive * percussive;
            const float denominator = harmonicPower + percussivePower + 1e-8f;
            const float magnitudeValue = spectrumMagnitude[band];
            harmonicSum += magnitudeValue * harmonicPower / denominator;
            percussiveSum += magnitudeValue * percussivePower / denominator;
            spectrumSum += magnitudeValue;
            const float bandPosition = (static_cast<float>(band) + 0.5f)
                                     / AudioFeatures::spectrumCount;
            centroidSum += magnitudeValue * bandPosition;
        }
        features_.harmonicEnergy = harmonicSum / std::max(1e-6f, spectrumSum);
        features_.percussiveEnergy = percussiveSum / std::max(1e-6f, spectrumSum);
        features_.spectralCentroid = centroidSum / std::max(1e-6f, spectrumSum);
        features_.stereoWidth = pendingStereoWidth_;

        features_.kick = features_.snare = features_.hat = false;
        features_.kickImpact *= 0.88f;
        features_.snareImpact *= 0.84f;
        features_.hatImpact *= 0.72f;
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
        for (std::size_t band = 0; band < AudioFeatures::spectrumCount; ++band) {
            const float positive = std::max(
                0.0f, spectrumMagnitude[band] - previousSpectrum_[band]);
            spectrumFluxMean_[band]
                = spectrumFluxMean_[band] * 0.94f + positive * 0.06f;
            spectrumLevelMean_[band]
                = spectrumLevelMean_[band] * 0.992f + spectrumMagnitude[band] * 0.008f;
            features_.spectrumFlux[band]
                = positive / std::max(0.015f, spectrumFluxMean_[band]);
            features_.spectrumLevel[band]
                = spectrumMagnitude[band] / std::max(0.02f, spectrumLevelMean_[band]);
            previousSpectrum_[band] = spectrumMagnitude[band];
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
        auto impactStrength = [](float level, float levelThreshold) {
            const float levelExcess = std::clamp(
                (level - levelThreshold) / 24.0f, 0.0f, 1.0f);
            return 0.45f + 0.90f * levelExcess;
        };
        const bool warmedUp = frame_ >= 12;
        if (warmedUp && kickCooldown_ == 0 && kickFlux > 2.25f
            && std::max(features_.level[0], features_.level[1]) > 1.08f
            && lowChange > middleChange * 1.25f
            && lowChange > highChange * 1.45f
            && lowEnergy > middleEnergy * 1.20f
            && lowEnergy > highEnergy * 1.40f) {
            features_.kick = true;
            features_.kickImpact = impactStrength(
                std::max(features_.level[0], features_.level[1]), 1.08f);
            kickCooldown_ = 16;
        }
        if (warmedUp && snareCooldown_ == 0 && snareFlux > 1.45f
            && std::max(features_.level[3], features_.level[4]) > 1.03f
            && middleChange > lowChange * 0.72f
            && middleChange > highChange * 0.92f) {
            features_.snare = true;
            features_.snareImpact = impactStrength(
                std::max(features_.level[3], features_.level[4]), 1.06f);
            snareCooldown_ = 10;
        }
        if (warmedUp && hatCooldown_ == 0 && hatFlux > 2.0f
            && features_.level[5] > 1.05f
            && highChange > lowChange * 0.55f
            && highChange > middleChange * 0.88f) {
            features_.hat = true;
            features_.hatImpact = impactStrength(features_.level[5], 1.05f);
            hatCooldown_ = 5;
        }
        if (kickCooldown_ > 0) --kickCooldown_;
        if (snareCooldown_ > 0) --snareCooldown_;
        if (hatCooldown_ > 0) --hatCooldown_;
        updateClock();
        ++frame_;
        return features_;
    }

    void updateClock() {
        std::move(onsetHistory_.begin() + 1, onsetHistory_.end(), onsetHistory_.begin());
        float onsetStrength = 0.0f;
        for (float flux : features_.flux) {
            onsetStrength += std::log1p(std::clamp(flux - 0.8f, 0.0f, 5.0f));
        }
        onsetHistory_.back() = onsetStrength;
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
                                             / (2.5f * 2.5f));
                const float score = normalized * prior;
                lagScores[lag] = score;
                if (score > bestScore) {
                    bestScore = score;
                    bestLag = lag;
                }
            }
            if (bestScore > 0.08f) {
                for (int lag = minimumLag; lag <= maximumLag; ++lag) {
                    tempoVotes_[lag] = tempoVotes_[lag] * 0.999f + lagScores[lag];
                }
                bestLag = minimumLag;
                for (int lag = minimumLag + 1; lag <= maximumLag; ++lag) {
                    if (tempoVotes_[lag] > tempoVotes_[bestLag]) bestLag = lag;
                }
                const float votedBpm = 3600.0f / bestLag;
                const int votedDoubleTempoLag = static_cast<int>(std::round(bestLag * 0.5f));
                if (votedBpm < 90.0f && votedDoubleTempoLag >= minimumLag
                    && tempoVotes_[votedDoubleTempoLag] >= tempoVotes_[bestLag] * 0.75f) {
                    bestLag = votedDoubleTempoLag;
                }

                float estimatedLag = static_cast<float>(bestLag);
                if (bestLag > minimumLag && bestLag < maximumLag) {
                    const float y0 = tempoVotes_[bestLag - 1];
                    const float y1 = tempoVotes_[bestLag];
                    const float y2 = tempoVotes_[bestLag + 1];
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
                for (float& vote : tempoVotes_) vote *= 0.999f;
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
    std::array<float, AudioFeatures::spectrumCount> previousSpectrum_{};
    std::array<float, AudioFeatures::spectrumCount> spectrumFluxMean_{};
    std::array<float, AudioFeatures::spectrumCount> spectrumLevelMean_{};
    std::array<std::array<float, AudioFeatures::spectrumCount>, 17> spectrumHistory_{};
    std::size_t spectrumHistoryCursor_ = 0;
    std::size_t spectrumHistoryFrames_ = 0;
    float pendingStereoWidth_ = 0.0f;
    std::uint64_t processedFrames_ = 0;
    fftwf_plan plan_ = nullptr;
    AudioFeatures features_{};
    int frame_ = 0;
    int kickCooldown_ = 0;
    int snareCooldown_ = 0;
    int hatCooldown_ = 0;
    std::array<float, 3600> onsetHistory_{};
    std::array<float, 59> tempoVotes_{};
    int historyFrames_ = 0;
    float beatPeriod_ = 30.0f;
    int beatIndex_ = 0;
};
