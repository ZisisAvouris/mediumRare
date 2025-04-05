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
    s32 selectedNode = -1, prevNumSamples = 1;

    const lvk::Dimensions fbSize = ctx->getDimensions( ctx->getCurrentSwapchainTexture() );
    lvk::Holder<lvk::TextureHandle> msaaColor = ctx->createTexture({
        .format     = ctx->getSwapchainFormat(),
        .dimensions = fbSize,
        .numSamples = app._numSamples,
        .usage      = lvk::TextureUsageBits_Attachment,
        .storage    = lvk::StorageType_Memoryless,
        .debugName  = "MSAA: Color"
    });
    lvk::Holder<lvk::TextureHandle> msaaDepth = ctx->createTexture({
        .format     = app.getDepthFormat(),
        .dimensions = fbSize,
        .numSamples = app._numSamples,
        .usage      = lvk::TextureUsageBits_Attachment,
        .storage    = lvk::StorageType_Memoryless,
        .debugName  = "MSAA: Depth"
    });

    LightParams light, prevLight = { .depthBiasConst = 0 };
    lvk::Holder<lvk::TextureHandle> shadowMap = ctx->createTexture({
        .type       = lvk::TextureType_2D,
        .format     = lvk::Format_Z_UN16,
        .dimensions = { 4096, 4096 },
        .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
        .swizzle    = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R, .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 },
        .debugName  = "Shadow Map"
    });
    lvk::Holder<lvk::SamplerHandle> shadowSampler = ctx->createSampler({
        .wrapU               = lvk::SamplerWrap_Clamp,
        .wrapV               = lvk::SamplerWrap_Clamp,
        .depthCompareOp      = lvk::CompareOp_LessEqual,
        .depthCompareEnabled = true,
        .debugName           = "Sampler: shadow"
    });
    lvk::Holder<lvk::BufferHandle> bufferLight = ctx->createBuffer({
        .usage     = lvk::BufferUsageBits_Storage,
        .storage   = lvk::StorageType_Device,
        .size      = sizeof(LightData),
        .debugName = "Buffer: light"
    });

    const VkMesh mesh( ctx, meshData, scene );
    Pipeline shadowPipeline( ctx, meshData.streams, lvk::Format_Invalid, ctx->getFormat(shadowMap), 1,
        loadShaderModule( ctx, "../shaders/shadow.vert"),
        loadShaderModule( ctx, "../shaders/shadow.frag"), lvk::CullMode_None); // Experiment with backface culling here, it seems it makes no difference for bistro
    Pipeline *opaquePipeline = new Pipeline( ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), app._numSamples,
        loadShaderModule( ctx, "../shaders/main.vert" ),
        loadShaderModule( ctx, "../shaders/main.frag" ), lvk::CullMode_Back );

    std::vector<BoundingBox> reorderedBoxes;
    reorderedBoxes.resize( scene.globalTransform.size() );
    for ( auto &p : scene.meshForNode ) {
        reorderedBoxes[p.first] = meshData.boxes[p.second].getTransformed( scene.globalTransform[p.first] );
    }
    BoundingBox bigBoxWS = reorderedBoxes.front();
    for ( const auto &b : reorderedBoxes ) {
        bigBoxWS.combinePoint( b.min_ );
        bigBoxWS.combinePoint( b.max_ );
    }

    const mat4 scaleBias = mat4( 0.5, 0.0, 0.0, 0.0,
                                 0.0, 0.5, 0.0, 0.0,
                                 0.0, 0.0, 1.0, 0.0,
                                 0.5, 0.5, 0.0, 1.0 );

    app.run( [&]( uint32_t width, uint32_t height, float aspectRatio, float deltaSeconds ) {
        
        const lvk::Framebuffer fbMain = { .color = { { .texture = ctx->getCurrentSwapchainTexture() } } };

        const mat4 view = app.camera.getViewMatrix();
        const mat4 proj = glm::perspective( 45.0f, aspectRatio, 0.01f, 200.0f );

        const mat4 rot1      = glm::rotate( mat4(1.0f), glm::radians(light.theta), glm::vec3(0, 1, 0) );
        const mat4 rot2      = glm::rotate( rot1, glm::radians(light.phi), glm::vec3(1, 0, 0) );
        const vec3 lightDir  = glm::normalize( vec3(rot2 * vec4(0.0f, -1.0f, 0.0f, 1.0f)) );
        const mat4 lightView = glm::lookAt( glm::vec3(0.0f), lightDir, vec3(0, 0, 1) );
        
        const BoundingBox boxLS = bigBoxWS.getTransformed( lightView );
        const mat4 lightProj    = glm::orthoLH_ZO( boxLS.min_.x, boxLS.max_.x, boxLS.min_.y, boxLS.max_.y, boxLS.max_.z, boxLS.min_.z );

        s32 updateMaterialIndex = -1;
        lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer(); {
            if ( prevLight != light ) { // Only update shadow map when the light parameters changed
                prevLight = light;
                buf.cmdBeginRendering(
                    lvk::RenderPass  { .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f } },
                    lvk::Framebuffer { .depthStencil = { .texture = shadowMap } }
                );
                buf.cmdPushDebugGroupLabel( "Shadow Pass", 0xFFFF00FF );
                    buf.cmdSetDepthBias( light.depthBiasConst, light.depthBiasSlope );
                    buf.cmdSetDepthBiasEnable( true );
                    mesh.draw( buf, shadowPipeline, lightView, lightProj );
                    buf.cmdSetDepthBiasEnable( false );
                buf.cmdPopDebugGroupLabel();
                buf.cmdEndRendering();
                
                buf.cmdUpdateBuffer( bufferLight, LightData {
                    .viewProjBias  = scaleBias * lightProj * lightView,
                    .lightDir      = vec4( lightDir, 0.0f ),
                    .shadowTexture = shadowMap.index(),
                    .shadowSampler = shadowSampler.index()
                });
            }

            const lvk::RenderPass renderPass = {
                .color = { { .loadOp = lvk::LoadOp_Clear,
                             .storeOp = ( app._numSamples > 1 ) ? lvk::StoreOp_MsaaResolve : lvk::StoreOp_Store,
                             .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f }
                } },
                .depth = {   .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
            };
            const lvk::Framebuffer offscreen = {
                .color = { {
                    .texture        = ( app._numSamples > 1 ) ? msaaColor : ctx->getCurrentSwapchainTexture(),
                    .resolveTexture = ( app._numSamples > 1 ) ? ctx->getCurrentSwapchainTexture() : lvk::TextureHandle{}
                } },
                .depthStencil = { .texture = ( app._numSamples > 1 ) ? msaaDepth : app.getDepthTexture() }
            };
            buf.cmdBeginRendering( renderPass, offscreen, { .textures = lvk::TextureHandle(shadowMap) } );
                app.drawSkybox( buf, view, proj );
                app.drawGrid( buf, proj );

                buf.cmdPushDebugGroupLabel( "Mesh", 0xFF0000FF );
                    const struct {
                        mat4 viewProj;
                        u64  bufferTransforms;
                        u64  bufferDrawData;
                        u64  bufferMaterials;
                        u64  bufferLight;
                        u32  skyboxIrradiance;
                    } pc {
                        .viewProj         = proj * view,
                        .bufferTransforms = ctx->gpuAddress( mesh.bufferTransforms_ ),
                        .bufferDrawData   = ctx->gpuAddress( mesh.bufferDrawData_ ),
                        .bufferMaterials  = ctx->gpuAddress( mesh.bufferMaterials_ ),
                        .bufferLight      = ctx->gpuAddress( bufferLight ),
                        .skyboxIrradiance = app.skyboxIrradiance.index()
                    };
                    static_assert( sizeof(pc) <= 128 );
                    mesh.draw( buf, *opaquePipeline, &pc, sizeof(pc), lvk::DepthState {.compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true},
                        app.options[mr::RendererOption::Wireframe] );
                buf.cmdPopDebugGroupLabel();

                canvas3d.clear();
                canvas3d.setMatrix( proj * view );

                if ( app.options[mr::RendererOption::BoundingBox] ) {
                    BoundingBox box;
                    for ( auto &p : scene.meshForNode ) {
                        box = meshData.boxes[p.second];
                        canvas3d.box( scene.globalTransform[p.first], box, vec4(1, 0, 0, 1) );
                    }
                }
                if ( selectedNode > -1 && scene.hierarchy[selectedNode].firstChild < 0 ) {
                    const u32 meshId      = scene.meshForNode[selectedNode];
                    const BoundingBox box = meshData.boxes[meshId];
                    canvas3d.box( scene.globalTransform[selectedNode], box, vec4(0, 1, 0, 1) );
                }

                if ( app.options[mr::RendererOption::LightFrustum] ) {
                    canvas3d.frustum( lightView, lightProj, vec4(1, 1, 0, 1) );
                }
                canvas3d.render( *ctx.get(), offscreen, buf, app._numSamples );
            buf.cmdEndRendering();

            buf.cmdBeginRendering( lvk::RenderPass {
                .color = { { .loadOp = lvk::LoadOp_Load, .storeOp = lvk::StoreOp_Store, .clearColor =  { 1.0f, 1.0f, 1.0f, 1.0f } } } },
                fbMain
            );
            app.imgui->beginFrame( fbMain );
                const ImVec2 statsSize         = mr::ImGuiFPSComponent( app.fpsCounter.getFPS() );
                const ImVec2 camControlSize    = mr::ImGuiCameraControlsComponent( app.cameraPos, app.cameraAngles, app.cameraType, { 10.0f, statsSize.y + mr::COMPONENT_PADDING } );
                if ( app.cameraType == false ) {
                    app.camera = Camera( app.fpsPositioner );
                } else {
                    app.moveToPositioner.setDesiredPosition( app.cameraPos );
                    app.moveToPositioner.setDesiredAngles( app.cameraAngles );
                    app.camera = Camera( app.moveToPositioner );
                }
                const ImVec2 renderOptionsSize = mr::ImGuiRenderOptionsComponent( app.options, { 10.0f, camControlSize.y + mr::COMPONENT_PADDING } );
                u32 selectedAA = std::find( &app.options[mr::RendererOption::NoAA], &app.options[mr::RendererOption::MSAAx16], true ) - app.options;
                app._numSamples = 1 << ( selectedAA - mr::RendererOption::NoAA );
                if ( prevNumSamples != app._numSamples ) {
                    msaaColor = nullptr;
                    msaaDepth = nullptr;

                    msaaColor = ctx->createTexture({
                        .format     = ctx->getSwapchainFormat(),
                        .dimensions = fbSize,
                        .numSamples = app._numSamples,
                        .usage      = lvk::TextureUsageBits_Attachment,
                        .storage    = lvk::StorageType_Memoryless,
                        .debugName  = "MSAA: Color"
                    });
                    msaaDepth = ctx->createTexture({
                        .format     = app.getDepthFormat(),
                        .dimensions = fbSize,
                        .numSamples = app._numSamples,
                        .usage      = lvk::TextureUsageBits_Attachment,
                        .storage    = lvk::StorageType_Memoryless,
                        .debugName  = "MSAA: Depth"
                    });

                    delete opaquePipeline;
                    opaquePipeline = new Pipeline( ctx, meshData.streams, ctx->getSwapchainFormat(), app.getDepthFormat(), app._numSamples,
                        loadShaderModule( ctx, "../shaders/main.vert" ),
                        loadShaderModule( ctx, "../shaders/main.frag" ), lvk::CullMode_Back );

                    prevNumSamples = app._numSamples;
                }

                const ImVec2 lightControlsSize = mr::ImGuiLightControlsComponent( light, shadowMap.index(), { 10.0f, renderOptionsSize.y + mr::COMPONENT_PADDING } );

                const ImVec2 sceneGraphSize    = mr::ImGuiSceneGraphComponent( scene, selectedNode, { 10.0f, lightControlsSize.y + mr::COMPONENT_PADDING } );
                mr::ImGuiEditNodeComponent( scene, meshData, view, proj, selectedNode, updateMaterialIndex, mesh.textureCache_ );
            app.imgui->endFrame( buf );
            buf.cmdEndRendering();
        }
        ctx->submit( buf, ctx->getCurrentSwapchainTexture() );

        if ( recalculateGlobalTransforms( scene ) ) {
            mesh.updateGlobalTransforms( scene.globalTransform.data(), scene.globalTransform.size() );
        }
        if ( updateMaterialIndex > -1 ) {
            mesh.updateMaterial( meshData.materials.data(), updateMaterialIndex );
        } 
    });

    delete opaquePipeline;

    ctx.release();
    return 0;
}
