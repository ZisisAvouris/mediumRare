#include <lvk/HelpersImGui.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "RendererOptions.hpp"
#include "types.hpp"
#include <span>

#include <shared/Scene/Scene.h>

namespace mr {
	static constexpr float COMPONENT_PADDING = 20.0f;

	ImVec2 ImGuiFPSComponent( const float fps, const ImVec2 pos = { 10, 10 } );
	ImVec2 ImGuiCameraControlsComponent( glm::vec3 &cameraPos, glm::vec3 &cameraAngles, bool &changedCameraType, const ImVec2 pos = { 10, 10 } );

	ImVec2 ImGuiRenderOptionsComponent( std::span<bool> options, const ImVec2 pos = { 10, 10 } );

	s32 __renderSceneTreeUI( const Scene &scene, s32 node, s32 selectedNode );
	ImVec2 ImGuiSceneGraphComponent( const Scene &scene, s32 &selectedNode, const ImVec2 pos = { 10, 10 } );
}