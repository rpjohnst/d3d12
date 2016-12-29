#include "mesh.h"
#include "context.h"
#include "util.h"
#include <d3d12.h>
#include <fstream>
#include <memory>

struct Vertex {
	float position[3];
	float normal[3];
	float texcoord[2];
};

struct Group {
	size_t numVertices;
	size_t numIndices;
};

HRESULT Mesh::create(
	Context *context, ID3D12GraphicsCommandList *commandList, ID3D12Resource *uploadHeap,
	Mesh *mesh
) {
	std::ifstream data("data/human.mesh", std::ios::binary);
	size_t numGroups;
	data.read((char*)&numGroups, sizeof(numGroups));

	std::vector<Group> groups;

	size_t bufferSize = 0;
	for (size_t i = 0; i < numGroups; i++) {
		Group group = {};
		data.read((char*)&group.numVertices, sizeof(group.numVertices));
		data.read((char*)&group.numIndices, sizeof(group.numIndices));

		bufferSize += group.numVertices * sizeof(Vertex) + group.numIndices * sizeof(uint16_t);
		groups.push_back(group);
	}

	{
		D3D12_HEAP_PROPERTIES hp = {};
		hp.Type = D3D12_HEAP_TYPE_DEFAULT;

		D3D12_RESOURCE_DESC rd = {};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = bufferSize;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN;
		rd.SampleDesc.Count = 1;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		TRY(context->device->CreateCommittedResource(
			&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_COPY_DEST, NULL,
			IID_PPV_ARGS(&mesh->data)
		));
	}

	void *target;
	uploadHeap->Map(0, NULL, &target);
	data.read((char*)target, bufferSize);
	uploadHeap->Unmap(0, NULL);
	commandList->CopyBufferRegion(mesh->data.Get(), 0, uploadHeap, 0, bufferSize);

	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Transition.pResource = mesh->data.Get();
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
	rb.Transition.StateAfter =
		D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER |
		D3D12_RESOURCE_STATE_INDEX_BUFFER;
	commandList->ResourceBarrier(1, &rb);

	auto address = mesh->data->GetGPUVirtualAddress();
	for (auto &group : groups) {
		D3D12_VERTEX_BUFFER_VIEW vbv = {};
		vbv.BufferLocation = address;
		vbv.SizeInBytes = (UINT)(group.numVertices * sizeof(Vertex));
		vbv.StrideInBytes = sizeof(Vertex);

		mesh->vertexBuffers.push_back(vbv);
		address += vbv.SizeInBytes;

		D3D12_INDEX_BUFFER_VIEW ibv = {};
		ibv.BufferLocation = address;
		ibv.SizeInBytes = (UINT)(group.numIndices * sizeof(uint16_t));
		ibv.Format = DXGI_FORMAT_R16_UINT;

		mesh->indexBuffers.push_back(ibv);
		address += ibv.SizeInBytes;
	}

	return S_OK;
}