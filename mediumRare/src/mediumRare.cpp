#include "../include/ImGuiComponents.hpp"
#include "../include/App.hpp"
#include "../include/Mesh.hpp"

const char *meshMeshes = "../.cache/bistro.meshes";
int main( void ) {
    if ( !isMeshDataValid( meshMeshes ) ) {
        printf( "[INFO] No cached mesh data found. Precaching...\n" );
        MeshData meshData;
        loadMeshFile( "../../deps/src/bistro/Exterior/exterior.obj", meshData );
        saveMeshData( meshMeshes, meshData );
    }
    MeshData meshData;
    const MeshFileHeader header = loadMeshData( meshMeshes, meshData );

    mr::App app = mr::App();
    app.fpsCounter.avgInterval_ = 0.25f;
    app.fpsCounter.printFPS_    = false;

    std::unique_ptr<lvk::IContext> ctx( app.ctx.get() );

    const VkMesh mesh( ctx, header, meshData, app.getDepthFormat() );
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

        const mat4 mvp = proj * app.camera.getViewMatrix() * glm::scale( mat4(1.0f), vec3(0.01f) );
        lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer(); {
            buf.cmdBeginRendering( renderPass, framebuffer );
                mesh.draw( buf, header, mvp );

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
