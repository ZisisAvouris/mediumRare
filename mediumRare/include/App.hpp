#pragma once

#include "types.hpp"
#include "RendererOptions.hpp"
#include "Pipeline.hpp"

#include <lvk/HelpersImGui.h>
#include <lvk/LVK.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/ext.hpp>
using glm::mat3;
using glm::mat4;
using glm::vec2;
using glm::vec3;
using glm::vec4;

#include <shared/UtilsFPS.h>
#include <shared/Camera.h>
#include <shared/Utils.h>

#include <functional>
#include <vector>

namespace mr {
	const vec3 kInitialCameraPos    = vec3( 0.0f, 1.0f, -1.5f );
	const vec3 kInitialCameraTarget = vec3( 0.0f, 0.5f,  0.0f );
	const vec3 kInitialCameraAngles = vec3( -18.5f, 180.0f, 0.0f );

	class App final {
	public:
		explicit App( std::string skyboxTexFilename = "../../data/immenstadter_horn_2k_prefilter.ktx", std::string skyboxIrrFilename = "../../data/immenstadter_horn_2k_irradiance.ktx" );
		virtual ~App();

		virtual void run( std::function<void( u32 width, u32 height, f32 aspectRatio, f32 deltaSeconds )> );

		lvk::Format getDepthFormat( void ) const         { return ctx->getFormat( depthTexture ); }
		lvk::TextureHandle getDepthTexture( void ) const { return depthTexture; }

		void addMouseButtonCallback( GLFWmousebuttonfun cb ) { mouseButtonCallbacks.push_back(cb); }
		void addKeyCallback( GLFWkeyfun cb )                 { keyCallbacks.push_back(cb); }

		void drawGrid( lvk::ICommandBuffer &buf, const mat4 &proj );
		void drawSkybox( lvk::ICommandBuffer &buf, const mat4 &view, const mat4 &proj );

	public:
		GLFWwindow                          *window = nullptr;
		std::unique_ptr<lvk::IContext>      ctx;
		lvk::Holder<lvk::TextureHandle>     depthTexture;
		std::unique_ptr<lvk::ImGuiRenderer> imgui;

		FramesPerSecondCounter fpsCounter = FramesPerSecondCounter( 0.5f );

		struct MouseState {
			vec2 pos         = vec2( 0.0f );
			bool pressedLeft = false;
		} mouseState;

		CameraPositioner_FirstPerson fpsPositioner = { kInitialCameraPos, kInitialCameraTarget, vec3( 0.0f, 1.0f, 0.0f ) };
		CameraPositioner_MoveTo moveToPositioner   = { kInitialCameraPos, kInitialCameraAngles };
		Camera camera                           = Camera( fpsPositioner );
		vec3 cameraPos    = kInitialCameraPos;
		vec3 cameraAngles = kInitialCameraAngles;
		bool cameraType   = false; // TODO: change this to an enum

		bool options[RendererOption::MAX];

		u32 _numSamples = 1;

	protected:
		std::vector<GLFWmousebuttonfun> mouseButtonCallbacks;
		std::vector<GLFWkeyfun>         keyCallbacks;

	public:
		// Grid
		Pipeline							   *gridPipeline;

		// Skybox
		lvk::Holder<lvk::TextureHandle>        skyboxTexture;
		lvk::Holder<lvk::TextureHandle>        skyboxIrradiance;
		lvk::Holder<lvk::ShaderModuleHandle>   skyboxVert;
		lvk::Holder<lvk::ShaderModuleHandle>   skyboxFrag;
		lvk::Holder<lvk::RenderPipelineHandle> skyboxPipeline;

	};
}
