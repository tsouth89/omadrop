#!/bin/bash
set -euo pipefail
here=$(dirname "$(readlink -f "$0")")
preset_dir=$(readlink -f "$here/../../presets/curated")
presets=("$preset_dir"/*.milk)

exec "$here/projectm-ascii-live" "${presets[@]}"
