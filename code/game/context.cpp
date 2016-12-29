#include "context.h"
#include "util.h"
#include <dxgi1_5.h>

#if defined(_DEBUG)
#include <dxgidebug.h>
#endif

using Microsoft::WRL::ComPtr;

HRESULT chooseAdapter(IDXGIFactory5 *factory, IDXGIAdapter3 **adapter);
HRESULT getRenderTargets(Context *context, UINT width, UINT height);

HRESULT Context::create(HWND hWnd, UINT width, UINT height, Context *context) {
#if defined(_DEBUG)
	ComPtr<ID3D12Debug> debugController;
	TRY(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
	debugController->EnableDebugLayer();

	ComPtr<IDXGIInfoQueue> infoQueue;
	TRY(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&infoQueue)));
	infoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
	infoQueue->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
#endif

	ComPtr<IDXGIFactory5> factory;
	TRY(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

	ComPtr<IDXGIAdapter3> adapter;
	TRY(chooseAdapter(factory.Get(), &adapter));

	// device-dependent objects

	TRY(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&context->device)));

	D3D12_COMMAND_QUEUE_DESC cqd = {};
	cqd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	TRY(context->device->CreateCommandQueue(&cqd, IID_PPV_ARGS(&context->commandQueue)));

	for (int i = 0; i < Context::BUFFER_COUNT; i++) {
		TRY(context->device->CreateCommandAllocator(
			D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&context->commandAllocators[i])
		));
	}

	TRY(context->device->CreateCommandList(
		0, D3D12_COMMAND_LIST_TYPE_DIRECT, context->commandAllocators[0].Get(), NULL,
		IID_PPV_ARGS(&context->commandList)
	));
	context->commandList->Close();

	{
		D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
		dhd.NumDescriptors = Context::BUFFER_COUNT;
		TRY(context->device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&context->rtvHeap)));
		context->rtvDescriptorSize = context->device->GetDescriptorHandleIncrementSize(dhd.Type);
	}

	{
		D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
		dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
		dhd.NumDescriptors = 1;
		TRY(context->device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&context->dsvHeap)));
	}

	TRY(context->device->CreateFence(
		context->fenceValues[0], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&context->fence))
	);

	context->fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (context->fenceEvent == NULL) {
		return GetLastError();
	}

	// window-dependent objects

	DXGI_SWAP_CHAIN_DESC1 scd = {};
	scd.Width = width;
	scd.Height = height;
	scd.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scd.SampleDesc.Count = 1;
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = Context::BUFFER_COUNT;
	scd.Scaling = DXGI_SCALING_NONE;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scd.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	scd.Flags = DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;

	ComPtr<IDXGISwapChain1> swapChain1;
	TRY(factory->CreateSwapChainForHwnd(
		context->commandQueue.Get(), hWnd, &scd, NULL, NULL, &swapChain1
	));
	TRY(swapChain1.As(&context->swapChain));
	TRY(context->swapChain->SetMaximumFrameLatency(1));
	TRY(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_WINDOW_CHANGES));

	context->frameIndex = context->swapChain->GetCurrentBackBufferIndex();
	TRY(getRenderTargets(context, width, height));

	return S_OK;
}

HRESULT chooseAdapter(IDXGIFactory5 *factory, IDXGIAdapter3 **result) {
	ComPtr<IDXGIAdapter3> adapter;

	HRESULT hr;
	for (auto adapterIndex = 0; ; adapterIndex++) {
		ComPtr<IDXGIAdapter1> adapter1;
		hr = factory->EnumAdapters1(adapterIndex, &adapter1);
		if (hr == DXGI_ERROR_NOT_FOUND) {
			break;
		} else if (FAILED(hr)) {
			return hr;
		}

		TRY(adapter1.As(&adapter));

		DXGI_ADAPTER_DESC2 ad;
		adapter->GetDesc2(&ad);
		if (ad.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			adapter.Reset();
			continue;
		}

		hr = D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device1), NULL);
		if (FAILED(hr)) {
			continue;
		}

		break;
	}

	if (adapter.Get() == NULL) {
		return hr;
	}

	*result = adapter.Detach();
	return S_OK;
}

