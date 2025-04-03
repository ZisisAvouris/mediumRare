#include "../include/ImGuiComponents.hpp"

ImVec2 mr::ImGuiFPSComponent( const float fps, const ImVec2 pos ) {
	ImGui::SetNextWindowPos( pos );
	ImGui::Begin( "Stats:", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
		ImGui::Text("FPS: %i", int(fps) );
		ImGui::Text("Frametime: %.2f ms", 1000.0f / fps );
		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}

ImVec2 mr::ImGuiCameraControlsComponent( glm::vec3 &cameraPos, glm::vec3 &cameraAngles, bool &changedCameraType, const ImVec2 pos ) {
	static const char *cameraType = "FirstPerson";
	static const char *comboBoxItems[] = { "FirstPerson", "MoveTo" };
	static const char *currentComboBoxItem = cameraType;

	ImGui::SetNextWindowPos( pos );
	ImGui::Begin( "Camera Controls:", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
		if ( ImGui::BeginCombo( "##combo", currentComboBoxItem ) ) {
			for ( int n = 0; n < IM_ARRAYSIZE( comboBoxItems ); ++n ) {
				const bool isSelected = currentComboBoxItem == comboBoxItems[n];

				if ( ImGui::Selectable( comboBoxItems[n], isSelected ) ) {
					currentComboBoxItem = comboBoxItems[n];
				}
				if ( isSelected ) {
						ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		if ( !strcmp( cameraType, "MoveTo" ) ) {
			ImGui::SliderFloat3( "Position", glm::value_ptr( cameraPos ), -10.0f, 10.0f );
			ImGui::SliderFloat3( "Pitch/Pan/Roll", glm::value_ptr( cameraAngles ), -180.0f, 180.0f );
		}

		if ( currentComboBoxItem && strcmp( currentComboBoxItem, cameraType ) ) {
			printf( "[INFO] Selected new camera type: %s\n", currentComboBoxItem );
			cameraType = currentComboBoxItem;
			changedCameraType = !changedCameraType;
		}
		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}

ImVec2 mr::ImGuiRenderOptionsComponent( std::span<bool> options, const ImVec2 pos ) {
	ImGui::SetNextWindowPos( pos );
	ImGui::Begin( "Render Options:", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
	
		ImGui::Checkbox( "Draw Grid ",          &options[RendererOption::Grid] );
		ImGui::Checkbox( "Draw Wireframe",      &options[RendererOption::Wireframe] );
		ImGui::Checkbox( "Draw Skybox",         &options[RendererOption::Skybox] );
		ImGui::Checkbox( "Draw Bounding Boxes", &options[RendererOption::BoundingBox] );

		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}

s32 mr::__renderSceneTreeUI( const Scene &scene, s32 node, s32 selectedNode ) {
	const std::string name  = getNodeName( scene, node );
	const std::string label = name.empty() ? std::string("Node") + std::to_string(node) : name;

	const bool isLeaf = scene.hierarchy[node].firstChild < 0;
	ImGuiTreeNodeFlags flags = isLeaf ? ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet : 0;
	if ( node == selectedNode )
		flags |= ImGuiTreeNodeFlags_Selected;

	ImVec4 color = isLeaf ? ImVec4( 0, 1, 0, 1 ) : ImVec4( 1, 1, 1, 1 );

	if ( name == "NewRoot" ) {
		flags |= ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet;
		color = ImVec4( 0.9f, 0.6f, 0.6f, 1 );
	}

	ImGui::PushStyleColor( ImGuiCol_Text, color );
	const bool isOpened = ImGui::TreeNodeEx( &scene.hierarchy[node], flags, "%s", label.c_str() );
	ImGui::PopStyleColor();

	ImGui::PushID( node ); {
		if ( ImGui::IsItemHovered() && isLeaf ) {
			selectedNode = node;
		}

		if ( isOpened ) {
			for ( s32 ch = scene.hierarchy[node].firstChild; ch != -1; ch = scene.hierarchy[ch].nextSibling ) {
				if ( s32 subNode = __renderSceneTreeUI( scene, ch, selectedNode ); subNode > - 1 )
					selectedNode = subNode;
			}
			ImGui::TreePop();
		}
	}
	ImGui::PopID();
	return selectedNode;
}

ImVec2 mr::ImGuiSceneGraphComponent( const Scene &scene, s32 &selectedNode, const ImVec2 pos ) {
	ImGui::SetNextWindowPos( pos );
	ImGui::Begin( "Scene Graph:", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
		const s32 node = mr::__renderSceneTreeUI( scene, 0, selectedNode );
		if ( node > -1 ) {
			selectedNode = node;
		}
		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}
