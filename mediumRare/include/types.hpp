#pragma once

#include <stdint.h>
#include <glm/glm.hpp>

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t   u8;

typedef int64_t s64;
typedef int32_t s32;
typedef int16_t s16;
typedef int8_t   s8;

typedef double f64;
typedef float  f32;

struct LightParams {
    f32 theta =  90.0f;
    f32 phi   = -26.0f;
    f32 depthBiasConst = 1.1f;
    f32 depthBiasSlope = 2.0f;

    bool operator==( const LightParams& ) const = default;
};

struct LightData {
    glm::mat4 viewProjBias;
    glm::vec4 lightDir;
    u32       shadowTexture;
    u32       shadowSampler;
};

struct SSAOpc {
    u32 textureDepth;
    u32 textureRot;
    u32 textureOut;
    u32 sampler;
    f32 zNear;
    f32 zFar;
    f32 radius;
    f32 attScale;
    f32 distScale;
};

struct CombinePC {
    u32 textureColor;
    u32 textureSSAO;
    u32 sampler;
    f32 scale;
    f32 bias;
};

struct BrightPassPC {
    u32 texColor;
    u32 texOut;
    u32 texLuminance;
    u32 sampler;
    f32 exposure;
};

struct ToneMapPC {
    u32 texColor;
    u32 texLuminance;
    u32 texBloom;
    u32 sampler;
    u32 tonemapMode = 1;
    f32 exposure = 0.95f;
    f32 bloomStrength = 0.5f;
        
    // Reinhard
    f32 maxWhite = 1.0f;

    // Uchimura
    f32 P = 1.0f;
    f32 a = 1.05f;
    f32 m = 0.1f;
    f32 l = 0.8f;
    f32 c = 3.0f;
    f32 b = 0.0f;

    // Khronos PBR
    f32 startCompression = 0.8f;
    f32 desaturation     = 0.15f;
};
