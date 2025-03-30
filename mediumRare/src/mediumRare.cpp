#include <shared/LineCanvas.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include "../include/ImGuiComponents.hpp"
#include "../include/App.hpp"

int main( void ) {
    App app = App();
    app.fpsCounter.avgInterval_ = 0.02f;
    app.fpsCounter.printFPS_    = false;

    LineCanvas3D canvas3d;

    std::unique_ptr<lvk::IContext> ctx( app.ctx.get() );

    lvk::Holder<lvk::ShaderModuleHandle> vert       = loadShaderModule(ctx, "../shaders/main.vert");
    lvk::Holder<lvk::ShaderModuleHandle> frag       = loadShaderModule(ctx, "../shaders/main.frag");
    lvk::Holder<lvk::ShaderModuleHandle> skyboxVert = loadShaderModule( ctx, "../shaders/skybox.vert" );
    lvk::Holder<lvk::ShaderModuleHandle> skyboxFrag = loadShaderModule( ctx, "../shaders/skybox.frag" );

    struct VertexData {
        vec3 pos;
        vec3 n;
        vec2 tc;
    };
    const lvk::VertexInput vdesc = {
        .attributes = { { .location = 0, .format = lvk::VertexFormat::Float3, .offset = offsetof(VertexData, pos) },
                        { .location = 1, .format = lvk::VertexFormat::Float3, .offset = offsetof(VertexData, n)   },
                        { .location = 2, .format = lvk::VertexFormat::Float2, .offset = offsetof(VertexData, tc)  }},
        .inputBindings = { { .stride = sizeof(VertexData) } }
    };

    lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
        .vertexInput = vdesc,
        .smVert      = vert,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .cullMode    = lvk::CullMode_Back,
    });
    LVK_ASSERT( pipeline.valid() );

    lvk::Holder<lvk::RenderPipelineHandle> pipelineSkybox = ctx->createRenderPipeline({
        .smVert      = skyboxVert,
        .smFrag      = skyboxFrag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat()
    });
    LVK_ASSERT( pipelineSkybox.valid() );

    const aiScene *scene = aiImportFile( "../../data/rubber_duck/scene.gltf", aiProcess_Triangulate );
    if ( !scene || !scene->HasMeshes() ) {
        printf("Unable to load rubber duck\n");
        exit( 255 );
    }
    const aiMesh *mesh = scene->mMeshes[0];
    std::vector<VertexData> vertices;
    for ( uint32_t i = 0; i != mesh->mNumVertices; ++i ) {
        const aiVector3D v = mesh->mVertices[i];
        const aiVector3D n = mesh->mNormals[i];
        const aiVector3D t = mesh->mTextureCoords[0][i];
        vertices.push_back( {
            .pos = vec3(v.x, v.y, v.z), .n = vec3(n.x, n.y, n.z), .tc = vec2(t.x, t.y)
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
    const size_t kSizeVertices = sizeof(VertexData) * vertices.size();

    lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Vertex,
        .storage   = lvk::StorageType_Device,
        .size      = kSizeVertices,
        .data      = vertices.data(),
        .debugName = "Buffer: Vertex"
    });
    lvk::Holder<lvk::BufferHandle> indexBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Index,
        .storage   = lvk::StorageType_Device,
        .size      = kSizeIndices,
        .data      = indices.data(),
        .debugName = "Buffer: Index"
    });

    struct PerFrameData {
        mat4 model;
        mat4 view;
        mat4 proj;
        vec4 cameraPos;
        uint32_t tex = 0;
        uint32_t texCube = 0;
    };
    lvk::Holder<lvk::BufferHandle> bufferPerFrame = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Uniform,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(PerFrameData),
        .debugName = "Buffer: per-frame"
    });
    lvk::Holder<lvk::TextureHandle> texture = loadTexture( ctx, "../../data/rubber_duck/textures/Duck_baseColor.png" );

    lvk::Holder<lvk::TextureHandle> cubemapTex; {
        int w, h;
        const float *img = stbi_loadf( "../../data/piazza_bologni_1k.hdr", &w, &h, nullptr, 4 );

        Bitmap in ( w, h, 4, eBitmapFormat_Float, img );
        Bitmap out = convertEquirectangularMapToVerticalCross( in );
        stbi_image_free( (void*)img );
        stbi_write_hdr( "./screenshot.hdr", out.w_, out.h_, out.comp_, (const float*)out.data_.data() );

        Bitmap cubemap = convertVerticalCrossToCubeMapFaces( out );
        cubemapTex = ctx->createTexture({
            .type       = lvk::TextureType_Cube,
            .format     = lvk::Format_RGBA_F32,
            .dimensions = { uint32_t(cubemap.w_), uint32_t(cubemap.h_) },
            .usage      = lvk::TextureUsageBits_Sampled,
            .data       = cubemap.data_.data(),
            .debugName  = "piazza_bologni_1k.hdr"
        });
    }

    app.run( [&]( uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds ) {
        const mat4 p  = glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 1000.0f);
        const mat4 m1 = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
        const mat4 m2 = glm::rotate(mat4(1.0f), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
        const mat4 v  = glm::translate(mat4(1.0f), app.camera.getPosition());

        const PerFrameData pushConstants = {
            .model     = m2 * m1,
            .view      = app.camera.getViewMatrix(),
            .proj      = p,
            .cameraPos = vec4( app.camera.getPosition(), 1.0f ),
            .tex       = texture.index(),
            .texCube   = cubemapTex.index(),
        };
        ctx->upload( bufferPerFrame, &pushConstants, sizeof(pushConstants) );

        const lvk::RenderPass renderPass = {
            .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
            .depth = {   .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
        };
        const lvk::Framebuffer framebuffer = {
            .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
            .depthStencil =   { .texture = app.getDepthTexture() }
        };

        lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer(); {
            buf.cmdBeginRendering( renderPass, framebuffer ); {
                buf.cmdPushDebugGroupLabel( "Skybox", 0xFF0000FF );
                buf.cmdBindRenderPipeline( pipelineSkybox );
                buf.cmdPushConstants( ctx->gpuAddress(bufferPerFrame) );
                buf.cmdDraw( 36 );
                buf.cmdPopDebugGroupLabel();
            } {
                buf.cmdPushDebugGroupLabel( "Duck", 0xFF0000FF );
                buf.cmdBindVertexBuffer( 0, vertexBuffer );
                buf.cmdBindRenderPipeline( pipeline );
                buf.cmdBindDepthState( { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true } );
                buf.cmdBindIndexBuffer( indexBuffer, lvk::IndexFormat_UI32 );
                buf.cmdDrawIndexed( indices.size() );
                buf.cmdPopDebugGroupLabel();
            }

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
                canvas3d.clear();
                canvas3d.setMatrix( pushConstants.proj * pushConstants.view );
                canvas3d.plane( vec3(0,0,0), vec3(1,0,0), vec3(0,0,1), 40, 40, 10.0f, 10.0f, vec4(1,0,0,1), vec4(0,1,0,1) );
                canvas3d.box( mat4(1.0f), BoundingBox( vec3(-2), vec3(2)), vec4(1, 1, 0, 1) );
                canvas3d.frustum(
                  glm::lookAt(vec3(cos(glfwGetTime()), kInitialCameraPos.y, sin(glfwGetTime())), kInitialCameraTarget, vec3(0.0f, 1.0f, 0.0f)),
                  glm::perspective(glm::radians(60.0f), aspectRatio, 0.1f, 30.0f), vec4(1, 1, 1, 1)
                );
                canvas3d.render( *ctx, framebuffer, buf );
            app.imgui->endFrame( buf );
            buf.cmdEndRendering();
        }
        ctx->submit( buf, ctx->getCurrentSwapchainTexture() );
    });

    ctx.release();
    return 0;
}
