#include "../include/App.hpp"

extern std::unordered_map<u32, std::string> debugGLSLSourceCode;
static void shaderModuleCallback( lvk::IContext *_, lvk::ShaderModuleHandle handle, s32 line, s32 col, const char *debugName ) {
	const auto it = debugGLSLSourceCode.find( handle.index() );
	if ( it != debugGLSLSourceCode.end() ) {
		lvk::logShaderSource( it->second.c_str() );
	}
}

mr::App::App( std::string skyboxTexFilename, std::string skyboxIrrFilename ) {
	minilog::initialize( nullptr, { .threadNames = false } );

	s32 width = -95, height = -90;

	window = lvk::initWindow( "MediumRare", width, height );
	ctx    = lvk::createVulkanContextWithSwapchain(
		window, width, height, 
		{
			.enableValidation = true,
			.shaderModuleErrorCallback = &shaderModuleCallback
		});
	depthTexture = ctx->createTexture({
		.type       = lvk::TextureType_2D,
		.format     = lvk::Format_Z_F32,
		.dimensions = { u32(width), u32(height) },
		.usage      = lvk::TextureUsageBits_Attachment,
		.debugName  = "Depth Buffer"
	});
	
	imgui = std::make_unique<lvk::ImGuiRenderer>( *ctx, "../../data/OpenSans-Light.ttf", 20.0f );

	glfwSetWindowUserPointer( window, this );
	glfwSetMouseButtonCallback( window, []( GLFWwindow *window, s32 button, s32 action, s32 mods ) {
		App *app = (App*)glfwGetWindowUserPointer( window );
		if ( button == GLFW_MOUSE_BUTTON_LEFT ) {
			app->mouseState.pressedLeft = action == GLFW_PRESS;
		}

		f64 xpos, ypos;
		glfwGetCursorPos( window, &xpos, &ypos );
		const ImGuiMouseButton_ imguiButton = ( button == GLFW_MOUSE_BUTTON_LEFT )
			                                ? ImGuiMouseButton_Left
											: ( button == GLFW_MOUSE_BUTTON_RIGHT )
											? ImGuiMouseButton_Right
											: ImGuiMouseButton_Middle;
		ImGuiIO &io               = ImGui::GetIO();
		io.MousePos               = ImVec2( f32(xpos), f32(ypos) );
		io.MouseDown[imguiButton] = action == GLFW_PRESS;
		for ( auto &callback : app->mouseButtonCallbacks ) {
			callback( window, button, action, mods );
		}
	});
	glfwSetScrollCallback( window, []( GLFWwindow *window, f64 dx, f64 dy ) {
		ImGuiIO &io    = ImGui::GetIO();
		io.MouseWheelH = f32(dx);
		io.MouseWheel  = f32(dy);
	});
	glfwSetCursorPosCallback( window, []( GLFWwindow *window, f64 x, f64 y) {
		App *app = (App*)glfwGetWindowUserPointer( window );
		s32 width, height;
		glfwGetFramebufferSize( window, &width, &height );
		ImGui::GetIO().MousePos = ImVec2( x, y );
		app->mouseState.pos.x = static_cast<f32>( x / width );
		app->mouseState.pos.y = 1.0f - static_cast<f32>( y / height );
	});
	glfwSetKeyCallback( window, []( GLFWwindow *window, s32 key, s32 scanCode, s32 action, s32 mods ) {
		App *app = (App*)glfwGetWindowUserPointer( window );
		const bool pressed = action != GLFW_RELEASE;
		if ( key == GLFW_KEY_ESCAPE && pressed ) {
			glfwSetWindowShouldClose( window, GLFW_TRUE );
		}
		if ( key == GLFW_KEY_W ) app->fpsPositioner.movement_.forward_  = pressed;
		if ( key == GLFW_KEY_S ) app->fpsPositioner.movement_.backward_ = pressed;
		if ( key == GLFW_KEY_A ) app->fpsPositioner.movement_.left_     = pressed;
		if ( key == GLFW_KEY_D ) app->fpsPositioner.movement_.right_    = pressed;
		if ( key == GLFW_KEY_1 ) app->fpsPositioner.movement_.up_       = pressed;
		if ( key == GLFW_KEY_2 ) app->fpsPositioner.movement_.down_     = pressed;

		app->fpsPositioner.movement_.fastSpeed_ = ( mods & GLFW_MOD_SHIFT ) != 0;

		if ( key == GLFW_KEY_SPACE ) {
			app->fpsPositioner.lookAt( kInitialCameraPos, kInitialCameraTarget, vec3( 0.0f, 1.0f, 0.0f ) );
			app->fpsPositioner.setSpeed( vec3(0.0f) );
		}
		for ( auto &callback : app->keyCallbacks ) {
			callback( window, key, scanCode, action, mods );
		}
	});

	options[RendererOption::Grid]      = false;
	options[RendererOption::Wireframe] = false; // TODO: Implement wireframe rendering
	options[RendererOption::Skybox]    = true;

	// Initialize Grid
	gridVert     = loadShaderModule( ctx, "../shaders/grid.vert" );
	gridFrag     = loadShaderModule( ctx, "../shaders/grid.frag" );
	gridPipeline = ctx->createRenderPipeline({
		.smVert = gridVert,
		.smFrag = gridFrag,
		.color  = { {
			.format            = ctx->getSwapchainFormat(),
			.blendEnabled      = true,
			.srcRGBBlendFactor = lvk::BlendFactor_SrcAlpha,
			.dstRGBBlendFactor = lvk::BlendFactor_OneMinusSrcAlpha
		} },
		.depthFormat = getDepthFormat()
	});
	LVK_ASSERT( gridPipeline.valid() );

	// Initialize Skybox
	skyboxTexture    = loadTexture( ctx, skyboxTexFilename.c_str(), lvk::TextureType_Cube );
	skyboxIrradiance = loadTexture( ctx, skyboxIrrFilename.c_str(), lvk::TextureType_Cube );
	skyboxVert       = loadShaderModule( ctx, "../shaders/skybox.vert" );
	skyboxFrag       = loadShaderModule( ctx, "../shaders/skybox.frag" );
	skyboxPipeline   = ctx->createRenderPipeline({
		.smVert       = skyboxVert,
		.smFrag       = skyboxFrag,
		.color        = { { .format = ctx->getSwapchainFormat() } },
		.depthFormat  = getDepthFormat(),
		.samplesCount = 1
	});
	LVK_ASSERT( skyboxPipeline.valid() );
}

