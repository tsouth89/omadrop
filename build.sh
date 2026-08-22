#!/bin/bash
# Qt picks a GLSL ES version at runtime; baking only desktop GLSL leaves it
# with "No GLSL shader code found" and a black screen.
set -e
cd "$(dirname "$(readlink -f "$0")")"
QSB=/usr/lib/qt6/bin/qsb
T="300es,310es,320es,150,440"
$QSB --glsl "$T" --msl 12 -b -o shaders/braille.vert.qsb shaders/braille.vert
$QSB --glsl "$T" --msl 12 -o shaders/field.frag.qsb   shaders/field.frag
$QSB --glsl "$T" --msl 12 -o shaders/display.frag.qsb shaders/display.frag
echo "shaders baked"
