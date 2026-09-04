#pragma once

#include "music_frame.h"
#include "native_scene_state.h"

#include <GL/glew.h>

#include <array>
#include <filesystem>
#include <string>

class NativeRenderer {
public:
    NativeRenderer() = default;
    ~NativeRenderer();
    NativeRenderer(const NativeRenderer&) = delete;
    NativeRenderer& operator=(const NativeRenderer&) = delete;

    bool initialize(const std::filesystem::path& shaderDirectory, std::string& error);
    bool render(const MusicFrame& music, const NativeSceneState& scene,
                int width, int height,
                const std::array<float, 3>& albumColor,
                GLuint artworkTexture, float artworkAspect, float frameSeconds,
                std::string& error);
    GLuint texture(NativeSceneKind scene) const;
    GLuint texture() const { return texture(NativeSceneKind::DepthTunnel); }
    void reset();
    void shutdown();

private:
    bool resize(int width, int height, std::string& error);

    std::array<GLuint, nativeSceneCount> programs_{};
    GLuint vao_ = 0;
    GLuint framebuffer_ = 0;
    std::array<std::array<GLuint, 2>, nativeSceneCount> textures_{};
    std::array<int, nativeSceneCount> activeTextures_{};
    int width_ = 0;
    int height_ = 0;
    float flowTime_ = 0.0f;
};
