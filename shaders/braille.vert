#version 440
layout(location = 0) in vec4 qt_Vertex;
layout(location = 1) in vec2 qt_MultiTexCoord0;
layout(location = 0) out vec2 vTex;
layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec4 grid;      // cols, rows, atlasCols, atlasRows
    vec4 audio;     // bass, mid, treble, energy
    vec4 anim;      // time, onset, dissolve, sceneMix
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
void main() {
    vTex = qt_MultiTexCoord0;
    gl_Position = qt_Matrix * qt_Vertex;
}
