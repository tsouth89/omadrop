#pragma once

#include <string>

inline void applyOmadropAdapter(const std::string& filename, std::string& preset) {
    if (filename.find("night cathedral") != std::string::npos) {
        const std::string original = "per_frame_72=zoom = 1.2 + sin(time/17)*.3;";
        if (const auto at = preset.find(original); at != std::string::npos) {
            preset.replace(at, original.size(),
                           "per_frame_72=zoom=1.2+sin(time/17)*.3+0.075*q24;");
        }
    }
}
