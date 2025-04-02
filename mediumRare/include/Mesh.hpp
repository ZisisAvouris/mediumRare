#pragma once

#include <lvk/LVK.h>

#include <meshoptimizer/src/meshoptimizer.h>
#include <shared/Scene/VtxData.h>

#include <assimp/cimport.h>
#include <assimp/postprocess.h>
#include <assimp/scene.h>

#include <shared/Scene/VtxData.h>
#include "types.hpp"

struct DrawIndexedIndirectCommand {
	u32 count;
	u32 instanceCount;
	u32 firstIndex;
	s32 baseVertex;
	u32 baseInstance;
};

class VkMesh final {
public:
	VkMesh( const std::unique_ptr<lvk::IContext> &ctx, const MeshFileHeader &header, const MeshData &meshData, lvk::Format depthFormat ) :
		num_Indices_( header.indexDataSize / sizeof(u32) ) {
		
		const u32 *indices    = meshData.indexData.data();
		const u8 *vertexData  = meshData.vertexData.data();

		bufferVertices_ = ctx->createBuffer({
			.usage     = lvk::BufferUsageBits_Vertex,
			.storage   = lvk::StorageType_Device,
			.size      = header.vertexDataSize,
			.data      = vertexData,
			.debugName = "Buffer: vertex"
		});
		bufferIndices_ = ctx->createBuffer({
			.usage     = lvk::BufferUsageBits_Index,
			.storage   = lvk::StorageType_Device,
			.size      = header.indexDataSize,
			.data      = indices,
			.debugName = "Buffer: index"
		});

		std::vector<u8> drawCommands;

		const u32 numCommands = header.meshCount;
		drawCommands.resize( sizeof(DrawIndexedIndirectCommand) * numCommands + sizeof(u32) );
		memcpy( drawCommands.data(), &numCommands, sizeof(numCommands) );

		DrawIndexedIndirectCommand *cmd = std::launder( reinterpret_cast<DrawIndexedIndirectCommand*>( drawCommands.data() + sizeof(u32) ) );
		for ( u32 i = 0; i != numCommands; ++i ) {
			*cmd++ = {
				.count         = meshData.meshes[i].getLODIndicesCount( 0 ),
				.instanceCount = 1,
				.firstIndex    = meshData.meshes[i].indexOffset,
				.baseVertex    = (s32)meshData.meshes[i].vertexOffset,
				.baseInstance  = 0
			};
		}

		bufferIndirect_ = ctx->createBuffer({
			.usage     = lvk::BufferUsageBits_Indirect,
			.storage   = lvk::StorageType_Device,
			.size      = sizeof(DrawIndexedIndirectCommand) * numCommands + sizeof(u32),
			.data      = drawCommands.data(),
			.debugName = "Buffer: indirect"
		});

		vert_ = loadShaderModule( ctx, "../shaders/main.vert" );
		geom_ = loadShaderModule( ctx, "../shaders/main.geom" );
		frag_ = loadShaderModule( ctx, "../shaders/main.frag" );

		pipeline_ = ctx->createRenderPipeline({
			.vertexInput = meshData.streams,
			.smVert      = vert_,
			.smGeom      = geom_,
			.smFrag      = frag_,
			.color       = { { .format = ctx->getSwapchainFormat() } },
			.depthFormat = depthFormat,
			.cullMode    = lvk::CullMode_Back
		});
		LVK_ASSERT( pipeline_.valid() );
	}

	void draw( lvk::ICommandBuffer &buf, const MeshFileHeader &header, const mat4 &mvp ) const {
		buf.cmdBindIndexBuffer( bufferIndices_, lvk::IndexFormat_UI32 );
		buf.cmdBindVertexBuffer( 0, bufferVertices_ );
		buf.cmdBindRenderPipeline( pipeline_ );
		buf.cmdPushConstants( mvp );
		buf.cmdBindDepthState( { .compareOp = lvk::CompareOp_Less, .isDepthWriteEnabled = true } );
		buf.cmdDrawIndexedIndirect( bufferIndirect_, sizeof(u32), header.meshCount );
	}

private:
	u32 num_Indices_;

	lvk::Holder<lvk::BufferHandle> bufferIndices_;
	lvk::Holder<lvk::BufferHandle> bufferVertices_;
	lvk::Holder<lvk::BufferHandle> bufferIndirect_;

	lvk::Holder<lvk::ShaderModuleHandle> vert_;
	lvk::Holder<lvk::ShaderModuleHandle> geom_;
	lvk::Holder<lvk::ShaderModuleHandle> frag_;

	lvk::Holder<lvk::RenderPipelineHandle> pipeline_;
};

