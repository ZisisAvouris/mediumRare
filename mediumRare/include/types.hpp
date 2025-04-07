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