mr::App::~App() {
	skyboxPipeline   = nullptr;
	skyboxVert       = nullptr;
	skyboxFrag       = nullptr;
	skyboxTexture    = nullptr;
	skyboxIrradiance = nullptr;

	gridPipeline = nullptr;
	gridVert     = nullptr;
	gridFrag     = nullptr;

	imgui        = nullptr;
	depthTexture = nullptr;
	ctx          = nullptr;

	glfwDestroyWindow( window );
	glfwTerminate();
}

void mr::App::run( std::function<void( u32 width, u32 height, f32 aspectRatio, f32 deltaSeconds )> drawFunc ) {
	f64 timeStamp    = glfwGetTime();
	f32 deltaSeconds = 0.0f;

	s32 width, height;
	while ( !glfwWindowShouldClose( window ) ) {
		fpsCounter.tick( deltaSeconds );
		const f64 newTimeStamp = glfwGetTime();
		deltaSeconds           = static_cast<f32>( newTimeStamp - timeStamp );
		timeStamp              = newTimeStamp;

		glfwPollEvents();
		glfwGetFramebufferSize( window, &width, &height );
		if ( !width || !height )
			continue;

		const f32 ratio = width / f32(height);

		fpsPositioner.update( deltaSeconds, mouseState.pos, ImGui::GetIO().WantCaptureMouse ? false : mouseState.pressedLeft );
		moveToPositioner.update( deltaSeconds, mouseState.pos, mouseState.pressedLeft );
		drawFunc( u32(width), u32(height), ratio, deltaSeconds );
	}
}

void mr::App::drawGrid( lvk::ICommandBuffer &buf, const mat4 &proj ) {
	if ( !options[RendererOption::Grid] )
		return;

	const struct {
        mat4 mvp;
        vec4 cameraPos;
        vec4 origin;
    } gridPC {
        .mvp       = proj * camera.getViewMatrix(),
        .cameraPos = vec4( camera.getPosition(), 1.0f ),
        .origin    = vec4( 0.0f )
    };

    buf.cmdPushDebugGroupLabel( "Grid", 0xFFFF00FF );
        buf.cmdBindRenderPipeline( gridPipeline );
        buf.cmdBindDepthState( {} );
        buf.cmdPushConstants( gridPC );
        buf.cmdDraw( 6 );
    buf.cmdPopDebugGroupLabel();
}

void mr::App::drawSkybox( lvk::ICommandBuffer &buf, const mat4 &view, const mat4 &proj ) const {
	if ( !options[RendererOption::Skybox] )
		return;

	const struct {
		mat4 mvp;
		u32  skyboxTextureId;
	} skyboxPC {
		.mvp             = proj * mat4( mat3(view) ),
		.skyboxTextureId = skyboxTexture.index()
	};
	
	buf.cmdPushDebugGroupLabel( "Skybox", 0xFF0000FF );
		buf.cmdBindRenderPipeline( skyboxPipeline );
		buf.cmdPushConstants( skyboxPC );
		buf.cmdBindDepthState( { .isDepthWriteEnabled = false } );
		buf.cmdDraw( 36 );
	buf.cmdPopDebugGroupLabel();
}