void processLods( std::vector<u32> &indices, std::vector<f32> &vertices, std::vector<std::vector<u32>> &outLods ) {
	const size_t verticesCountIn = vertices.size() / 3;
	size_t targetIndicesCount = indices.size();

	uint8_t LOD = 1;

	printf( "[INFO]    LOD0: %i indices\n", (s32)indices.size() );
	outLods.push_back( indices );

	while ( targetIndicesCount > 1024 && LOD < kMaxLODs ) {
		targetIndicesCount = indices.size() / 2;

		bool sloppy = false;

		size_t numOptIndices = meshopt_simplify(
			indices.data(), indices.data(), (u32)indices.size(), vertices.data(),
			verticesCountIn, sizeof(f32) * 3, targetIndicesCount, 0.02f
		);
		
		if ( static_cast<size_t>( numOptIndices * 1.1f ) > indices.size() ) {
			if ( LOD > 1 ) {
				numOptIndices = meshopt_simplifySloppy( indices.data(), indices.data(), indices.size(),
					vertices.data(), verticesCountIn, sizeof(f32) * 3, targetIndicesCount, 0.02f
				);
				sloppy = true;
				if ( numOptIndices == indices.size() )
					break;
			} else {
				break;
			}
		}

		indices.resize( numOptIndices );
		meshopt_optimizeVertexCache( indices.data(), indices.data(), indices.size(), verticesCountIn );

		printf( "[INFO]    LOD%i: %i indices %s\n", (s32)LOD, (s32)numOptIndices, sloppy ? "[sloppy]" : "" );
		LOD++;
		outLods.push_back( indices );
	}
}

Mesh convertAIMesh( const aiMesh *m, MeshData &meshData, u32 &indexOffset, u32 &vertexOffset ) {
	static_assert( sizeof(aiVector3D) == 3 * sizeof(f32) );

	const bool hasTexCoords = m->HasTextureCoords( 0 );

	std::vector<f32> srcVertices;
	std::vector<u32> srcIndices;

	std::vector<std::vector<u32>> outLods;
	std::vector<u8> &vertices = meshData.vertexData;

	for ( size_t i = 0; i != m->mNumVertices; ++i ) {
		const aiVector3D v = m->mVertices[i];
		const aiVector3D n = m->mNormals[i];
		const aiVector2D t = hasTexCoords ? aiVector2D( m->mTextureCoords[0][i].x, m->mTextureCoords[0][i].y ) : aiVector2D();

		srcVertices.push_back( v.x );
		srcVertices.push_back( v.y );
		srcVertices.push_back( v.z );

		put( vertices, v );
		put( vertices, glm::packHalf2x16( vec2( t.x, t.y ) ) );
		put( vertices, glm::packSnorm3x10_1x2( vec4( n.x, n.y, n.z, 0 ) ) );
	}

	meshData.streams = {
		.attributes  = {
			{ .location = 0, .format = lvk::VertexFormat::Float3, .offset = 0 },
			{ .location = 1, .format = lvk::VertexFormat::HalfFloat2, .offset = sizeof(vec3) },
			{ .location = 2, .format = lvk::VertexFormat::Int_2_10_10_10_REV, .offset = sizeof(vec3) + sizeof(u32) }
		},
		.inputBindings = { { .stride = sizeof(vec3) + sizeof(u32) + sizeof(u32) } }
	};

	for ( size_t i = 0; i != m->mNumFaces; ++i ) {
		if ( m->mFaces[i].mNumIndices != 3 )
			continue;
		for ( size_t j = 0; j != m->mFaces[i].mNumIndices; ++j ) {
			srcIndices.push_back( m->mFaces[i].mIndices[j] );
		}
	}
	processLods( srcIndices, srcVertices, outLods );

	printf( "[INFO] Calculated LOD count: %u\n", (u32)outLods.size() );

	Mesh result = { .indexOffset = indexOffset, .vertexOffset = vertexOffset, .vertexCount = m->mNumVertices };

	u32 numIndices = 0;
	for ( u32 i = 0; i != outLods.size(); ++i ) {
		for ( u32 j = 0; j != outLods[i].size(); ++j ) {
			meshData.indexData.push_back( outLods[i][j] );
		}
		result.lodOffset[i] = numIndices;
		numIndices         += (s32)outLods[i].size();
	}

	result.lodOffset[outLods.size()] = numIndices;
	result.lodCount                  = (u32)outLods.size();

	indexOffset += numIndices;
	vertexOffset += m->mNumVertices;

	return result;
}

void loadMeshFile( const char *filename, MeshData &meshData ) {
	printf( "[INFO] Loading '%s'...\n", filename );

	const u32 flags = 0 | aiProcess_JoinIdenticalVertices | aiProcess_Triangulate | aiProcess_GenSmoothNormals |
					  aiProcess_LimitBoneWeights | aiProcess_SplitLargeMeshes | aiProcess_ImproveCacheLocality |
					  aiProcess_RemoveRedundantMaterials | aiProcess_FindDegenerates | aiProcess_FindInvalidData |
					  aiProcess_GenUVCoords;
	const aiScene *scene = aiImportFile( filename, flags );
	if ( !scene || !scene->HasMeshes() ) {
		printf( "[ERROR] Unable to load '%s'\n", filename );
		exit( 0xFF );
	}

	meshData.meshes.reserve( scene->mNumMeshes );
	meshData.boxes.reserve( scene->mNumMeshes );

	u32 indexOffset = 0, vertexOffset = 0;
	for ( u32 i = 0; i != scene->mNumMeshes; ++i ) {
		printf( "[INFO] Converting meshes %u/%u...", i+1, scene->mNumMeshes );
		fflush(stdout);
		meshData.meshes.push_back( convertAIMesh( scene->mMeshes[i], meshData, indexOffset, vertexOffset ) );
	}
	recalculateBoundingBoxes( meshData );
}
