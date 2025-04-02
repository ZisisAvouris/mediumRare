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
    //vec3 n;
};

int main( void ) {
    mr::App app = mr::App();
    app.fpsCounter.avgInterval_ = 0.25f;
    app.fpsCounter.printFPS_    = false;

    std::unique_ptr<lvk::IContext> ctx( app.ctx.get() );

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule( ctx, "../shaders/main.vert" );
    lvk::Holder<lvk::ShaderModuleHandle> tesc = loadShaderModule( ctx, "../shaders/main.tesc" );
    lvk::Holder<lvk::ShaderModuleHandle> geom = loadShaderModule( ctx, "../shaders/main.geom" );
    lvk::Holder<lvk::ShaderModuleHandle> tese = loadShaderModule( ctx, "../shaders/main.tese" );
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule( ctx, "../shaders/main.frag" );

    lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
        .topology           = lvk::Topology_Patch,
        .smVert             = vert,
        .smTesc             = tesc,
        .smTese             = tese,
        .smGeom             = geom,
        .smFrag             = frag,
        .color              = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat        = app.getDepthFormat(),
        .patchControlPoints = 3,
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
        const aiVector3D t = mesh->mTextureCoords[0][i];
        vertices.push_back( {
            .pos = vec3(v.x, v.y, v.z), .uv = vec2(t.x, t.y)
        });
    }
    std::vector<uint32_t> indices;
    for ( uint32_t i = 0; i != mesh->mNumFaces; ++i ) {
        for ( uint32_t j = 0; j != 3; ++j ) {
            indices.push_back( mesh->mFaces[i].mIndices[j] );
        }
    }
    aiReleaseImport( scene );

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
    
    struct PerFrameData {
        mat4 model           = mat4( 1.0f );
        mat4 view            = mat4( 1.0f );
        mat4 proj            = mat4( 1.0f );
        vec4 cameraPos       = {};
        u32 texture          = 0;
        f32 tesselationScale = 1.0f;
        u64 vertices         = 0;
    };
    lvk::Holder<lvk::BufferHandle> bufferPerFrame = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Uniform,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(PerFrameData),
        .debugName = "Buffer: per-frame"
    });

    u32 frameId = 0;
    f32 tesselationScale = 1.0f;
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
            const mat4 m = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
            const mat4 v = glm::rotate(glm::translate(mat4(1.0f), vec3(0.0f, -0.5f, -1.5f)), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
            const mat4 p = glm::perspective(45.0f, aspectRatio, 0.1f, 1000.0f);

            const PerFrameData pc = {
                .model            = v * m,
                .view             = app.camera.getViewMatrix(),
                .proj             = p,
                .cameraPos        = vec4( app.camera.getPosition(), 1.0f ),
                .texture          = texture.index(),
                .tesselationScale = tesselationScale,
                .vertices         = ctx->gpuAddress( vertexBuffer )
            };
            buf.cmdUpdateBuffer( bufferPerFrame, pc );

            buf.cmdBeginRendering( renderPass, framebuffer );
                buf.cmdBindIndexBuffer( indexBuffer, lvk::IndexFormat_UI32 );
                buf.cmdBindRenderPipeline( pipeline );
                buf.cmdPushConstants( ctx->gpuAddress( bufferPerFrame ) );
                buf.cmdBindDepthState( { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true } );
                buf.cmdDrawIndexed( indices.size() );

            app.drawGrid( buf, proj );
            app.imgui->beginFrame( framebuffer );
                const ImVec2 statsSize         = mr::ImGuiFPSComponent( app.fpsCounter.getFPS() );
                const ImVec2 camControlSize    = mr::ImGuiCameraControlsComponent( app.cameraPos, app.cameraAngles, app.cameraType, { 10.0f, statsSize.y + mr::COMPONENT_PADDING } );
                const ImVec2 renderOptionsSize = mr::ImGuiRenderOptionsComponent( app.options, { 10.0f, camControlSize.y + mr::COMPONENT_PADDING } );
                if ( app.cameraType == false ) {
                    app.camera = Camera( app.fpsPositioner );
                } else {
                    app.moveToPositioner.setDesiredPosition( app.cameraPos );
                    app.moveToPositioner.setDesiredAngles( app.cameraAngles );
                    app.camera = Camera( app.moveToPositioner );
                }
                ImGui::SetNextWindowPos( { 10.0f, renderOptionsSize.y + mr::COMPONENT_PADDING } );
                ImGui::Begin( "Tesselation", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
                    ImGui::SliderFloat( "Scale:", &tesselationScale, 0.7f, 1.2f, "%0.1f" );
                ImGui::End();
            app.imgui->endFrame( buf );
            buf.cmdEndRendering();
        }
        ctx->submit( buf, ctx->getCurrentSwapchainTexture() );
        frameId = ( frameId + 1 ) & 1;
    });

    ctx.release();
    return 0;
}
