#include <GLFW/glfw3.h>
#include <lvk/LVK.h>
#include <minilog/minilog.h>

#include <shared/Utils.h>
#include <shared/UtilsFPS.h>
#include <shared/Bitmap.h>
#include <shared/UtilsCubemap.h>
#include <shared/Camera.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>

#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/cimport.h>
#include <assimp/version.h>

#include <implot/implot.h>

#include <vector>

#include <stb/stb_image.h>
#include <stb/stb_image_write.h>

#include "../include/ImGuiComponents.hpp"

using glm::mat4;
using glm::vec4;
using glm::vec3;
using glm::vec2;

struct MouseState {
	vec2 pos = vec2( 0.0f );
	bool pressedLeft = false;
} mouseState;
const vec3 kInitialCameraPos    = vec3( 0.0f, 1.0f, -1.5f );
const vec3 kInitialCameraTarget = vec3( 0.0f, 0.5f,  0.0f );
const vec3 kInitialCameraAngles = vec3( -18.5f, 180.0f, 0.0f );

CameraPositioner_FirstPerson fpsPositioner( kInitialCameraPos, kInitialCameraTarget, vec3( 0.0f, 1.0f, 0.0f ) );
CameraPositioner_MoveTo moveToPositioner( kInitialCameraPos, kInitialCameraAngles );
Camera camera( fpsPositioner );
vec3 cameraPos = kInitialCameraPos;
vec3 cameraAngles = kInitialCameraAngles;
bool cameraType = false; // TODO: change this to an enum

