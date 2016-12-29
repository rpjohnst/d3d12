#include "material.h"
#include "context.h"

using Microsoft::WRL::ComPtr;

HRESULT Material::create(
	Context *context,
	std::vector<char> *vertexBytecode,
	std::vector<char> *pixelBytecode,
  Material *material
) {
	D3D12_ROOT_DESCRIPTOR1 cbv = {};
	cbv.RegisterSpace = 0;
	cbv.ShaderRegister = 0;

	D3D12_ROOT_PARAMETER1 parameters[1] = {};
	parameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
	parameters[0].Descriptor = cbv;
	parameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

	ComPtr<ID3DBlob> signatureData;
	D3D12_VERSIONED_ROOT_SIGNATURE_DESC rsd = {};
	rsd.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
	rsd.Desc_1_1.NumParameters = sizeof(parameters) / sizeof(*parameters);
	rsd.Desc_1_1.pParameters = parameters;
	rsd.Desc_1_1.Flags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_PIXEL_SHADER_ROOT_ACCESS;
	TRY(D3D12SerializeVersionedRootSignature(&rsd, &signatureData, NULL));

	TRY(context->device->CreateRootSignature(
		0, signatureData->GetBufferPointer(), signatureData->GetBufferSize(),
		IID_PPV_ARGS(&material->rootSignature)
	));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psd = {};

	psd.pRootSignature = material->rootSignature.Get();
	psd.VS.pShaderBytecode = vertexBytecode->data();
	psd.VS.BytecodeLength = vertexBytecode->size();
	psd.PS.pShaderBytecode = pixelBytecode->data();
	psd.PS.BytecodeLength = pixelBytecode->size();

	for (int i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
		psd.BlendState.RenderTarget[i].SrcBlend = D3D12_BLEND_ONE;
		psd.BlendState.RenderTarget[i].DestBlend = D3D12_BLEND_ZERO;
		psd.BlendState.RenderTarget[i].BlendOp = D3D12_BLEND_OP_ADD;

		psd.BlendState.RenderTarget[i].SrcBlendAlpha = D3D12_BLEND_ONE;
		psd.BlendState.RenderTarget[i].DestBlendAlpha = D3D12_BLEND_ZERO;
		psd.BlendState.RenderTarget[i].BlendOpAlpha = D3D12_BLEND_OP_ADD;

		psd.BlendState.RenderTarget[i].LogicOp = D3D12_LOGIC_OP_NOOP;

		psd.BlendState.RenderTarget[i].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
	}

	psd.SampleMask = 0xffffffff;

	psd.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psd.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psd.RasterizerState.FrontCounterClockwise = TRUE;
	psd.RasterizerState.DepthClipEnable = TRUE;

	psd.DepthStencilState.DepthEnable = TRUE;
	psd.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	psd.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	psd.DepthStencilState.StencilEnable = FALSE;
	psd.DepthStencilState.StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
	psd.DepthStencilState.StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;

	D3D12_DEPTH_STENCILOP_DESC dsd = {};
	dsd.StencilFailOp = D3D12_STENCIL_OP_KEEP;
	dsd.StencilDepthFailOp = D3D12_STENCIL_OP_KEEP;
	dsd.StencilPassOp = D3D12_STENCIL_OP_KEEP;
	dsd.StencilFunc = D3D12_COMPARISON_FUNC_ALWAYS;
	psd.DepthStencilState.FrontFace = psd.DepthStencilState.BackFace = dsd;

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};
	psd.InputLayout.pInputElementDescs = inputLayout;
	psd.InputLayout.NumElements = sizeof(inputLayout) / sizeof(*inputLayout);

	psd.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	psd.NumRenderTargets = 1;
	psd.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	psd.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	psd.SampleDesc.Count = 1;

	TRY(context->device->CreateGraphicsPipelineState(
		&psd, IID_PPV_ARGS(&material->pipelineState)
	));

	return S_OK;

}