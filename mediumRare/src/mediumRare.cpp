#include "../include/ImGuiComponents.hpp"
#include "../include/App.hpp"
#include "../include/Mesh.hpp"
#include <shared/Scene/SceneUtils.h>
#include <shared/Scene/MergeUtil.h>
#include <shared/LineCanvas.h>

const char *cachedMeshesFilename    = ".cache/cache.meshes";
const char *cachedMaterialsFilename = ".cache/cache.materials";
const char *cachedHierarchyFilename = ".cache/cache.scene";

int main( void ) {
    if ( !isMeshDataValid(cachedMeshesFilename) || !isMeshHierarchyValid(cachedHierarchyFilename) || !isMeshMaterialsValid(cachedMaterialsFilename) ) {
        printf( "[INFO] No cached mesh data found. Precaching...\n" );

        MeshData meshData_Exterior, meshData_Interior;
        Scene scene_Exterior, scene_Interior;

        loadMeshFile( "../../deps/src/bistro/Exterior/exterior.obj", meshData_Exterior, scene_Exterior, false );
        loadMeshFile( "../../deps/src/bistro/Interior/interior.obj", meshData_Interior, scene_Interior, false );

        printf("[Unmerged] scene items: %u\n", (u32)scene_Exterior.hierarchy.size());
        mergeNodesWithMaterial(scene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Orange_Leaves");
        printf("[Merged orange leaves] scene items: %u\n", (u32)scene_Exterior.hierarchy.size());
        mergeNodesWithMaterial(scene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Green_Leaves");
        printf("[Merged green leaves]  scene items: %u\n", (u32)scene_Exterior.hierarchy.size());
        mergeNodesWithMaterial(scene_Exterior, meshData_Exterior, "Foliage_Linde_Tree_Large_Trunk");
        printf("[Merged trunk]  scene items: %u\n", (u32)scene_Exterior.hierarchy.size());

        MeshData meshData;
        Scene scene;

        mergeScenes(
            scene, {
                &scene_Exterior,
                &scene_Interior
            }, {}, {
                static_cast<u32>( meshData_Exterior.meshes.size() ),
                static_cast<u32>( meshData_Interior.meshes.size() )
            }
        );
        mergeMeshData( meshData, { &meshData_Exterior, &meshData_Interior } );
        mergeMaterialLists({
            &meshData_Exterior.materials,
            &meshData_Interior.materials
        }, {
            &meshData_Exterior.textureFiles,
            &meshData_Interior.textureFiles
        }, meshData.materials, meshData.textureFiles );
        scene.localTransform[0] = glm::scale( vec3(0.01f) );
        markAsChanged( scene, 0 );

        recalculateBoundingBoxes( meshData );
        saveMeshData( cachedMeshesFilename, meshData );
        saveMeshDataMaterials( cachedMaterialsFilename, meshData );
        saveScene( cachedHierarchyFilename, scene );
    }

    MeshData meshData;
    const MeshFileHeader header = loadMeshData( cachedMeshesFilename, meshData );
    loadMeshDataMaterials( cachedMaterialsFilename, meshData );

    Scene scene;
    loadScene( cachedHierarchyFilename, scene );

    mr::App app = mr::App();
    app.fpsCounter.avgInterval_ = 0.25f;
    app.fpsCounter.printFPS_    = false;

    LineCanvas3D canvas3d;
    std::unique_ptr<lvk::IContext> ctx( app.ctx.get() );
    s32 selectedNode = -1;

    const VkMesh mesh( ctx, meshData, scene, ctx->getSwapchainFormat(), app.getDepthFormat() );
    app.run( [&]( uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds ) {
        const lvk::RenderPass renderPass = {
            .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
            .depth = {   .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
        };
        const lvk::Framebuffer framebuffer = {
            .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
            .depthStencil =   { .texture = app.getDepthTexture() }
        };

        const mat4 view = app.camera.getViewMatrix();
        const mat4 proj = glm::perspective( 45.0f, aspectRatio, 0.01f, 1000.0f );

        lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer(); {
            buf.cmdBeginRendering( renderPass, framebuffer );
                
                app.drawSkybox( buf, app.camera.getViewMatrix(), proj );
                app.drawGrid( buf, proj );

                buf.cmdPushDebugGroupLabel( "Mesh", 0xFF0000FF );
                    mesh.draw( buf, view, proj, app.options[mr::RendererOption::Wireframe] );
                buf.cmdPopDebugGroupLabel();

            app.imgui->beginFrame( framebuffer );
                canvas3d.clear();
                canvas3d.setMatrix( proj * view );

                if ( app.options[mr::RendererOption::BoundingBox] ) {
                    BoundingBox box;
                    for ( auto &p : scene.meshForNode ) {
                        box = meshData.boxes[p.second];
                        canvas3d.box( scene.globalTransform[p.first], box, vec4(1, 0, 0, 1) );
                    }
                }

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

                const ImVec2 sceneGraphSize = mr::ImGuiSceneGraphComponent( scene, selectedNode, { 10.0f, renderOptionsSize.y + mr::COMPONENT_PADDING } );
                if ( selectedNode > -1 && scene.hierarchy[selectedNode].firstChild < 0 ) {
                    const u32 meshId      = scene.meshForNode[selectedNode];
                    const BoundingBox box = meshData.boxes[meshId];
                    canvas3d.box( scene.globalTransform[selectedNode], box, vec4(0, 1, 0, 1) );
                }
                canvas3d.render( *ctx.get(), framebuffer, buf );
            app.imgui->endFrame( buf );
            buf.cmdEndRendering();
        }
        ctx->submit( buf, ctx->getCurrentSwapchainTexture() );
    });

    ctx.release();
    return 0;
}
