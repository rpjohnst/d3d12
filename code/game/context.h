#define WIN32_LEAN_AND_MEAN
#include <d3d12.h>
#include <dxgi1_5.h>
#include <wrl/client.h>

struct Context {
	static const size_t BUFFER_COUNT = 2;
	size_t frameIndex = 0;

	Microsoft::WRL::ComPtr<ID3D12Device1> device;
	Microsoft::WRL::ComPtr<ID3D12CommandQueue> commandQueue;
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocators[BUFFER_COUNT];
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> commandList;

	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> rtvHeap;
	UINT rtvDescriptorSize;
	Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> dsvHeap;

	Microsoft::WRL::ComPtr<ID3D12Fence> fence;
	UINT64 fenceValues[BUFFER_COUNT] = {};
	HANDLE fenceEvent;

	Microsoft::WRL::ComPtr<IDXGISwapChain4> swapChain;
	Microsoft::WRL::ComPtr<ID3D12Resource> renderTargets[BUFFER_COUNT];
	Microsoft::WRL::ComPtr<ID3D12Resource> depthStencil;

	static HRESULT create(HWND hWnd, UINT width, UINT height, Context *context);
	HRESULT resize(UINT width, UINT height);

	HRESULT prepare();
	HRESULT present();

	HRESULT waitForGpu();
};