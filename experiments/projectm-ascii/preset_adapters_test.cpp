#include "preset_adapters.h"

#include <cassert>
#include <iostream>
#include <string>

int main() {
    std::string contortion = "per_pixel_6=zoom = zoom + 0.2*(0.5-rad);\n";
    const std::string originalContortion = contortion;
    applyOmadropAdapter("Aderrasi - Contortion (Escher's Tunnel Mix).milk", contortion);
    assert(contortion == originalContortion);

    std::string wire = "per_frame_36=zoom = 1.2;\n";
    const std::string originalWire = wire;
    applyOmadropAdapter("Martin - wire dance.milk", wire);
    assert(wire == originalWire);

    std::string cathedral = "per_frame_72=zoom = 1.2 + sin(time/17)*.3;\n";
    applyOmadropAdapter("martin - night cathedral.milk", cathedral);
    assert(cathedral.find("+0.075*q24") != std::string::npos);

    std::cout << "preset adapters passed\n";
}
