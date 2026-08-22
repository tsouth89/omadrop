#!/bin/bash
# Qt picks a GLSL ES version at runtime; baking only desktop GLSL leaves it
# with "No GLSL shader code found" and a black screen.
set -e
cd "$(dirname "$(readlink -f "$0")")"
QSB=/usr/lib/qt6/bin/qsb
$QSB --glsl "300es,310es,320es,150,440" --msl 12 -b -o shaders/braille.vert.qsb shaders/braille.vert
$QSB --glsl "300es,310es,320es,150,440" --msl 12 -o shaders/braille.frag.qsb shaders/braille.frag
echo "shaders baked"