int main( void ) {
	minilog::initialize( nullptr, { .threadNames = false } );

	GLFWwindow *window = nullptr;
	std::unique_ptr<lvk::IContext> ctx;
	lvk::Holder<lvk::TextureHandle> depthTexture;
	{
		LVK_PROFILER_ZONE( "Initialization", LVK_PROFILER_COLOR_CREATE );
		int width = -95, height = -90;

		window = lvk::initWindow( "MediumRare", width, height );
		ctx    = lvk::createVulkanContextWithSwapchain( window, width, height, {} );

		depthTexture = ctx->createTexture({
			.type = lvk::TextureType_2D,
			.format = lvk::Format_Z_F32,
			.dimensions = { uint32_t(width), uint32_t(height) },
			.usage = lvk::TextureUsageBits_Attachment,
			.debugName = "Depth Buffer"
		});
		LVK_PROFILER_ZONE_END();
	}

	std::unique_ptr<lvk::ImGuiRenderer> imgui = std::make_unique<lvk::ImGuiRenderer>(*ctx, "../../../data/OpenSans-Light.ttf", 30.0f);

	glfwSetCursorPosCallback(window, [](auto* window, double x, double y) {
		ImGui::GetIO().MousePos = ImVec2(x, y);

		int width, height;
		glfwGetFramebufferSize( window, &width, &height );
		mouseState.pos.x = static_cast<float>( x / width );
		mouseState.pos.y = 1.0f - static_cast<float>( y / height );
	});
	glfwSetMouseButtonCallback(window, [](auto* window, int button, int action, int mods) {
		if ( button == GLFW_MOUSE_BUTTON_LEFT ) {
			mouseState.pressedLeft = action == GLFW_PRESS;
		}

		double xpos, ypos;
		glfwGetCursorPos(window, &xpos, &ypos);
		const ImGuiMouseButton_ imguiButton = (button == GLFW_MOUSE_BUTTON_LEFT)
			? ImGuiMouseButton_Left  : (button == GLFW_MOUSE_BUTTON_RIGHT
			? ImGuiMouseButton_Right : ImGuiMouseButton_Middle);
		ImGuiIO& io               = ImGui::GetIO();
		io.MousePos               = ImVec2((float)xpos, (float)ypos);
		io.MouseDown[imguiButton] = action == GLFW_PRESS;
	});
	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
      const bool pressed = action != GLFW_RELEASE;
      if (key == GLFW_KEY_ESCAPE && pressed)
        glfwSetWindowShouldClose(window, GLFW_TRUE);
      if (key == GLFW_KEY_W)
        fpsPositioner.movement_.forward_ = pressed;
      if (key == GLFW_KEY_S)
        fpsPositioner.movement_.backward_ = pressed;
      if (key == GLFW_KEY_A)
        fpsPositioner.movement_.left_ = pressed;
      if (key == GLFW_KEY_D)
        fpsPositioner.movement_.right_ = pressed;
      if (key == GLFW_KEY_1)
        fpsPositioner.movement_.up_ = pressed;
      if (key == GLFW_KEY_2)
        fpsPositioner.movement_.down_ = pressed;
      if (mods & GLFW_MOD_SHIFT)
        fpsPositioner.movement_.fastSpeed_ = pressed;
      if (key == GLFW_KEY_SPACE) {
        fpsPositioner.lookAt(kInitialCameraPos, kInitialCameraTarget, vec3(0.0f, 1.0f, 0.0f));
        fpsPositioner.setSpeed(vec3(0));
      }
    });

	struct VertexData {
		vec3 pos;
		vec3 n;
		vec2 tc;
	};

	lvk::Holder<lvk::ShaderModuleHandle> vert       = loadShaderModule(ctx, "../shaders/main.vert");
	lvk::Holder<lvk::ShaderModuleHandle> frag       = loadShaderModule(ctx, "../shaders/main.frag");
	lvk::Holder<lvk::ShaderModuleHandle> skyboxVert = loadShaderModule( ctx, "../shaders/skybox.vert" );
	lvk::Holder<lvk::ShaderModuleHandle> skyboxFrag = loadShaderModule( ctx, "../shaders/skybox.frag" );

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
		.depthFormat = ctx->getFormat( depthTexture ),
		.cullMode    = lvk::CullMode_Back,
	});
	LVK_ASSERT( pipeline.valid() );

	lvk::Holder<lvk::RenderPipelineHandle> pipelineSkybox = ctx->createRenderPipeline({
		.smVert      = skyboxVert,
		.smFrag      = skyboxFrag,
		.color       = { { .format = ctx->getSwapchainFormat() } },
		.depthFormat = ctx->getFormat( depthTexture )
	});
	LVK_ASSERT( pipelineSkybox.valid() );

	const aiScene *scene = aiImportFile( "../../../data/rubber_duck/scene.gltf", aiProcess_Triangulate );
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
	lvk::Holder<lvk::TextureHandle> texture = loadTexture( ctx, "../../../data/rubber_duck/textures/Duck_baseColor.png" );

	lvk::Holder<lvk::TextureHandle> cubemapTex; {
		int w, h;
		const float *img = stbi_loadf( "../../../data/piazza_bologni_1k.hdr", &w, &h, nullptr, 4 );

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

	double timeStamp   = glfwGetTime();
	float deltaSeconds = 0.0f;

	FramesPerSecondCounter fpsCounter( 0.5f );
	fpsCounter.printFPS_ = false;
	ImPlotContext *implotCtx = ImPlot::CreateContext();

	while (!glfwWindowShouldClose(window)) {
		fpsCounter.tick( deltaSeconds );
		const double newTimeStamp = glfwGetTime();
		deltaSeconds              = static_cast<float>( newTimeStamp - timeStamp );
		timeStamp                 = newTimeStamp;

		glfwPollEvents();
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		if (!width || !height)
			continue;
		const float ratio = width / (float)height;

		fpsPositioner.update( deltaSeconds, mouseState.pos, mouseState.pressedLeft );
		moveToPositioner.update( deltaSeconds, mouseState.pos, ImGui::GetIO().WantCaptureMouse ? false : mouseState.pressedLeft );

      const mat4 p  = glm::perspective(glm::radians(60.0f), ratio, 0.1f, 1000.0f);
      const mat4 m1 = glm::rotate(mat4(1.0f), glm::radians(-90.0f), vec3(1, 0, 0));
      const mat4 m2 = glm::rotate(mat4(1.0f), (float)glfwGetTime(), vec3(0.0f, 1.0f, 0.0f));
      const mat4 v  = glm::translate(mat4(1.0f), camera.getPosition());

		const PerFrameData pc = {
        .model     = m2 * m1,
        .view      = camera.getViewMatrix(),
        .proj      = p,
        .cameraPos = vec4( camera.getPosition(), 1.0f ),
        .tex       = texture.index(),
        .texCube   = cubemapTex.index(),
      };
		ctx->upload( bufferPerFrame, &pc, sizeof(pc) );

		const lvk::RenderPass renderPass = {
        .color = { { .loadOp = lvk::LoadOp_Clear, .clearColor = { 1.0f, 1.0f, 1.0f, 1.0f } } },
        .depth = { .loadOp = lvk::LoadOp_Clear, .clearDepth = 1.0f }
      };

      const lvk::Framebuffer framebuffer = {
        .color        = { { .texture = ctx->getCurrentSwapchainTexture() } },
        .depthStencil = { .texture = depthTexture },
      };

		lvk::ICommandBuffer& buf = ctx->acquireCommandBuffer();
		{
			buf.cmdBeginRendering( renderPass, framebuffer );
			{
				buf.cmdPushDebugGroupLabel( "Skybox", 0xFF0000FF );
				buf.cmdBindRenderPipeline( pipelineSkybox );
				buf.cmdPushConstants( ctx->gpuAddress(bufferPerFrame) );
				buf.cmdDraw( 36 );
				buf.cmdPopDebugGroupLabel();
			}
			{
				buf.cmdPushDebugGroupLabel( "Duck", 0xFF0000FF );
				buf.cmdBindVertexBuffer( 0, vertexBuffer );
				buf.cmdBindRenderPipeline( pipeline );
				buf.cmdBindDepthState( { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true } );
				buf.cmdBindIndexBuffer( indexBuffer, lvk::IndexFormat_UI32 );
				buf.cmdDrawIndexed( indices.size() );
				buf.cmdPopDebugGroupLabel();
			}

			imgui->beginFrame(framebuffer);
				const ImVec2 statsSize = mr::ImGuiFPSComponent( fpsCounter.getFPS() );
				mr::ImGuiCameraControlsComponent( cameraPos, cameraAngles, cameraType, { 10.0f, statsSize.y + mr::COMPONENT_PADDING } );
				if ( cameraType == false ) {
					camera = Camera( fpsPositioner );
				} else {
					moveToPositioner.setDesiredPosition( cameraPos );
					moveToPositioner.setDesiredAngles( cameraAngles );
					camera = Camera( moveToPositioner );
				}
			imgui->endFrame(buf);
			buf.cmdEndRendering();
		}
		ctx->submit(buf, ctx->getCurrentSwapchainTexture());
	}

	glfwDestroyWindow( window );
	glfwTerminate();

	return 0;
}
