#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>
#include <wrl/client.h>
#include <Windows.h>
#include <vector>

struct Context;

struct Mesh {
	Microsoft::WRL::ComPtr<ID3D12Resource> data;
	std::vector<D3D12_VERTEX_BUFFER_VIEW> vertexBuffers;
	std::vector<D3D12_INDEX_BUFFER_VIEW> indexBuffers;

	static HRESULT create(
		Context *context, ID3D12GraphicsCommandList *commandList, ID3D12Resource *uploadHeap,
		Mesh *mesh
	);
};