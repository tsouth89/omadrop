#version 440
layout(location = 0) in vec2 vTex;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 grid;      // cols, rows, atlasCols, atlasRows
    vec4 audio;     // bass, mid, treble, energy
    vec4 anim;      // time, onset, dissolve, artFade
    vec4 view;      // aspect, zoom, gain, edgeAmount
    vec4 tint;      // rgb tint for desaturated art, a = amount
    vec4 fx;        // vignette strength, reserved, reserved, reserved
    vec4 scene;     // sceneA index, sceneB index, blend, spare
    vec4 pal0;      // dominant colours of the current cover
    vec4 pal1;
    vec4 pal2;
    vec4 pal3;
    vec4 sp0;       // 32-band spectrum, 4 bands per vec4
    vec4 sp1;
    vec4 sp2;
    vec4 sp3;
    vec4 sp4;
    vec4 sp5;
    vec4 sp6;
    vec4 sp7;
    vec4 music;     // beatPhase, beatImpact, beatSwell, beatConf
    vec4 music2;    // percussive, harmonic, barPhase, bpm/200
};
layout(binding = 1) uniform sampler2D fieldTex;   // accumulated field
layout(binding = 2) uniform sampler2D atlasTex;   // braille glyphs

float dotValue(int dx, int dy) {
    if (dx == 0) return dy == 3 ? 64.0 : float(1 << dy);
    return dy == 3 ? 128.0 : float(8 << dy);
}

float hash(vec2 p) { return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453); }

float ign(vec2 p) {
    return fract(52.9829189 * fract(dot(p, vec2(0.06711056, 0.00583715))));
}

// Display pass. The accumulator holds a smooth continuous field; braille
// quantisation happens HERE, as a view of it. Quantising before accumulating
// would stair-step every trail.
void main() {
    vec2 cols = grid.xy;
    vec2 atlas = grid.zw;

    vec2 scaled = vTex * cols;
    vec2 cell = floor(scaled);
    vec2 frac = scaled - cell;

    float idx = 0.0;
    vec3 accum = vec3(0.0);
    float lit = 0.0;
    float weight = 0.0;

    for (int dx = 0; dx < 2; dx++) {
        for (int dy = 0; dy < 4; dy++) {
            vec2 dotPos = cell + vec2((float(dx) + 0.5) / 2.0, (float(dy) + 0.5) / 4.0);
            vec4 s = texture(fieldTex, dotPos / cols);
            // The accumulator saturates toward 1; without a curve here every
            // dot lights and the frame is a uniform haze again.
            float lvl = smoothstep(0.17, 0.72, s.a);
            vec3 col = s.rgb / max(lvl, 0.002);      // undo premultiply

            vec2 dotId = vec2(cell.x * 2.0 + float(dx), cell.y * 4.0 + float(dy));
            float th = ign(dotId) * 0.86 + hash(dotId) * 0.14;
            th -= audio.z * 0.26 * hash(dotId + floor(anim.x * 30.0));

            if (lvl > th) {
                idx += dotValue(dx, dy);
                lit += 1.0;
                accum += col;
                weight += 1.0;
            }
        }
    }

    if (lit < 0.5) { fragColor = vec4(0.0); return; }

    vec3 col = accum / max(weight, 1.0);
    float l = dot(col, vec3(0.299, 0.587, 0.114));
    col = clamp(mix(vec3(l), col, 2.15), 0.0, 1.0);
    col *= 1.05 + 0.45 * (lit / 8.0);

    float ax = mod(idx, atlas.x);
    float ay = floor(idx / atlas.x);
    vec2 inset = mix(vec2(0.02), vec2(0.98), frac);
    vec2 auv = (vec2(ax, ay) + inset) / atlas;
    float mask = texture(atlasTex, auv).a;

    float alpha = mask * qt_Opacity;
    fragColor = vec4(col * alpha, alpha);
}
