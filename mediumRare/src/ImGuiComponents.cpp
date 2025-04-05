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
		ImGui::Checkbox( "Draw Light Frustum",  &options[RendererOption::LightFrustum] );
		
		const char* aaOptions[] = { "No AA", "MSAAx2", "MSAAx4", "MSAAx8", "MSAAx16" };
		static s32 currentAA = 0;
		if ( ImGui::Combo( "Anti-Aliasing", &currentAA, aaOptions, IM_ARRAYSIZE(aaOptions) ) ) {
			for ( s32 i = RendererOption::NoAA; i <= RendererOption::MSAAx16; ++i )
				options[i] = false;
			options[currentAA + RendererOption::NoAA] = true;
		}

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

bool mr::__editTransformUI( const mat4 &view, const mat4 &proj, mat4 &matrix ) {
	static ImGuizmo::OPERATION gizmoOperation( ImGuizmo::TRANSLATE );

	ImGui::Text( "Transforms:" );
	if ( ImGui::RadioButton( "Translate", gizmoOperation == ImGuizmo::TRANSLATE ) ) {
		gizmoOperation = ImGuizmo::TRANSLATE;
	}

	if ( ImGui::RadioButton( "Rotate", gizmoOperation == ImGuizmo::ROTATE ) ) {
		gizmoOperation = ImGuizmo::ROTATE;
	}

	ImGuiIO &io = ImGui::GetIO();
	ImGuizmo::SetRect( 0, 0, io.DisplaySize.x, io.DisplaySize.y );
	return ImGuizmo::Manipulate( glm::value_ptr(view), glm::value_ptr(proj), gizmoOperation, ImGuizmo::WORLD, glm::value_ptr(matrix) );
}

bool mr::__editMaterialUI( Scene &scene, MeshData &meshData, s32 node, s32 &outUpdateMaterialIndex, const TextureCache &textureCache ) {
	static s32 *textureToEdit = nullptr;

	if ( !scene.materialForNode.contains(node) ) {
		return false;
	}

	const u32 matIdx   = scene.materialForNode[node];
	Material &material = meshData.materials[matIdx];

	bool updated = false;
	updated |= ImGui::ColorEdit3( "Emissive color", glm::value_ptr(material.emissiveFactor) );
	updated |= ImGui::ColorEdit3( "Base color", glm::value_ptr(material.baseColorFactor) );

	const char *ImagesGalleryName = "Images Gallery";

	auto drawTextureUI = [&textureCache, ImagesGalleryName]( const char *name, s32 &texture ) {
		if ( texture == -1 )
			return;

		ImGui::Text( "%s", name );
		ImGui::Image( textureCache[texture].index(), ImVec2( 512, 512 ), ImVec2( 0, 1 ), ImVec2( 1, 0 ) );
		if ( ImGui::IsItemClicked() ) {
			textureToEdit = &texture;
			ImGui::OpenPopup( ImagesGalleryName );
		}
	};

	ImGui::Separator();
	ImGui::Text( "Click on a texture to change it!" );
	ImGui::Separator();

	drawTextureUI( "Base texture:",     material.baseColorTexture );
	drawTextureUI( "Emissive texture:", material.emissiveTexture );
	drawTextureUI( "Normal texture:",   material.normalTexture );
	drawTextureUI( "Opacity texture:",  material.opacityTexture );

	if ( const ImGuiViewport *v = ImGui::GetMainViewport() ) {
		ImGui::SetNextWindowPos( ImVec2(v->WorkSize.x * 0.5f, v->WorkSize.y * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f) );
	}
	if ( ImGui::BeginPopupModal(ImagesGalleryName, nullptr, ImGuiWindowFlags_AlwaysAutoResize) ) {
		for ( s32 i = 0; i != textureCache.size(); ++i ) {
			if ( i && i % 4 != 0 )
				ImGui::SameLine();
			ImGui::Image( textureCache[i].index(), ImVec2(256, 256), ImVec2(0, 1), ImVec2(1, 0) );
			if ( ImGui::IsItemHovered() ) {
				ImGui::GetWindowDrawList()->AddRectFilled(ImGui::GetItemRectMin(), ImGui::GetItemRectMax(), 0x66FFFFFF );
			}

			if ( ImGui::IsItemClicked() ) {
				*textureToEdit = i;
				updated        = true;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	if ( updated ) {
		outUpdateMaterialIndex = static_cast<s32>( matIdx );
	}
	return updated;
}

ImVec2 mr::ImGuiEditNodeComponent( Scene &scene, MeshData &meshData, const mat4 &view, const mat4 &proj, s32 node, s32 &outUpdateMaterialIndex, const TextureCache &textureCache ) {
	ImGuizmo::SetOrthographic( false );
	ImGuizmo::BeginFrame();

	std::string name  = getNodeName( scene, node );
	std::string label = name.empty() ? std::string("Node") + std::to_string(node) : name;
	label             = "Node: " + label;

	if ( const ImGuiViewport *v = ImGui::GetMainViewport() ) {
		ImGui::SetNextWindowPos( ImVec2( v->WorkSize.x * 0.83f, 0.0f ) );
		ImGui::SetNextWindowSize( ImVec2( v->WorkSize.x / 6, v->WorkSize.y ) );
	}
	ImGui::Begin( "Editor", nullptr, ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize );
	if ( !name.empty() ) {
		ImGui::Text( "%s", label.c_str() );
	}

	if ( node >= 0 ) {
		ImGui::Separator();
		ImGuizmo::PushID( 1 );

		mat4 globalTransform = scene.globalTransform[node];
		mat4 srcTransform    = globalTransform;
		mat4 localTransform  = scene.localTransform[node];

		if ( mr::__editTransformUI( view, proj, globalTransform ) ) {
			mat4 deltaTransform        = glm::inverse( srcTransform ) * globalTransform;
			scene.localTransform[node] = localTransform * deltaTransform;
			markAsChanged( scene, node );
		}

		ImGui::Separator();
		ImGui::Text( "%s", "Material" );

		mr::__editMaterialUI( scene, meshData, node, outUpdateMaterialIndex, textureCache );
		ImGuizmo::PopID();
	}
	ImGui::End();
	return ImVec2();
}

ImVec2 mr::ImGuiLightControlsComponent( LightParams &lightParams, u32 shadowMapIndex, const ImVec2 pos ) {
	ImGui::SetNextWindowPos( pos );
	ImGui::Begin( "Light", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
		ImGui::SliderFloat( "Depth Bias Constant", &lightParams.depthBiasConst,    0.0f,   5.0f );
		ImGui::SliderFloat( "Depth Bias Slope",    &lightParams.depthBiasSlope,    0.0f,   5.0f );
		ImGui::SliderFloat( "Theta",               &lightParams.theta,          -180.0f, 180.0f );
		ImGui::SliderFloat( "Phi",                 &lightParams.phi,             -85.0f,  85.0f );

		ImGui::Separator();
		if ( ImGui::CollapsingHeader( "Preview Shadow Map" ) ) {
			ImGui::Image( shadowMapIndex, ImVec2( 512, 512 ) );
		}

		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}
