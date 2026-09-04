const float tau = 6.28318530718;

mat2 rotate2d(float angle) {
    float c = cos(angle);
    float s = sin(angle);
    return mat2(c, -s, s, c);
}

float line(float value, float width) {
    return 1.0 - smoothstep(width, width * 2.2, abs(value));
}

vec3 hueRotate(vec3 color, float angle) {
    const vec3 axis = vec3(0.57735026919);
    float c = cos(angle);
    return color * c + cross(axis, color) * sin(angle)
         + axis * dot(axis, color) * (1.0 - c);
}

vec3 vividAlbumColor() {
    float maximum = max(albumColor.r, max(albumColor.g, albumColor.b));
    float minimum = min(albumColor.r, min(albumColor.g, albumColor.b));
    float chroma = maximum - minimum;
    // Keep the artwork's hue, but rebuild it from chroma instead of carrying
    // a pale artwork's high floor into every feedback layer. Otherwise the
    // scene palettes add together toward white after only a few frames.
    vec3 saturated = (albumColor - vec3(minimum)) / max(0.001, chroma);
    saturated = mix(vec3(0.025), saturated, 0.94);
    // Neutral artwork still gets an intentional blue-violet base rather than
    // collapsing every native scene to white dots.
    vec3 neutralFallback = vec3(0.08, 0.34, 1.0);
    return mix(neutralFallback, saturated, smoothstep(0.035, 0.18, chroma));
}

vec3 palettePrimary(float sceneHue) {
    return clamp(hueRotate(vividAlbumColor(), sceneHue), 0.035, 1.0);
}

vec3 paletteSecondary(float sceneHue) {
    return clamp(hueRotate(vividAlbumColor(), sceneHue + 2.12), 0.035, 1.0);
}

vec3 paletteAccent(float sceneHue) {
    vec3 complementary = clamp(
        hueRotate(vividAlbumColor(), sceneHue - 1.72), 0.035, 1.0);
    return mix(complementary, vec3(1.0, 0.88, 0.52), 0.22);
}
