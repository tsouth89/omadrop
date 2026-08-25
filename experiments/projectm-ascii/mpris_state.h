#pragma once

#include <json-c/json.h>

#include <cmath>
#include <optional>
#include <string>

struct MprisState {
    std::string identity;
    std::string playbackStatus;
    std::string artPath;
    double positionSeconds = 0.0;
    double durationSeconds = 0.0;
};

inline std::optional<MprisState> parseMprisState(const std::string& input,
                                                 std::string& error) {
    json_tokener* tokener = json_tokener_new();
    json_object* root = json_tokener_parse_ex(
        tokener, input.c_str(), static_cast<int>(input.size()));
    const json_tokener_error parseError = json_tokener_get_error(tokener);
    json_tokener_free(tokener);
    if (parseError != json_tokener_success || !root
        || !json_object_is_type(root, json_type_object)) {
        if (root) json_object_put(root);
        error = "invalid MPRIS state JSON";
        return std::nullopt;
    }

    auto readString = [&](const char* key, std::string& value) {
        json_object* member = nullptr;
        if (!json_object_object_get_ex(root, key, &member)
            || !json_object_is_type(member, json_type_string)) return false;
        value = json_object_get_string(member);
        return true;
    };
    auto readNumber = [&](const char* key, double& value) {
        json_object* member = nullptr;
        if (!json_object_object_get_ex(root, key, &member)
            || (!json_object_is_type(member, json_type_double)
                && !json_object_is_type(member, json_type_int))) return false;
        value = json_object_get_double(member);
        return std::isfinite(value) && value >= 0.0;
    };

    MprisState state;
    if (!readString("identity", state.identity) || state.identity.empty()
        || !readString("playback_status", state.playbackStatus)
        || !readString("art_path", state.artPath)
        || !readNumber("position_seconds", state.positionSeconds)
        || !readNumber("duration_seconds", state.durationSeconds)) {
        json_object_put(root);
        error = "MPRIS state is missing a required field";
        return std::nullopt;
    }
    json_object_put(root);
    return state;
}

struct PlaybackObservation {
    bool first = false;
    bool trackChanged = false;
    bool seeked = false;
};

class PlaybackClock {
public:
    PlaybackObservation observe(const MprisState& state, double monotonicSeconds) {
        PlaybackObservation result;
        result.first = !state_.has_value();
        result.trackChanged = state_ && state_->identity != state.identity;
        if (state_ && !result.trackChanged) {
            result.seeked = std::abs(state.positionSeconds - positionAt(monotonicSeconds)) > 1.5;
        }
        state_ = state;
        observedAt_ = monotonicSeconds;
        return result;
    }

    double positionAt(double monotonicSeconds) const {
        if (!state_) return 0.0;
        const double advance = state_->playbackStatus == "Playing"
            ? std::max(0.0, monotonicSeconds - observedAt_) : 0.0;
        const double position = state_->positionSeconds + advance;
        return state_->durationSeconds > 0.0
            ? std::min(state_->durationSeconds, position) : position;
    }

    const std::string& identity() const {
        static const std::string empty;
        return state_ ? state_->identity : empty;
    }

    bool hasState() const { return state_.has_value(); }

private:
    std::optional<MprisState> state_;
    double observedAt_ = 0.0;
};
