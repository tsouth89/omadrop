#!/bin/bash
set -euo pipefail
cd "$(dirname "$(readlink -f "$0")")"
# Arch's projectM-4.pc currently emits -l:projectM-4, but the installed shared
# object follows the normal libprojectM-4.so naming convention.
g++ -std=c++20 -O2 -Wall -Wextra main.cpp -o projectm-ascii \
  $(pkg-config --cflags projectM-4 sdl2) \
  $(pkg-config --libs sdl2) -lprojectM-4 -lGL

g++ -std=c++20 -O2 -Wall -Wextra live.cpp -o projectm-ascii-live \
  $(pkg-config --cflags projectM-4 sdl2 glew libpng fftw3f json-c) \
  $(pkg-config --libs sdl2 glew libpng fftw3f json-c) -lprojectM-4 -lGL

g++ -std=c++20 -O2 -Wall -Wextra audio_features_test.cpp \
  -o audio-features-test $(pkg-config --cflags --libs fftw3f)

g++ -std=c++20 -O2 -Wall -Wextra preset_profiles_test.cpp \
  -o preset-profiles-test

g++ -std=c++20 -O2 -Wall -Wextra preset_selector_test.cpp \
  -o preset-selector-test

g++ -std=c++20 -O2 -Wall -Wextra preset_adapters_test.cpp \
  -o preset-adapters-test

g++ -std=c++20 -O2 -Wall -Wextra audio_queue_test.cpp \
  -o audio-queue-test

g++ -std=c++20 -O2 -Wall -Wextra structure_timeline_test.cpp \
  -o structure-timeline-test $(pkg-config --cflags --libs json-c)

g++ -std=c++20 -O2 -Wall -Wextra musical_structure_test.cpp \
  -o musical-structure-test

g++ -std=c++20 -O2 -Wall -Wextra audio_replay.cpp \
  -o audio-feature-replay $(pkg-config --cflags --libs fftw3f)
