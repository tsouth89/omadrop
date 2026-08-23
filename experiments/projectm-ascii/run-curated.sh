#!/bin/bash
set -euo pipefail
here=$(dirname "$(readlink -f "$0")")
base=/usr/share/projectM/presets/presets_bltc201
tryptonaut=/usr/share/projectM/presets/presets_tryptonaut

exec "$here/projectm-ascii-live" \
  "$base/Aderrasi - Contortion (Escher's Tunnel Mix).milk" \
  "$tryptonaut/Martin - wire dance.milk" \
  "$base/Aderrasi - Halls Of Centrifuge.milk" \
  "$tryptonaut/martin - night cathedral.milk" \
  "$base/Aderrasi - Bitterfeld (Crystal Border Mix).milk" \
  "$base/Aderrasi + Geiss - Airhandler (Kali Mix) - Painterly Kaleidoscope 2.milk"
