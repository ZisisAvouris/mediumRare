#include <shared/LineCanvas.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <meshoptimizer/src/meshoptimizer.h>

#include "../include/ImGuiComponents.hpp"
#include "../include/App.hpp"

struct Vertex {
    vec3 pos;
    vec2 uv;
    vec3 n;
};
static_assert( sizeof(Vertex) == 8 * sizeof(f32) );

int main( void ) {
    mr::App app = mr::App();
    app.fpsCounter.avgInterval_ = 0.25f;
    app.fpsCounter.printFPS_    = false;

    std::unique_ptr<lvk::IContext> ctx( app.ctx.get() );

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule( ctx, "../shaders/main.vert" );
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule( ctx, "../shaders/main.frag" );

    lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
        .smVert      = vert,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .cullMode    = lvk::CullMode_Back
    });
    LVK_ASSERT( pipeline.valid() );

    lvk::Holder<lvk::TextureHandle> texture = loadTexture( ctx, "../../data/rubber_duck/textures/Duck_baseColor.png" );
    const aiScene *scene = aiImportFile( "../../data/rubber_duck/scene.gltf", aiProcess_Triangulate );
    if ( !scene || !scene->HasMeshes() ) {
        printf("Unable to load rubber duck\n");
        exit( 255 );
    }
    const aiMesh *mesh = scene->mMeshes[0];
    std::vector<Vertex> vertices;
    for ( uint32_t i = 0; i != mesh->mNumVertices; ++i ) {
        const aiVector3D v = mesh->mVertices[i];
        const aiVector3D n = mesh->mNormals[i];
        const aiVector3D t = mesh->mTextureCoords[0][i];
        vertices.push_back( {
            .pos = vec3(v.x, v.y, v.z), .uv = vec2(t.x, t.y), .n = vec3(n.x, n.y, n.z)
        });
    }
    std::vector<uint32_t> indices;
    for ( uint32_t i = 0; i != mesh->mNumFaces; ++i ) {
        for ( uint32_t j = 0; j != 3; ++j ) {
            indices.push_back( mesh->mFaces[i].mIndices[j] );
        }
    }
    aiReleaseImport( scene );

    // Mesh optimizations (TODO: Move this to a Model/Mesh class)
    std::vector<u32> indicesLod; {
        std::vector<u32> remap( indices.size() );
        const size_t vertexCount = meshopt_generateVertexRemap( remap.data(), indices.data(), indices.size(), vertices.data(), indices.size(), sizeof(Vertex) );

        std::vector<u32> remappedIndices( indices.size() );
        std::vector<Vertex> remappedVertices( vertexCount );
        
        meshopt_remapIndexBuffer( remappedIndices.data(), indices.data(), indices.size(), remap.data() );
        meshopt_remapVertexBuffer( remappedVertices.data(), vertices.data(), vertices.size(), sizeof(Vertex), remap.data() );

        meshopt_optimizeVertexCache( remappedIndices.data(), remappedIndices.data(), indices.size(), vertexCount );
        meshopt_optimizeOverdraw( remappedIndices.data(), remappedIndices.data(), indices.size(), glm::value_ptr(remappedVertices[0].pos), vertexCount, sizeof(Vertex), 1.05f );

        meshopt_optimizeVertexFetch( remappedVertices.data(), remappedIndices.data(), indices.size(), remappedVertices.data(), vertexCount, sizeof(Vertex) );

        const float threshold         = 0.2f;
        const size_t targetIndexCount = size_t(remappedIndices.size() * threshold);
        const float targetError       = 0.01f;

        indicesLod.resize( remappedIndices.size() );
        indicesLod.resize( meshopt_simplify(
            &indicesLod[0], remappedIndices.data(), remappedIndices.size(), &remappedVertices[0].pos.x, vertexCount, sizeof(Vertex), targetIndexCount, targetError
        ));
        indices  = remappedIndices;
        vertices = remappedVertices;
    }

    const size_t kSizeIndices = sizeof(uint32_t) * indices.size();
    const size_t kSizeVertices = sizeof(Vertex) * vertices.size();

    lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Storage,
        .storage   = lvk::StorageType_Device,
        .size      = kSizeVertices,
        .data      = vertices.data(),
        .debugName = "Buffer: Vertex"
    }, nullptr );
    lvk::Holder<lvk::BufferHandle> indexBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Index,
        .storage   = lvk::StorageType_Device,
        .size      = kSizeIndices,
        .data      = indices.data(),
        .debugName = "Buffer: Index"
    }, nullptr );
    lvk::Holder<lvk::BufferHandle> indexBufferLod = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Index,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(u32) * indicesLod.size(),
        .data      = indicesLod.data(),
        .debugName = "Buffer: Index LOD"
    }, nullptr );

    const u32 numMeshes = 32 * 1024;
    std::vector<vec4> centers( numMeshes );
    for ( vec4 &p : centers ) {
        p = vec4( glm::linearRand( -vec3(500.0f), vec3(500.0f) ), glm::linearRand( 0.0f, 3.14159f ) );
    }
    lvk::Holder<lvk::BufferHandle> bufferPosAngle = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Storage,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(vec4) * numMeshes,
        .data      = centers.data(),
        .debugName = "Buffer: angles & positions"
    });
    lvk::Holder<lvk::BufferHandle> bufferMatrices[] = {
        ctx->createBuffer({
            .usage     = lvk::BufferUsageBits_Storage,
            .storage   = lvk::StorageType_Device,
            .size      = sizeof(mat4) * numMeshes,
            .debugName = "Buffer: matrices 1"
        }),
        ctx->createBuffer({
            .usage     = lvk::BufferUsageBits_Storage,
            .storage   = lvk::StorageType_Device,
            .size      = sizeof(mat4) * numMeshes,
            .debugName = "Buffer: matrices 2"
        })
    };
    lvk::Holder<lvk::ShaderModuleHandle> compute = loadShaderModule( ctx, "../shaders/main.comp" );
    lvk::Holder<lvk::ComputePipelineHandle> pipelineComputeMat = ctx->createComputePipeline({
        .smComp = compute
    });
    LVK_ASSERT( pipelineComputeMat.valid() );

    u32 frameId = 0;
    app.run( [&]( uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds ) {
        const mat4 proj = glm::perspective( 45.0f, aspectRatio, 0.1f, 1500.0f );
        const lvk::RenderPass renderPass = {
            .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
            .depth = {   .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
        };
        const lvk::Framebuffer framebuffer = {
            .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
            .depthStencil =   { .texture = app.getDepthTexture() }
        };

        lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer(); {
            const mat4 view = glm::translate( mat4(1.0f), vec3(0.0f, 0.0f, -1000.0f + 500.0f * (1.0f - cos(-glfwGetTime() * 0.5f))) );
            const struct {
                mat4 viewproj;
                u32 textureId;
                u64 bufferPosAngle;
                u64 bufferMat;
                u64 bufferVertices;
                f32 time;
            } pc {
                .viewproj       = proj * view,
                .textureId      = texture.index(),
                .bufferPosAngle = ctx->gpuAddress( bufferPosAngle ),
                .bufferMat      = ctx->gpuAddress( bufferMatrices[frameId] ),
                .bufferVertices = ctx->gpuAddress( vertexBuffer ),
                .time           = (f32)glfwGetTime()
            };
            buf.cmdBindComputePipeline( pipelineComputeMat );
            buf.cmdPushConstants( pc );
            buf.cmdDispatchThreadGroups( { .width = numMeshes / 32 } );

            buf.cmdBeginRendering( renderPass, framebuffer, { .buffers = lvk::BufferHandle( bufferMatrices[frameId] ) } );
            buf.cmdPushDebugGroupLabel( "Duck", 0xFF0000FF );
                buf.cmdBindRenderPipeline( pipeline );
                buf.cmdPushConstants( pc );
                buf.cmdBindDepthState( { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true } );
                buf.cmdBindIndexBuffer( indexBufferLod, lvk::IndexFormat_UI32 );
                buf.cmdDrawIndexed( indicesLod.size(), numMeshes );
            buf.cmdPopDebugGroupLabel();

            app.drawGrid( buf, proj );

            app.imgui->beginFrame( framebuffer );
                const ImVec2 statsSize = mr::ImGuiFPSComponent( app.fpsCounter.getFPS() );
                mr::ImGuiCameraControlsComponent( app.cameraPos, app.cameraAngles, app.cameraType, { 10.0f, statsSize.y + mr::COMPONENT_PADDING } );
                if ( app.cameraType == false ) {
                    app.camera = Camera( app.fpsPositioner );
                } else {
                    app.moveToPositioner.setDesiredPosition( app.cameraPos );
                    app.moveToPositioner.setDesiredAngles( app.cameraAngles );
                    app.camera = Camera( app.moveToPositioner );
                }
            app.imgui->endFrame( buf );
            buf.cmdEndRendering();
        }
        ctx->submit( buf, ctx->getCurrentSwapchainTexture() );
        frameId = ( frameId + 1 ) & 1;
    });

    ctx.release();
    return 0;
}