HRESULT Context::resize(UINT width, UINT height) {
	this->waitForGpu();
	for (int i = 0; i < Context::BUFFER_COUNT; i++) {
		this->renderTargets[i].Reset();
		this->fenceValues[i] = this->fenceValues[this->frameIndex];
	}

	TRY(this->swapChain->ResizeBuffers(
		0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT
	));

	this->frameIndex = this->swapChain->GetCurrentBackBufferIndex();
	TRY(getRenderTargets(this, width, height));

	return S_OK;
}

HRESULT getRenderTargets(Context *context, UINT width, UINT height) {
	auto rtv = context->rtvHeap->GetCPUDescriptorHandleForHeapStart();
	for (UINT i = 0; i < Context::BUFFER_COUNT; i++) {
		TRY(context->swapChain->GetBuffer(i, IID_PPV_ARGS(&context->renderTargets[i])));

		D3D12_RENDER_TARGET_VIEW_DESC rtvd = {};
		rtvd.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
		rtvd.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
		context->device->CreateRenderTargetView(context->renderTargets[i].Get(), &rtvd, rtv);

		rtv.ptr += context->rtvDescriptorSize;
	}

	D3D12_HEAP_PROPERTIES hp = {};
	hp.Type = D3D12_HEAP_TYPE_DEFAULT;

	D3D12_RESOURCE_DESC rd = {};
	rd.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	rd.Width = width;
	rd.Height = height;
	rd.DepthOrArraySize = 1;
	rd.MipLevels = 1;
	rd.Format = DXGI_FORMAT_D32_FLOAT;
	rd.SampleDesc.Count = 1;
	rd.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	rd.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;

	D3D12_CLEAR_VALUE dscv = {};
	dscv.Format = DXGI_FORMAT_D32_FLOAT;
	dscv.DepthStencil.Depth = 1.0f;

	TRY(context->device->CreateCommittedResource(
		&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_DEPTH_WRITE, &dscv,
		IID_PPV_ARGS(&context->depthStencil)
	));

	auto dsv = context->dsvHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvd = {};
	dsvd.Format = DXGI_FORMAT_D32_FLOAT;
	dsvd.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	context->device->CreateDepthStencilView(context->depthStencil.Get(), &dsvd, dsv);

	return S_OK;
}

HRESULT Context::prepare() {
	TRY(this->commandAllocators[this->frameIndex]->Reset());
	TRY(this->commandList->Reset(this->commandAllocators[this->frameIndex].Get(), NULL));

	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Transition.pResource = this->renderTargets[this->frameIndex].Get();
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	this->commandList->ResourceBarrier(1, &rb);

	return S_OK;
}

HRESULT Context::present() {
	D3D12_RESOURCE_BARRIER rb = {};
	rb.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	rb.Transition.pResource = this->renderTargets[this->frameIndex].Get();
	rb.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	rb.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	this->commandList->ResourceBarrier(1, &rb);

	TRY(this->commandList->Close());

	ID3D12CommandList *const commandLists[] = { this->commandList.Get() };
	this->commandQueue->ExecuteCommandLists(1, commandLists);

	TRY(this->swapChain->Present(1, 0));

	auto currentFenceValue = this->fenceValues[this->frameIndex];
	TRY(this->commandQueue->Signal(this->fence.Get(), currentFenceValue));

	this->frameIndex = this->swapChain->GetCurrentBackBufferIndex();
	auto nextFenceValue = this->fenceValues[this->frameIndex];
	if (this->fence->GetCompletedValue() < nextFenceValue) {
		TRY(this->fence->SetEventOnCompletion(nextFenceValue, this->fenceEvent));
		WaitForSingleObjectEx(this->fenceEvent, INFINITE, FALSE);
	}
	this->fenceValues[this->frameIndex] = currentFenceValue + 1;

	return S_OK;
}

HRESULT Context::waitForGpu() {
	auto fenceValue = this->fenceValues[this->frameIndex];
	TRY(this->commandQueue->Signal(this->fence.Get(), fenceValue));
	TRY(this->fence->SetEventOnCompletion(fenceValue, this->fenceEvent));
	WaitForSingleObjectEx(this->fenceEvent, INFINITE, FALSE);
	this->fenceValues[this->frameIndex]++;

	return S_OK;
}
