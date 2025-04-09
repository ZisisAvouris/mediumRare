#include "../include/ImGuiComponents.hpp"
#include "../include/App.hpp"
#include "../include/Mesh.hpp"
#include <shared/Scene/SceneUtils.h>
#include <shared/Scene/MergeUtil.h>
#include <shared/LineCanvas.h>
#include <shared/UtilsMath.h>

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
    s32 selectedNode = -1, prevNumSamples = 1, numBlurPassesSSAO = 1, numBlurPassesBloom = 1;
    const lvk::Format kOffscreenFormat = lvk::Format_RGBA_F16;

    const lvk::Dimensions fbSize        = ctx->getDimensions( ctx->getCurrentSwapchainTexture() );
    const lvk::Dimensions offscreenSize = fbSize;

    lvk::Holder<lvk::TextureHandle> msaaColor = ctx->createTexture({
        .format     = kOffscreenFormat,
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

    lvk::Holder<lvk::TextureHandle> offscreenColor = ctx->createTexture({
        .format     = kOffscreenFormat,
        .dimensions = offscreenSize,
        .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Storage | lvk::TextureUsageBits_Sampled,
        .debugName  = "Buffer: offscreen color"
    });
    lvk::Holder<lvk::TextureHandle> offscreenDepth = ctx->createTexture({
        .format     = app.getDepthFormat(),
        .dimensions = offscreenSize,
        .usage      = lvk::TextureUsageBits_Attachment | lvk::TextureUsageBits_Sampled,
        .debugName  = "Buffer: offscreen depth"
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

    lvk::Holder<lvk::ShaderModuleHandle> compSSAO = loadShaderModule( ctx, "../shaders/SSAO.comp" );
    lvk::Holder<lvk::ComputePipelineHandle> pipelineSSAO = ctx->createComputePipeline( { .smComp = compSSAO } );

    struct BlurPass {
        lvk::TextureHandle textureIn;
        lvk::TextureHandle textureOut;
    };
    std::vector<BlurPass> blurPassesSSAO( 2 * 5 );  // Maximum number of blur passes for SSAO is 5
    std::vector<BlurPass> blurPassesBloom( 2 * 5 ); // Maximum number of blur passes for Bloom is 5
    lvk::Holder<lvk::ShaderModuleHandle> compBlur = loadShaderModule( ctx, "../../data/shaders/Blur.comp" );
    const u32 kHorizontal = 1, kVertical = 0;
    lvk::Holder<lvk::ComputePipelineHandle> pipelineBlurX = ctx->createComputePipeline({
        .smComp   = compBlur,
        .specInfo = {
            .entries = { {
                .constantId = 0,
                .size       = sizeof(u32)
            } },
            .data     = &kHorizontal,
            .dataSize = sizeof(u32)
        }
    });
    lvk::Holder<lvk::ComputePipelineHandle> pipelineBlurY = ctx->createComputePipeline({
        .smComp   = compBlur,
        .specInfo = {
            .entries = { {
                .constantId = 0,
                .size       = sizeof(u32)
            } },
            .data     = &kVertical,
            .dataSize = sizeof(u32)
        } 
    });

    lvk::Holder<lvk::ShaderModuleHandle> vertCombine = loadShaderModule( ctx, "../../data/shaders/QuadFlip.vert" );
    lvk::Holder<lvk::ShaderModuleHandle> fragCombine = loadShaderModule( ctx, "../shaders/combine.frag" );
    lvk::Holder<lvk::RenderPipelineHandle> pipelineCombine = ctx->createRenderPipeline({
        .smVert = vertCombine,
        .smFrag = fragCombine,
        .color  = {{ .format = kOffscreenFormat }}
    });

    lvk::Holder<lvk::TextureHandle> textureSSAO = ctx->createTexture({
        .format     = ctx->getSwapchainFormat(),
        .dimensions = fbSize,
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "Texture SSAO"
    });
    lvk::Holder<lvk::TextureHandle> textureBlur[] = {
        ctx->createTexture({
            .format     = ctx->getSwapchainFormat(),
            .dimensions = fbSize,
            .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
            .debugName  = "Texture Blur 0"
        }),
        ctx->createTexture({
            .format     = ctx->getSwapchainFormat(),
            .dimensions = fbSize,
            .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
            .debugName  = "Texture Blur 1"
        })
    };

    lvk::Holder<lvk::TextureHandle> textureRot   = loadTexture( ctx, "../../data/rot_texture.bmp" );
    lvk::Holder<lvk::SamplerHandle> samplerClamp = ctx->createSampler({
        .wrapU = lvk::SamplerWrap_Clamp,
        .wrapV = lvk::SamplerWrap_Clamp,
        .wrapW = lvk::SamplerWrap_Clamp
    });

    struct SSAOpc ssaoPC {
        .textureDepth = offscreenDepth.index(),
        .textureRot   = textureRot.index(),
        .textureOut   = textureSSAO.index(),
        .sampler      = samplerClamp.index(),
        .zNear        = 0.01f,
        .zFar         = 1000.0f,
        .radius       = 0.03f,
        .attScale     = 0.95f,
        .distScale    = 1.7f
    };

    struct CombinePC combinePC {
        .textureColor = offscreenColor.index(),
        .textureSSAO  = textureSSAO.index(),
        .sampler      = samplerClamp.index(),
        .scale        = 1.5f,
        .bias         = 0.16f
    };

    lvk::Holder<lvk::ShaderModuleHandle> compBrightPass = loadShaderModule( ctx, "../shaders/BrightPass.comp" );
    lvk::Holder<lvk::ComputePipelineHandle> pipelineBrightPass = ctx->createComputePipeline( { .smComp = compBrightPass } );
    
    lvk::Holder<lvk::ShaderModuleHandle> compBloomPass = loadShaderModule( ctx, "../shaders/Bloom.comp" );
    lvk::Holder<lvk::ComputePipelineHandle> pipelineBloomX = ctx->createComputePipeline({
        .smComp   = compBloomPass,
        .specInfo = {
            .entries = {{ .constantId = 0, .size = sizeof(u32) }},
            .data     = &kHorizontal,
            .dataSize = sizeof(u32)
        }
    });
    lvk::Holder<lvk::ComputePipelineHandle> pipelineBloomY = ctx->createComputePipeline({
        .smComp   = compBloomPass,
        .specInfo = {
            .entries = {{ .constantId = 0, .size = sizeof(u32) }},
            .data     = &kVertical,
            .dataSize = sizeof(u32)
        }
    });

    lvk::Holder<lvk::ShaderModuleHandle> vertToneMap = loadShaderModule( ctx, "../../data/shaders/QuadFlip.vert" );
    lvk::Holder<lvk::ShaderModuleHandle> fragToneMap = loadShaderModule( ctx, "../shaders/ToneMap.frag" );
    lvk::Holder<lvk::RenderPipelineHandle> pipelineToneMap = ctx->createRenderPipeline({
        .smVert = vertToneMap,
        .smFrag = fragToneMap,
        .color  = { { .format = ctx->getSwapchainFormat() } },
    });
    const lvk::Dimensions sizeBloom = { 512, 512 };
    lvk::Holder<lvk::TextureHandle> texBrightPass = ctx->createTexture({
        .format     = kOffscreenFormat,
        .dimensions = sizeBloom,
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "Texture: Bright Pass"
    });
    lvk::Holder<lvk::TextureHandle> texBloomPass = ctx->createTexture({
        .format     = kOffscreenFormat,
        .dimensions = sizeBloom,
        .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
        .debugName  = "Texture: Bloom Pass"
    });

    const lvk::ComponentMapping swizzle = { .r = lvk::Swizzle_R, .g = lvk::Swizzle_R, .b = lvk::Swizzle_R, .a = lvk::Swizzle_1 };
    lvk::Holder<lvk::TextureHandle> texLumViews[10] = {
        ctx->createTexture({
            .format       = lvk::Format_R_F16,
            .dimensions   = sizeBloom,
            .usage        = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
            .numMipLevels = lvk::calcNumMipLevels( sizeBloom.width, sizeBloom.height ),
            .swizzle      = swizzle,
            .debugName    = "Texture: Luminance"
        })
    };
    for ( u32 v = 1; v != LVK_ARRAY_NUM_ELEMENTS( texLumViews ); ++v ) {
        texLumViews[v] = ctx->createTextureView( texLumViews[0], { .mipLevel = v, .swizzle = swizzle } );
    }

    lvk::Holder<lvk::TextureHandle> texBloom[] = {
        ctx->createTexture({
            .format     = kOffscreenFormat,
            .dimensions = sizeBloom,
            .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
            .debugName  = "Texture: Bloom 0"
        }),
        ctx->createTexture({
            .format     = kOffscreenFormat,
            .dimensions = sizeBloom,
            .usage      = lvk::TextureUsageBits_Sampled | lvk::TextureUsageBits_Storage,
            .debugName  = "Texture: Bloom 1"
        }),
    };
    struct ToneMapPC pcHDR = {
        .texColor     = offscreenColor.index(),
        .texLuminance = texLumViews[LVK_ARRAY_NUM_ELEMENTS(texLumViews) - 1].index(),
        .texBloom     = texBloomPass.index(),
        .sampler      = samplerClamp.index(),
        .tonemapMode  = 1
    };

    const VkMesh mesh( ctx, meshData, scene, lvk::StorageType_HostVisible );
    Pipeline shadowPipeline( ctx, meshData.streams, lvk::Format_Invalid, ctx->getFormat(shadowMap), 1,
        loadShaderModule( ctx, "../shaders/shadow.vert"),
        loadShaderModule( ctx, "../shaders/shadow.frag"), lvk::CullMode_None); // Experiment with backface culling here, it seems it makes no difference for bistro
    Pipeline *opaquePipeline = new Pipeline( ctx, meshData.streams, kOffscreenFormat, app.getDepthFormat(), app._numSamples,
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
        const mat4 view = app.camera.getViewMatrix();
        const mat4 proj = glm::perspective( 45.0f, aspectRatio, ssaoPC.zNear, ssaoPC.zFar );

        if ( app.options[mr::RendererOption::CullingCPU] ) {
            vec4 frustumPlanes[6];
            getFrustumPlanes( proj * view, frustumPlanes );
            vec4 frustumCorners[8];
            getFrustumCorners( proj * view, frustumCorners );

            s32 numVisibleMeshes = 0;
            {
                DrawIndexedIndirectCommand *cmd = mesh.getDrawIndexedIndirectCommand();
                for ( auto &p : scene.meshForNode ) {
                    const BoundingBox box  = meshData.boxes[p.second].getTransformed( scene.globalTransform[p.first] );
                    const u32 count        = isBoxInFrustum( frustumPlanes, frustumCorners, box ) ? 1 : 0;
                    (cmd++)->instanceCount = count;
                    numVisibleMeshes      += count;
                }
                // Flush changes to the GPU
                ctx->flushMappedMemory( mesh.bufferIndirect_._bufferIndirect, 0, mesh.numMeshes_ * sizeof(DrawIndexedIndirectCommand) );
            }
        }

        const mat4 rot1      = glm::rotate( mat4(1.0f), glm::radians(light.theta), glm::vec3(0, 1, 0) );
        const mat4 rot2      = glm::rotate( rot1, glm::radians(light.phi), glm::vec3(1, 0, 0) );
        const vec3 lightDir  = glm::normalize( vec3(rot2 * vec4(0.0f, -1.0f, 0.0f, 1.0f)) );
        const mat4 lightView = glm::lookAt( glm::vec3(0.0f), lightDir, vec3(0, 0, 1) );
        
        const BoundingBox boxLS = bigBoxWS.getTransformed( lightView );
        const mat4 lightProj    = glm::orthoLH_ZO( boxLS.min_.x, boxLS.max_.x, boxLS.min_.y, boxLS.max_.y, boxLS.max_.z, boxLS.min_.z );

        s32 updateMaterialIndex = -1;
        lvk::ICommandBuffer &buf = ctx->acquireCommandBuffer(); {

#pragma region Render_Shadow_Map
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
#pragma endregion

#pragma region Render_Scene
            const lvk::RenderPass renderPass = {
                .color = { { .loadOp     = lvk::LoadOp_Clear,
                             .storeOp    = app.IsMSAAEnabled() ? lvk::StoreOp_MsaaResolve : lvk::StoreOp_Store,
                             .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f }
                } },
                .depth = {   .loadOp     = lvk::LoadOp_Clear,
                             .storeOp    = app.IsMSAAEnabled() ? lvk::StoreOp_MsaaResolve : lvk::StoreOp_Store,
                             .clearDepth = 1.0f }
            };
            const lvk::Framebuffer offscreen = {
                .color = { {
                    .texture        = app.IsMSAAEnabled() ? msaaColor      : offscreenColor,
                    .resolveTexture = app.IsMSAAEnabled() ? offscreenColor : lvk::TextureHandle{}
                } },
                .depthStencil = {
                    .texture        = app.IsMSAAEnabled() ? msaaDepth      : offscreenDepth,
                    .resolveTexture = app.IsMSAAEnabled() ? offscreenDepth : lvk::TextureHandle{}
                }
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
                    const DrawIndexedIndirectCommand *cmd = mesh.getDrawIndexedIndirectCommand();
                    for ( auto &p : scene.meshForNode ) {
                        if ( (cmd++)->instanceCount == 0 && app.options[mr::RendererOption::CullingCPU] )
                            continue;
                        const BoundingBox box = meshData.boxes[p.second];
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
#pragma endregion

#pragma region Compute_SSAO
            if ( app.options[mr::RendererOption::SSAO] ) {
                buf.cmdPushDebugGroupLabel( "Compute SSAO", 0xFF805020 );
                    buf.cmdBindComputePipeline( pipelineSSAO );
                    buf.cmdPushConstants( ssaoPC );
                    buf.cmdDispatchThreadGroups({
                        .width  = 1 + (u32)fbSize.width / 16,
                        .height = 1 + (u32)fbSize.height / 16
                    }, {
                        .textures = { lvk::TextureHandle( offscreenDepth ), lvk::TextureHandle( textureSSAO ) }
                    });
                buf.cmdPopDebugGroupLabel();
            }
#pragma endregion

#pragma region Blur_SSAO
            if ( app.options[mr::RendererOption::BlurSSAO] ) {
                buf.cmdPushDebugGroupLabel( "Blur SSAO", 0xFF205080 );
                    const lvk::Dimensions blurDim = {
                        .width  = 1 + (u32)fbSize.width / 16,
                        .height = 1 + (u32)fbSize.height / 16 
                    };
                    struct BlurPC {
                        u32 textureDepth;
                        u32 textureIn;
                        u32 textureOut;
                        f32 depthThreshold;
                    };
                    blurPassesSSAO.clear();
                    blurPassesSSAO.push_back( { textureSSAO, textureBlur[0] } );
                    for ( s32 i = 0; i != numBlurPassesSSAO - 1; ++i ) {
                        blurPassesSSAO.push_back( { textureBlur[0], textureBlur[1] } );
                        blurPassesSSAO.push_back( { textureBlur[1], textureBlur[0] } );
                    }
                    blurPassesSSAO.push_back( { textureBlur[0], textureSSAO } );
                    for ( u32 i = 0; i != blurPassesSSAO.size(); ++i ) {
                        const BlurPass p = blurPassesSSAO[i];
                        buf.cmdBindComputePipeline( i & 1 ? pipelineBlurX : pipelineBlurY );
                        buf.cmdPushConstants( BlurPC {
                            .textureDepth   = offscreenDepth.index(),
                            .textureIn      = p.textureIn.index(),
                            .textureOut     = p.textureOut.index(),
                            .depthThreshold = ssaoPC.zFar * app.ssaoDepthThreshold 
                        });
                        buf.cmdDispatchThreadGroups( blurDim, { .textures = { p.textureIn, p.textureOut, lvk::TextureHandle(offscreenDepth) } } );
                    }
                buf.cmdPopDebugGroupLabel();
            }
#pragma endregion

#pragma region Render_Scene_With_SSAO
            buf.cmdPushDebugGroupLabel( "Combine Pass", 0xFF204060 );
                if ( app.options[mr::RendererOption::SSAO] ) {
                    buf.cmdCopyImage( textureSSAO, ctx->getCurrentSwapchainTexture(), offscreenSize );
                } else {
                    buf.cmdCopyImage( offscreenColor, ctx->getCurrentSwapchainTexture(), offscreenSize );
                }

            buf.cmdBeginRendering(
                { .color = {{ .loadOp = lvk::LoadOp_Load, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } }} },
                { .color = { { .texture = offscreenColor } } },
                { .textures = { app.options[mr::RendererOption::SSAO] ? lvk::TextureHandle(textureSSAO) : lvk::TextureHandle() } } );
                    if ( app.options[mr::RendererOption::SSAO] ) {
                        buf.cmdBindRenderPipeline( pipelineCombine );
                        buf.cmdPushConstants( combinePC );
                        buf.cmdBindDepthState({});
                        buf.cmdDraw(3);
                        buf.cmdEndRendering();
                    }
            buf.cmdPopDebugGroupLabel();
#pragma endregion

#pragma region Bright_Pass
            buf.cmdPushDebugGroupLabel( "Bright Pass", 0xFF803050 );
                BrightPassPC pcBrightPass = {
                    .texColor     = offscreenColor.index(),
                    .texOut       = texBrightPass.index(),
                    .texLuminance = texLumViews[0].index(),
                    .sampler      = samplerClamp.index(),
                    .exposure     = pcHDR.exposure
                };
                buf.cmdBindComputePipeline( pipelineBrightPass );
                buf.cmdPushConstants( pcBrightPass );
                buf.cmdDispatchThreadGroups( sizeBloom.divide2D(16), { .textures = {
                    lvk::TextureHandle( offscreenColor ),
                    lvk::TextureHandle( texLumViews[0] )
                } });
                buf.cmdGenerateMipmap( texLumViews[0] );
            buf.cmdPopDebugGroupLabel();
#pragma endregion

#pragma region Bloom_Pass
            if ( app.options[mr::RendererOption::Bloom] ) {
                buf.cmdPushDebugGroupLabel( "Bloom Pass", 0xFF503080 );
                    struct BloomPC {
                        u32 texIn;
                        u32 texOut;
                        u32 sampler;
                    };
                    struct StreaksPC {
                        u32 texIn;
                        u32 texOut;
                        u32 texRotationPattern;
                        u32 sampler;
                    };
                    blurPassesBloom.clear();
                    blurPassesBloom.push_back( { texBrightPass, texBloom[0] } );
                    for ( s32 i = 0; i != numBlurPassesBloom - 1; ++i ) {
                        blurPassesBloom.push_back( { texBloom[0], texBloom[1] } );
                        blurPassesBloom.push_back( { texBloom[1], texBloom[0] } );
                    }
                    blurPassesBloom.push_back( { texBloom[0], texBloomPass } );
                    for ( u32 i = 0; i != blurPassesBloom.size(); ++i ) {
                        const BlurPass p = blurPassesBloom[i];
                        buf.cmdBindComputePipeline( i & 1 ? pipelineBloomX : pipelineBloomY );
                        buf.cmdPushConstants( BloomPC {
                            .texIn   = p.textureIn.index(),
                            .texOut  = p.textureOut.index(),
                            .sampler = samplerClamp.index()
                        });
                        buf.cmdDispatchThreadGroups( sizeBloom.divide2D(16), {
                            .textures = { p.textureIn, p.textureOut, lvk::TextureHandle(texBrightPass)
                        }});
                    }
                buf.cmdPopDebugGroupLabel();
            } else {
                pcHDR.bloomStrength = 0.0f; // Instead of clearing the bloom texture, zero out its impact on the final image
            }
#pragma endregion

#pragma region ToneMapping
            const lvk::RenderPass renderPassMain = {
                .color = { { .loadOp = lvk::LoadOp_Load, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } }
            };
            const lvk::Framebuffer framebufferMain = {
                .color = { { .texture = ctx->getCurrentSwapchainTexture() } }
            };

            buf.cmdPushDebugGroupLabel( "ToneMapping", 0xFF701080 );
                buf.cmdBeginRendering( renderPassMain, framebufferMain, {
                    .textures = { lvk::TextureHandle(texLumViews[0]) }
                });
                buf.cmdBindRenderPipeline( pipelineToneMap );
                buf.cmdPushConstants( pcHDR );
                buf.cmdBindDepthState( {} );
                buf.cmdDraw( 3 );
            buf.cmdPopDebugGroupLabel();
#pragma endregion

#pragma region Render_UI
            app.imgui->beginFrame( framebufferMain );
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
                        .format     = kOffscreenFormat,
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
                    opaquePipeline = new Pipeline( ctx, meshData.streams, kOffscreenFormat, app.getDepthFormat(), app._numSamples,
                        loadShaderModule( ctx, "../shaders/main.vert" ),
                        loadShaderModule( ctx, "../shaders/main.frag" ), lvk::CullMode_Back );

                    prevNumSamples = app._numSamples;
                }
                s32 selectedToneMap = std::find( &app.options[mr::RendererOption::ToneMappingNone], &app.options[mr::RendererOption::ToneMappingKhronosPBR], true ) - app.options;
                pcHDR.tonemapMode = selectedToneMap - mr::RendererOption::ToneMappingNone;

                const ImVec2 lightControlsSize = mr::ImGuiLightControlsComponent( light, shadowMap.index(), { 10.0f, renderOptionsSize.y + mr::COMPONENT_PADDING } );
                const ImVec2 ssaoControlsSize  = mr::ImGuiSSAOControlsComponent( ssaoPC, combinePC, numBlurPassesSSAO, app.ssaoDepthThreshold,
                    textureSSAO.index(), { 10.0f, lightControlsSize.y + mr::COMPONENT_PADDING } );
                const ImVec2 bloomControlsSize = mr::ImGuiBloomToneMapControlsComponent( pcHDR, pcBrightPass, numBlurPassesBloom, { 10.0f, ssaoControlsSize.y + mr::COMPONENT_PADDING } );
                const ImVec2 sceneGraphSize    = mr::ImGuiSceneGraphComponent( scene, selectedNode, { 10.0f, bloomControlsSize.y + mr::COMPONENT_PADDING } );
                mr::ImGuiEditNodeComponent( scene, meshData, view, proj, selectedNode, updateMaterialIndex, mesh.textureCache_ );
            app.imgui->endFrame( buf );
            buf.cmdEndRendering();
#pragma endregion
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
