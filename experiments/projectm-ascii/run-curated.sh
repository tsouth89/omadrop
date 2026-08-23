#!/bin/bash
set -euo pipefail
here=$(dirname "$(readlink -f "$0")")
base=/usr/share/projectM/presets/presets_bltc201

exec "$here/projectm-ascii-live" \
  "$base/Aderrasi - Contortion (Escher's Tunnel Mix).milk" \
  "$base/Aderrasi - Bitterfeld (Crystal Border Mix).milk" \
  "$base/Aderrasi - Halls Of Centrifuge.milk" \
  "$base/Aderrasi - Songflower (Hybrid Plant).milk" \
  "$base/Unchained - Morat's Final Voyage.milk"
