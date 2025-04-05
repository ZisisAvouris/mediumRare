#pragma once

#include <lvk/LVK.h>
#include <shared/Utils.h>

class Pipeline final {
public:
	Pipeline( const std::unique_ptr<lvk::IContext> &ctx, const lvk::VertexInput &streams, lvk::Format colorFormat, lvk::Format depthFormat, u32 numSamples = 1,
		lvk::Holder<lvk::ShaderModuleHandle> &&vert = {},
		lvk::Holder<lvk::ShaderModuleHandle> &&frag = {}, lvk::CullMode cullMode = lvk::CullMode_None) {
	
		if ( !vert.valid() || !frag.valid() )
			exit( 0xF0 );
		_vert = std::move( vert );
		_frag = std::move( frag );

		_pipeline = ctx->createRenderPipeline({
			.vertexInput      = streams,
			.smVert           = _vert,
			.smFrag           = _frag,
			.color            = { { .format = colorFormat } },
			.depthFormat      = depthFormat,
			.cullMode         = cullMode,
			.samplesCount     = numSamples,
			.minSampleShading = numSamples > 1 ? 0.25f : 0.0f
		});
		LVK_ASSERT( _pipeline.valid() );

		_pipelineWireframe = ctx->createRenderPipeline({
			.vertexInput  = streams,
			.smVert       = _vert,
			.smFrag       = _frag,
			.color        = { { .format = colorFormat } },
			.depthFormat  = depthFormat,
			.cullMode     = lvk::CullMode_None,
			.polygonMode  = lvk::PolygonMode_Line,
			.samplesCount = numSamples,
		});
		LVK_ASSERT( _pipelineWireframe.valid() );
	}
	~Pipeline() {
		_pipeline          = nullptr;
		_pipelineWireframe = nullptr;
		_vert              = nullptr;
		_frag              = nullptr;
	}

	lvk::Holder<lvk::ShaderModuleHandle> _vert;
	lvk::Holder<lvk::ShaderModuleHandle> _frag;

	lvk::Holder<lvk::RenderPipelineHandle> _pipeline;
	lvk::Holder<lvk::RenderPipelineHandle> _pipelineWireframe;;
};