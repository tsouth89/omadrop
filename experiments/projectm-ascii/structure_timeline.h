#pragma once

#include <json-c/json.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

struct TimelineSection {
    double start = 0.0;
    double end = 0.0;
    std::string identity;
    std::string label;
    float confidence = 1.0f;
};

class StructureTimeline {
public:
    bool load(const std::filesystem::path& path, std::string& error) {
        std::ifstream input(path);
        if (!input) {
            error = "could not open timeline: " + path.string();
            return false;
        }
        std::ostringstream contents;
        contents << input.rdbuf();
        const std::string inputText = contents.str();

        json_tokener* tokener = json_tokener_new();
        json_object* root = json_tokener_parse_ex(
            tokener, inputText.c_str(), static_cast<int>(inputText.size()));
        const json_tokener_error parseError = json_tokener_get_error(tokener);
        json_tokener_free(tokener);
        if (parseError != json_tokener_success || !root) {
            if (root) json_object_put(root);
            error = "invalid timeline JSON: " + std::string(json_tokener_error_desc(parseError));
            return false;
        }

        const bool loaded = loadObject(root, error);
        json_object_put(root);
        if (loaded) sourcePath_ = path;
        return loaded;
    }

    const std::vector<TimelineSection>& sections() const { return sections_; }
    double duration() const { return duration_; }
    const std::string& trackIdentity() const { return trackIdentity_; }
    const std::filesystem::path& sourcePath() const { return sourcePath_; }

    bool appliesTo(const std::string& identity) const {
        return trackIdentity_.empty() || trackIdentity_ == identity;
    }

    std::optional<std::size_t> sectionAt(double seconds) const {
        if (!std::isfinite(seconds) || seconds < 0.0 || sections_.empty()) return std::nullopt;
        const auto after = std::upper_bound(
            sections_.begin(), sections_.end(), seconds,
            [](double time, const TimelineSection& section) { return time < section.start; });
        if (after == sections_.begin()) return std::nullopt;
        const auto section = std::prev(after);
        if (seconds >= section->end) return std::nullopt;
        return static_cast<std::size_t>(std::distance(sections_.begin(), section));
    }

private:
    static bool number(json_object* object, const char* key, double& value) {
        json_object* member = nullptr;
        if (!json_object_object_get_ex(object, key, &member)
            || (!json_object_is_type(member, json_type_double)
                && !json_object_is_type(member, json_type_int))) return false;
        value = json_object_get_double(member);
        return std::isfinite(value);
    }

    static bool text(json_object* object, const char* key, std::string& value, bool required) {
        json_object* member = nullptr;
        if (!json_object_object_get_ex(object, key, &member)) return !required;
        if (!json_object_is_type(member, json_type_string)) return false;
        value = json_object_get_string(member);
        return !required || !value.empty();
    }

    bool loadObject(json_object* root, std::string& error) {
        if (!json_object_is_type(root, json_type_object)) {
            error = "timeline root must be an object";
            return false;
        }

        double duration = 0.0;
        if (!number(root, "duration", duration) || duration <= 0.0) {
            error = "timeline duration must be a positive number";
            return false;
        }

        std::string trackIdentity;
        json_object* track = nullptr;
        if (json_object_object_get_ex(root, "track", &track)) {
            if (!json_object_is_type(track, json_type_object)
                || !text(track, "identity", trackIdentity, false)) {
                error = "timeline track.identity must be a string";
                return false;
            }
        }

        json_object* sections = nullptr;
        if (!json_object_object_get_ex(root, "sections", &sections)
            || !json_object_is_type(sections, json_type_array)
            || json_object_array_length(sections) == 0) {
            error = "timeline sections must be a non-empty array";
            return false;
        }

        std::vector<TimelineSection> parsed;
        double previousEnd = 0.0;
        const std::size_t count = json_object_array_length(sections);
        parsed.reserve(count);
        for (std::size_t index = 0; index < count; ++index) {
            json_object* item = json_object_array_get_idx(sections, index);
            TimelineSection section;
            double confidence = 1.0;
            if (!item || !json_object_is_type(item, json_type_object)
                || !number(item, "start", section.start)
                || !number(item, "end", section.end)
                || !text(item, "identity", section.identity, true)
                || !text(item, "label", section.label, false)) {
                error = "timeline section " + std::to_string(index)
                      + " has invalid start, end, identity, or label";
                return false;
            }
            json_object* confidenceValue = nullptr;
            if (json_object_object_get_ex(item, "confidence", &confidenceValue)) {
                if (!number(item, "confidence", confidence)
                    || confidence < 0.0 || confidence > 1.0) {
                    error = "timeline section " + std::to_string(index)
                          + " confidence must be between 0 and 1";
                    return false;
                }
            }
            if (section.start < previousEnd || section.end <= section.start
                || section.end > duration + 0.001) {
                error = "timeline section " + std::to_string(index)
                      + " must be ordered, non-overlapping, and inside duration";
                return false;
            }
            section.confidence = static_cast<float>(confidence);
            previousEnd = section.end;
            parsed.push_back(std::move(section));
        }

        duration_ = duration;
        trackIdentity_ = std::move(trackIdentity);
        sections_ = std::move(parsed);
        return true;
    }

    double duration_ = 0.0;
    std::string trackIdentity_;
    std::filesystem::path sourcePath_;
    std::vector<TimelineSection> sections_;
};

struct TimelineDecision {
    bool initial = false;
    bool sectionChanged = false;
    bool hardSync = false;
    std::size_t sectionIndex = 0;
    std::size_t targetPreset = 0;
};

class TimelineDirector {
public:
    void reset() {
        activeSection_.reset();
        activePreset_.reset();
        familyByIdentity_.clear();
    }

    template <typename ChoosePreset, typename FamilyForPreset>
    std::optional<TimelineDecision> sync(const StructureTimeline& timeline,
                                         double positionSeconds,
                                         std::size_t currentPreset,
                                         std::size_t presetCount,
                                         bool hardSync,
                                         ChoosePreset choosePreset,
                                         FamilyForPreset familyForPreset) {
        const auto sectionIndex = timeline.sectionAt(positionSeconds);
        if (!sectionIndex || presetCount == 0) return std::nullopt;
        const TimelineSection& section = timeline.sections()[*sectionIndex];
        const bool initial = !activeSection_.has_value();
        const bool changed = initial || *activeSection_ != *sectionIndex;
        activeSection_ = *sectionIndex;

        if (changed || !activePreset_) {
            auto family = familyByIdentity_.find(section.identity);
            const int preferredFamily = family == familyByIdentity_.end() ? -1 : family->second;
            const std::size_t chosen = initial ? currentPreset % presetCount
                : choosePreset(preferredFamily) % presetCount;
            activePreset_ = chosen;
            familyByIdentity_.try_emplace(section.identity, familyForPreset(chosen));
        }

        return TimelineDecision{
            .initial = initial,
            .sectionChanged = changed,
            .hardSync = hardSync,
            .sectionIndex = *sectionIndex,
            .targetPreset = *activePreset_,
        };
    }

private:
    std::optional<std::size_t> activeSection_;
    std::optional<std::size_t> activePreset_;
    std::unordered_map<std::string, int> familyByIdentity_;
};
