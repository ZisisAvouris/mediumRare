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
	
		ImGui::Checkbox( "Draw Grid ",     &options[RendererOption::Grid] );
		ImGui::Checkbox( "Draw Wireframe", &options[RendererOption::Wireframe] );
		ImGui::Checkbox( "Draw Skybox",    &options[RendererOption::Skybox] );

		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}
