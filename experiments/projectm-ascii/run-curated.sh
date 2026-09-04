#!/bin/bash
set -euo pipefail
here=$(dirname "$(readlink -f "$0")")
preset_dir=$(readlink -f "$here/../../presets/curated")
presets=("$preset_dir"/*.milk)
if [[ ${OMADROP_CLASSIC_CONTORTION:-0} == 1 ]]; then
  classic=$(readlink -f "$here/../../presets/classic/Aderrasi - Contortion (Escher's Tunnel Mix).milk")
  for index in "${!presets[@]}"; do
    if [[ ${presets[$index]} == *"Omadrop + Aderrasi - Contortion (Reactive Tunnel Edition).milk" ]]; then
      presets[$index]=$classic
      break
    fi
  done
fi
if [[ ${OMADROP_CLASSIC_HALLS:-0} == 1 ]]; then
  classic=$(readlink -f "$here/../../presets/classic/Aderrasi - Halls Of Centrifuge.milk")
  for index in "${!presets[@]}"; do
    if [[ ${presets[$index]} == *"Omadrop + Aderrasi - Halls Of Centrifuge (Reactive Orbit Edition).milk" ]]; then
      presets[$index]=$classic
      break
    fi
  done
fi
if [[ ${OMADROP_CLASSIC_WIRE:-0} == 1 ]]; then
  classic=$(readlink -f "$here/../../presets/classic/Martin - wire dance.milk")
  for index in "${!presets[@]}"; do
    if [[ ${presets[$index]} == *"Omadrop + Martin - Wire Dance (Reactive Wire Edition).milk" ]]; then
      presets[$index]=$classic
      break
    fi
  done
fi
if [[ ${OMADROP_CONTORTION_ONLY:-0} == 1 ]]; then
  for preset in "${presets[@]}"; do
    if [[ $preset == *"Contortion"* ]]; then
      presets=("$preset")
      break
    fi
  done
fi
if [[ ${OMADROP_HALLS_ONLY:-0} == 1 ]]; then
  for preset in "${presets[@]}"; do
    if [[ $preset == *"Halls Of Centrifuge"* ]]; then
      presets=("$preset")
      break
    fi
  done
fi
if [[ ${OMADROP_WIRE_ONLY:-0} == 1 ]]; then
  for preset in "${presets[@]}"; do
    if [[ $preset == *"Wire Dance"* || $preset == *"wire dance"* ]]; then
      presets=("$preset")
      break
    fi
  done
fi

exec "$here/projectm-ascii-live" "${presets[@]}"
