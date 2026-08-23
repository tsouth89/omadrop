#!/bin/bash
set -euo pipefail
here=$(dirname "$(readlink -f "$0")")
presets=$(readlink -f "$here/../../presets/curated")

exec "$here/projectm-ascii-live" \
  "$presets/Aderrasi - Contortion (Escher's Tunnel Mix).milk" \
  "$presets/Martin - wire dance.milk" \
  "$presets/Aderrasi - Halls Of Centrifuge.milk" \
  "$presets/martin - night cathedral.milk" \
  "$presets/Aderrasi - Bitterfeld (Crystal Border Mix).milk" \
  "$presets/Aderrasi + Geiss - Airhandler (Kali Mix) - Painterly Kaleidoscope 2.milk"
