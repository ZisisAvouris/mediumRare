#include <lvk/HelpersImGui.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "RendererOptions.hpp"
#include "types.hpp"
#include <span>

#include <shared/Scene/Scene.h>
#include <shared/Scene/VtxData.h>
#include <deps/src/ImGuizmo/ImGuizmo.h>

using TextureCache = std::vector<lvk::Holder<lvk::TextureHandle>>;

namespace mr {
	static constexpr float COMPONENT_PADDING = 20.0f;

	f32 __computeMaxItemWidth( const char **items, size_t itemsLength );

	ImVec2 ImGuiFPSComponent( const float fps, const ImVec2 pos = { 10, 10 } );
	ImVec2 ImGuiCameraControlsComponent( glm::vec3 &cameraPos, glm::vec3 &cameraAngles, bool &changedCameraType, const ImVec2 pos = { 10, 10 } );

	ImVec2 ImGuiRenderOptionsComponent( std::span<bool> options, const ImVec2 pos = { 10, 10 } );

	s32 __renderSceneTreeUI( const Scene &scene, s32 node, s32 selectedNode );
	ImVec2 ImGuiSceneGraphComponent( const Scene &scene, s32 &selectedNode, const ImVec2 pos = { 10, 10 } );

	bool __editTransformUI( const mat4 &view, const mat4 &proj, mat4 &matrix );
	bool __editMaterialUI( Scene &scene, MeshData &meshData, s32 node, s32 &outUpdateMaterialIndex, const TextureCache &textureCache );
	ImVec2 ImGuiEditNodeComponent( Scene &scene, MeshData &meshData, const mat4 &view, const mat4 &proj, s32 node, s32 &outUpdateMaterialIndex, const TextureCache &textureCache );

	ImVec2 ImGuiLightControlsComponent( LightParams &lightParams, u32 shadowMapIndex, const ImVec2 pos = { 10, 10 } );

	ImVec2 ImGuiSSAOControlsComponent( SSAOpc &pc, CombinePC &comb, s32 &blurPasses, f32 &depthThreshold, u32 ssaoTextureIndex, const ImVec2 pos = { 10, 10 } );
}