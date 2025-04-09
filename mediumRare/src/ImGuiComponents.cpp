#include "../include/ImGuiComponents.hpp"

f32 mr::__computeMaxItemWidth( const char **items, size_t itemsLength ) {
	f32 maxWidth = 0.0f;
	for ( s32 i = 0; i != itemsLength; ++i ) {
		f32 width = ImGui::CalcTextSize( items[i] ).x;
		maxWidth  = std::max<f32>( maxWidth, width );
	}
	return maxWidth;
}

ImVec2 mr::ImGuiFPSComponent( const float fps, const ImVec2 pos ) {
	ImGui::SetNextWindowPos( pos );
	ImGui::Begin( "Stats:", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
		ImGui::Text("FPS: %i, Frametime: %.2f ms", int(fps), 1000.0f / fps );
		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}

ImVec2 mr::ImGuiCameraControlsComponent( glm::vec3 &cameraPos, glm::vec3 &cameraAngles, bool &changedCameraType, const ImVec2 pos ) {
	static const char *cameraType = "FirstPerson";
	static const char *comboBoxItems[] = { "FirstPerson", "MoveTo" };
	static const char *currentComboBoxItem = cameraType;

	ImGui::SetNextWindowPos( pos );
	ImGui::SetNextWindowCollapsed( true, ImGuiCond_Once );
	ImGui::Begin( "Camera Controls:", nullptr, ImGuiWindowFlags_AlwaysAutoResize );
		static f32 dropDownWidth = __computeMaxItemWidth( comboBoxItems, IM_ARRAYSIZE(comboBoxItems)) + ImGui::GetStyle().FramePadding.x * 2
				+ ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetFrameHeight();
		ImGui::SetNextItemWidth( dropDownWidth );
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

		static f32 dropDownWidth = __computeMaxItemWidth( aaOptions, IM_ARRAYSIZE(aaOptions) ) + ImGui::GetStyle().FramePadding.x * 2
			+ ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetFrameHeight();
		ImGui::SetNextItemWidth( dropDownWidth );
		if ( ImGui::Combo( "Anti-Aliasing", &currentAA, aaOptions, IM_ARRAYSIZE(aaOptions) ) ) {
			for ( s32 i = RendererOption::NoAA; i <= RendererOption::MSAAx16; ++i )
				options[i] = false;
			options[currentAA + RendererOption::NoAA] = true;
		}

		ImGui::Checkbox( "Enable SSAO", &options[RendererOption::SSAO] );
		ImGui::Checkbox( "Blur SSAO",   &options[RendererOption::BlurSSAO] );
		if ( !options[RendererOption::SSAO] )
			options[RendererOption::BlurSSAO] = false;

		ImGui::Checkbox( "Enable Bloom", &options[RendererOption::Bloom] );
		
		const char *toneMapOptions[] = { "None", "Reinhard", "Ochimura", "Khronos PBR" };
		static s32 currentToneMapping = 0;

		static f32 ddw = __computeMaxItemWidth( toneMapOptions, IM_ARRAYSIZE(toneMapOptions) ) + ImGui::GetStyle().FramePadding.x * 2
			+ ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetFrameHeight();
		ImGui::SetNextItemWidth( ddw );
		if ( ImGui::Combo( "ToneMapping", &currentToneMapping, toneMapOptions, IM_ARRAYSIZE(toneMapOptions) ) ) {
			for ( s32 i = RendererOption::ToneMappingNone; i <= RendererOption::ToneMappingKhronosPBR; ++i )
				options[i] = false;
			options[currentToneMapping + RendererOption::ToneMappingNone] = true;
		}

		const char *cullingOptions[] = { "None", "CPU", "GPU" };
		static s32 currentCulling    = 1;

		static f32 cddw = __computeMaxItemWidth( cullingOptions, IM_ARRAYSIZE(cullingOptions) ) + ImGui::GetStyle().FramePadding.x * 2
			+ ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetFrameHeight();
		ImGui::SetNextItemWidth( cddw );
		if ( ImGui::Combo( "Frustum Culling", &currentCulling, cullingOptions, IM_ARRAYSIZE(cullingOptions) ) ) {
			for ( s32 i = RendererOption::CullingNone; i <= RendererOption::CullingGPU; ++i )
				options[i] = false;
			options[currentCulling + RendererOption::CullingNone] = true; 
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
	ImGui::SetNextWindowCollapsed( true, ImGuiCond_Once );
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
	const char *items[] = { "Depth Bias Constant", "Depth Bias Slope", "Theta", "Phi" };

	ImGui::SetNextWindowPos( pos );
	ImGui::SetNextWindowCollapsed( true, ImGuiCond_Once );
	ImGui::Begin( "Light", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

		static f32 dropDownWidth = __computeMaxItemWidth( items, IM_ARRAYSIZE(items) ) + ImGui::GetStyle().FramePadding.x * 2
				+ ImGui::GetStyle().ItemInnerSpacing.x + ImGui::GetFrameHeight();
		ImGui::PushItemWidth( dropDownWidth );
			ImGui::SliderFloat( items[0], &lightParams.depthBiasConst,    0.0f,   5.0f );
			ImGui::SliderFloat( items[1], &lightParams.depthBiasSlope,    0.0f,   5.0f );
			ImGui::SliderFloat( items[2], &lightParams.theta,          -180.0f, 180.0f );
			ImGui::SliderFloat( items[3], &lightParams.phi,             -85.0f,  85.0f );
		ImGui::PopItemWidth();

		ImGui::Separator();
		if ( ImGui::CollapsingHeader( "Preview Shadow Map" ) ) {
			ImGui::Image( shadowMapIndex, ImVec2( 512, 512 ) );
		}

		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}

ImVec2 mr::ImGuiSSAOControlsComponent( SSAOpc &pc, CombinePC &comb, s32 &blurPasses, f32 &depthThreshold, u32 ssaoTextureIndex, const ImVec2 pos ) {
	ImGui::SetNextWindowPos( pos );
	ImGui::SetNextWindowCollapsed( true, ImGuiCond_Once );
	ImGui::Begin( "SSAO Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize );

		ImGui::Text( "SSAO Blur Controls" );
			ImGui::SliderFloat( "Blur Depth Threshold", &depthThreshold, 0.0f, 50.0f );
			ImGui::SliderInt( "Blur Num Passes", &blurPasses, 1, 5 );
		ImGui::Separator();
		ImGui::Text( "SSAO Controls" );
			ImGui::SliderFloat( "SSAO radius",            &pc.radius,   0.01f, 0.1f );
			ImGui::SliderFloat( "SSAO attenuation scale", &pc.attScale,  0.5f, 1.5f );
			ImGui::SliderFloat( "SSAO distance scale",    &pc.distScale, 0.0f, 2.0f );
		ImGui::Separator();
			if ( ImGui::CollapsingHeader( "Preview SSAO Texture" ) ) {
				ImGui::Image( ssaoTextureIndex, ImVec2( 512, 512 ) );
			}
		ImGui::Separator();
		ImGui::Text( "SSAO Combine Controls" );
			ImGui::SliderFloat( "SSAO scale", &comb.scale, 0.0f, 2.0f );
			ImGui::SliderFloat( "SSAO bias",  &comb.bias,  0.0f, 0.3f );
		
		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}

ImVec2 mr::ImGuiBloomToneMapControlsComponent( ToneMapPC &pcHDR, BrightPassPC &brightPassPC, s32 &blurPasses, const ImVec2 pos ) {
	ImGui::SetNextWindowPos( pos );
	ImGui::SetNextWindowCollapsed( true, ImGuiCond_Once );
	ImGui::Begin( "Bloom & ToneMapping Controls", nullptr, ImGuiWindowFlags_AlwaysAutoResize  );

		ImGui::SliderFloat( "Bloom strength",      &pcHDR.bloomStrength, 0.0f, 1.0f );
		ImGui::SliderInt( "Bloom Blur Num Passes", &blurPasses,            1,    5 );

		ImGui::Separator();
		ImGui::Text( "Reinhard" );
		ImGui::SliderFloat( "Max White", &pcHDR.maxWhite, 0.5f, 2.0f );

		ImGui::Separator();
		ImGui::Text( "Uchimura" );
		ImGui::SliderFloat( "Max Brightness",        &pcHDR.P, 1.0f, 2.0f );
		ImGui::SliderFloat( "Contrast",              &pcHDR.a, 0.0f, 5.0f );
		ImGui::SliderFloat( "Linear Section Start",  &pcHDR.m, 0.0f, 1.0f );
		ImGui::SliderFloat( "Linear Section Length", &pcHDR.l, 0.0f, 1.0f );
		ImGui::SliderFloat( "Black Tightness",       &pcHDR.c, 1.0f, 3.0f );
		ImGui::SliderFloat( "Pedestal",              &pcHDR.b, 0.0f, 1.0f );

		ImGui::Separator();
		ImGui::Text( "Khronos PBR" );
		ImGui::SliderFloat( "Hightlight Compressions Start", &pcHDR.startCompression, 0.0f, 1.0f );
		ImGui::SliderFloat( "Desaturation Speed",            &pcHDR.desaturation,     0.0f, 1.0f );

		if ( ImGui::CollapsingHeader( "Preview Bright Pass Texture" ) ) {
			ImGui::Image(brightPassPC.texOut, ImVec2( 512, 512 ) );
		}
		if ( ImGui::CollapsingHeader( "Preview Bloom Texture" ) ) {
			ImGui::Image( pcHDR.texBloom, ImVec2( 512, 512 ) );
		}
		const ImVec2 componentSize = ImGui::GetItemRectMax();
	ImGui::End();
	return componentSize;
}
