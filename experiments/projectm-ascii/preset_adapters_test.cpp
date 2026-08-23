#include "preset_adapters.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    std::string contortion =
        "per_frame_10=wave_y = wave_y + 0.0257*cos(time);\n"
        "per_pixel_5=rot = rot + above(bass,1)*0.25*(1-rad)*(100*dx_r);\n"
        "per_pixel_6=zoom = zoom + 0.2*(0.5-rad);\n";
    applyOmadropAdapter("Aderrasi - Contortion (Escher's Tunnel Mix).milk", contortion);
    assert(contortion.find("q29=min(max(bass-bass_att,0),1.25)") != std::string::npos);
    assert(contortion.find("q30=min(max(mid-mid_att,0),1.0)") != std::string::npos);
    assert(contortion.find("+q29*0.035") != std::string::npos);
    assert(contortion.find("+q30*0.012") != std::string::npos);

    std::string wire = "per_frame_36=zoom = 1.2;\n";
    applyOmadropAdapter("Martin - wire dance.milk", wire);
    assert(wire.find("zoom=1.16+0.08*q24") != std::string::npos);

    std::string cathedral = "per_frame_72=zoom = 1.2 + sin(time/17)*.3;\n";
    applyOmadropAdapter("martin - night cathedral.milk", cathedral);
    assert(cathedral.find("+0.075*q24") != std::string::npos);

    std::cout << "preset adapters passed\n";
}
