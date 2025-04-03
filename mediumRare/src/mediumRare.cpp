#include "../include/ImGuiComponents.hpp"
#include "../include/App.hpp"
#include "../include/Mesh.hpp"

struct Vertex {
    vec3 position;
    vec4 color;
    vec2 uv;
};
struct PerFrameData {
    mat4 mvp;
    vec4 baseColor;
    u32 baseTextureId;
};

int main( void ) {
    mr::App app = mr::App();
    app.fpsCounter.avgInterval_ = 0.25f;
    app.fpsCounter.printFPS_    = false;

    std::unique_ptr<lvk::IContext> ctx( app.ctx.get() );

    const aiScene *scene = aiImportFile( "../../deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/DamagedHelmet.gltf", aiProcess_Triangulate );
    if ( !scene || !scene->HasMeshes() ) {
        exit( 0xFF );
    }
    const aiMesh *mesh = scene->mMeshes[0];

    std::vector<Vertex> vertices;
    vertices.reserve(mesh->mNumVertices);
    for ( u32 i = 0; i != mesh->mNumVertices; ++i ) {
        const aiVector3D v = mesh->mVertices[i];
        const aiColor4D  c = mesh->mColors[0] ? mesh->mColors[0][i] : aiColor4D( 1, 1, 1, 1 );
        const aiVector3D t = mesh->mTextureCoords[0] ? mesh->mTextureCoords[0][i] : aiVector3D( 0, 0, 0 );

        vertices.push_back({
            .position = vec3( v.x, v.y, v.z ),
            .color    = vec4( c.r, c.g, c.b, c.a ),
            .uv       = vec2( t.x, 1.0f - t.y )
        });
    }
    std::vector<u32> indices;
    indices.reserve(mesh->mNumFaces * 3);
    for ( u32 i = 0; i != mesh->mNumFaces; ++i ) {
        for ( u32 j = 0; j != 3; ++j ) {
            indices.push_back( mesh->mFaces[i].mIndices[j] );
        }
    }
    aiReleaseImport( scene );

    lvk::Holder<lvk::TextureHandle> baseColorTexture = loadTexture( ctx, "../../deps/src/glTF-Sample-Assets/Models/DamagedHelmet/glTF/Default_albedo.jpg" );
    if ( baseColorTexture.empty() )
        exit( 0xFF );
    lvk::Holder<lvk::BufferHandle> vertexBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Vertex,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(Vertex) * vertices.size(),
        .data      = vertices.data(),
        .debugName = "Buffer: vertex"
    });
    lvk::Holder<lvk::BufferHandle> indexBuffer = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Index,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(u32) * indices.size(),
        .data      = indices.data(),
        .debugName = "Buffer: index"
    });
    const lvk::VertexInput vdesc = {
        .attributes = {
            { .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0 },
            { .location = 1, .format = lvk::VertexFormat::Float4, .offset = offsetof( Vertex, color ) },
            { .location = 2, .format = lvk::VertexFormat::Float2, .offset = offsetof( Vertex, uv ) }
        },
        .inputBindings = { { .stride = sizeof(Vertex) } }
    };

    lvk::Holder<lvk::ShaderModuleHandle> vert = loadShaderModule( ctx, "../shaders/pbrUnlit.vert" );
    lvk::Holder<lvk::ShaderModuleHandle> frag = loadShaderModule( ctx, "../shaders/pbrUnlit.frag" );

    lvk::Holder<lvk::RenderPipelineHandle> pipeline = ctx->createRenderPipeline({
        .vertexInput = vdesc,
        .smVert      = vert,
        .smFrag      = frag,
        .color       = { { .format = ctx->getSwapchainFormat() } },
        .depthFormat = app.getDepthFormat(),
        .cullMode    = lvk::CullMode_Back
    });

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

        const mat4 m1 = glm::rotate(mat4(1.0f), glm::radians(+90.0f), vec3(1, 0, 0));
        const mat4 m2 = glm::rotate(mat4(1.0f), (float)glfwGetTime() * 0.1f, vec3(0.0f, 1.0f, 0.0f));

        const mat4 mvp = proj * app.camera.getViewMatrix() * m2 * m1;
        lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer(); {
            buf.cmdBeginRendering( renderPass, framebuffer );
                
                app.drawSkybox( buf, app.camera.getViewMatrix(), proj );

                buf.cmdBindVertexBuffer( 0, vertexBuffer, 0 );
                buf.cmdBindIndexBuffer( indexBuffer, lvk::IndexFormat_UI32 );
                buf.cmdBindRenderPipeline( pipeline );
                buf.cmdBindDepthState( { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true } );
                buf.cmdPushConstants( PerFrameData { .mvp = mvp, .baseColor = vec4(1, 1, 1, 1), .baseTextureId = baseColorTexture.index() } );
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
            app.imgui->endFrame( buf );
            buf.cmdEndRendering();
        }
        ctx->submit( buf, ctx->getCurrentSwapchainTexture() );
    });

    ctx.release();
    return 0;
}
