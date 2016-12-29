#include "util.h"

#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>
#include <wrl/client.h>
#include <vector>

struct Context;

struct Material {
	Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
	Microsoft::WRL::ComPtr<ID3D12PipelineState> pipelineState;

	static HRESULT create(
		Context *context,
		std::vector<char> *vertexBytecode,
		std::vector<char> *pixelBytecode,
	 	Material *material
	);
};