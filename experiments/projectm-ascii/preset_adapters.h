#pragma once

#include <array>
#include <string>
#include <utility>

inline void applyOmadropAdapter(const std::string& filename, std::string& preset) {
    if (filename.find("Contortion (Escher's Tunnel Mix)") != std::string::npos) {
        const std::string anchor = "per_frame_10=wave_y = wave_y + 0.0257*cos(time);";
        if (const auto at = preset.find(anchor); at != std::string::npos) {
            const auto end = preset.find('\n', at);
            preset.insert(end == std::string::npos ? preset.size() : end + 1,
                "per_frame_11=q29=min(max(bass-bass_att,0),1.25);\n"
                "per_frame_12=q30=min(max(mid-mid_att,0),1.0);\n");
        }
        const std::array<std::pair<std::string, std::string>, 2> replacements{{
            {"per_pixel_5=rot = rot + above(bass,1)*0.25*(1-rad)*(100*dx_r);",
             "per_pixel_5=rot=rot+above(bass,1)*0.25*(1-rad)*(100*dx_r)"
             "+q29*0.035*(1-rad)*sin(2*ang);"},
            {"per_pixel_6=zoom = zoom + 0.2*(0.5-rad);",
             "per_pixel_6=zoom=zoom+0.2*(0.5-rad)+q29*0.045*(0.58-rad)"
             "+q30*0.012*sin(6*ang)*(1-rad);"},
        }};
        for (const auto& [original, adapted] : replacements) {
            if (const auto at = preset.find(original); at != std::string::npos) {
                preset.replace(at, original.size(), adapted);
            }
        }
    }
}
