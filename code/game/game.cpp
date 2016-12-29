#include "mesh.h"
#include "material.h"
#include "context.h"
#include "util.h"

#define WIN32_LEAN_AND_MEAN
#include <DirectXMath.h>
#include <d3d12.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#include <ShellScalingApi.h>
#include <Windows.h>
#include <vector>
#include <iterator>
#include <fstream>

using Microsoft::WRL::ComPtr;
using namespace DirectX;

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

struct App {
	UINT dpi;
	int width;
	int height;

	Context context;
};

float clamp(float x) { if (x < 0.0) return 0.0; else if (x > 1.0) return 1.0; return x; }

int WINAPI wWinMain(
	HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow
) {
	App app_data, *app = &app_data;

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(wc);
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = L"GameApp";
	auto windowClass = RegisterClassEx(&wc);
	if (windowClass == 0) {
		printWindowsError(GetLastError());
		return 1;
	}

	auto hWnd = CreateWindowEx(
		WS_EX_APPWINDOW, (LPCWSTR)windowClass, NULL, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT, 800, 600,
		NULL, NULL, hInstance, (LPVOID)app
	);
	if (hWnd == NULL) {
		printWindowsError(GetLastError());
		return 1;
	}

	HRESULT hr;

	hr = Context::create(hWnd, app->width, app->height, &app->context);
	if (FAILED(hr)) {
		printWindowsError(hr);
		return 1;
	}

	auto vertexBinary = readFile("data/vertex.cso");
	auto pixelBinary = readFile("data/pixel.cso");

	Material material;
	hr = Material::create(&app->context, &vertexBinary, &pixelBinary, &material);
	if (FAILED(hr)) {
		printWindowsError(hr);
		return 1;
	}

	ComPtr<ID3D12Resource> uploadHeap;
	{
		D3D12_HEAP_PROPERTIES hp = {};
		hp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC rd = {};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = 8 * 1024 * 1024;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN;
		rd.SampleDesc.Count = 1;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		hr = app->context.device->CreateCommittedResource(
			&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
			IID_PPV_ARGS(&uploadHeap)
		);
		if (FAILED(hr)) {
			printWindowsError(hr);
			return 1;
		}
	}

	auto commandList = app->context.commandList.Get();
	hr = commandList->Reset(app->context.commandAllocators[app->context.frameIndex].Get(), NULL);
	if (FAILED(hr)) {
		printWindowsError(hr);
		return 1;
	}

	Mesh mesh;
	hr = Mesh::create(&app->context, commandList, uploadHeap.Get(), &mesh);
	if (FAILED(hr)) {
		printWindowsError(hr);
		return 1;
	}

	commandList->Close();
	ID3D12CommandList *commandLists[] = { commandList };
	app->context.commandQueue->ExecuteCommandLists(1, commandLists);

	app->context.fenceValues[app->context.frameIndex]++;
	hr = app->context.commandQueue->Signal(
		app->context.fence.Get(), app->context.fenceValues[app->context.frameIndex]
	);
	if (FAILED(hr)) {
		printWindowsError(hr);
		return 1;
	}

	struct ConstantsPerObject {
		XMFLOAT4X4 worldViewProj;
	};

	auto proj = XMMatrixTranspose(
		XMMatrixPerspectiveFovRH(0.5f * XM_PIDIV2, (float)app->width / app->height, 0.1f, 4.0f)
	);

	auto position = XMFLOAT4(0.0f, 1.5f, -4.0, 1.0f);
	auto target = XMFLOAT4(0.0f, 0.5f, 0.0f, 1.0f);
	auto up = XMFLOAT4(0.0f, 1.0f, 0.0f, 0.0f);
	auto view = XMMatrixTranspose(
		XMMatrixLookAtRH(XMLoadFloat4(&position), XMLoadFloat4(&target), XMLoadFloat4(&up))
	);

	auto rot = XMMatrixRotationY(0.0f);

	ComPtr<ID3D12Resource> constantHeaps[2];
	void *constantBuffers[2];
	for (int i = 0; i < 2; i++) {
		D3D12_HEAP_PROPERTIES hp = {};
		hp.Type = D3D12_HEAP_TYPE_UPLOAD;

		D3D12_RESOURCE_DESC rd = {};
		rd.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
		rd.Width = 64 * 1024;
		rd.Height = 1;
		rd.DepthOrArraySize = 1;
		rd.MipLevels = 1;
		rd.Format = DXGI_FORMAT_UNKNOWN;
		rd.SampleDesc.Count = 1;
		rd.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

		hr = app->context.device->CreateCommittedResource(
			&hp, D3D12_HEAP_FLAG_NONE, &rd, D3D12_RESOURCE_STATE_GENERIC_READ, NULL,
			IID_PPV_ARGS(&constantHeaps[i])
		);
		if (FAILED(hr)) {
			printWindowsError(hr);
			return 1;
		}

		hr = constantHeaps[i]->Map(0, NULL, &constantBuffers[i]);
		if (FAILED(hr)) {
			printWindowsError(hr);
			return 1;
		}
	}

	D3D12_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = (float)app->width;
	viewport.Height = (float)app->height;
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 0.0f;

	D3D12_RECT scissor = {};
	scissor.left = 0;
	scissor.top = 0;
	scissor.right = app->width;
	scissor.bottom = app->height;

	ShowWindow(hWnd, nCmdShow);

	while (true) {
		MSG msg = {};
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				goto out;
			}

			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		rot = XMMatrixRotationY(0.05f) * rot;

		auto worldViewProj = proj * view * rot;
		XMStoreFloat4x4((XMFLOAT4X4*)constantBuffers[app->context.frameIndex], worldViewProj);

		hr = app->context.prepare();
		if (FAILED(hr)) {
			printWindowsError(hr);
			return 1;
		}
		commandList->SetPipelineState(material.pipelineState.Get());

		auto rtv = app->context.rtvHeap->GetCPUDescriptorHandleForHeapStart();
		rtv.ptr += app->context.frameIndex * app->context.rtvDescriptorSize;

		auto dsv = app->context.dsvHeap->GetCPUDescriptorHandleForHeapStart();

		commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

		FLOAT clearColor[] = { 0.0f, 0.3f, 0.6f, 1.0f };
		commandList->ClearRenderTargetView(rtv, clearColor, 0, NULL);
		commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, NULL);

		commandList->SetGraphicsRootSignature(material.rootSignature.Get());

		commandList->RSSetViewports(1, &viewport);
		commandList->RSSetScissorRects(1, &scissor);
		commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		auto cbv = constantHeaps[app->context.frameIndex]->GetGPUVirtualAddress();
		commandList->SetGraphicsRootConstantBufferView(0, cbv);

		for (size_t i = 0; i < mesh.vertexBuffers.size(); i++) {
			commandList->IASetVertexBuffers(0, 1, &mesh.vertexBuffers[i]);
			commandList->IASetIndexBuffer(&mesh.indexBuffers[i]);

			auto count = mesh.indexBuffers[i].SizeInBytes / sizeof(uint16_t);
			commandList->DrawIndexedInstanced((UINT)count, 1, 0, 0, 0);
		}

		app->context.present();
	}
out:

	app->context.waitForGpu();
	return 0;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	auto app = (App*)GetWindowLongPtr(hWnd, GWLP_USERDATA);

	switch (msg) {
	case WM_CREATE: {
		CREATESTRUCT *cs = (LPCREATESTRUCT)lParam;
		app = (App*)cs->lpCreateParams;
		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)app);

		HMONITOR monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTOPRIMARY);
		UINT _dpiY;
		GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &app->dpi, &_dpiY);

		LONG width = cs->cx * app->dpi / 96;
		LONG height = cs->cy * app->dpi / 96;
		RECT rect = { cs->x, cs->y, cs->x + width, cs->y + height };
		AdjustWindowRectEx(&rect, cs->style, FALSE, cs->dwExStyle);

		app->width = width;
		app->height = height;

		SetWindowPos(
			hWnd, HWND_TOP,
			cs->x, cs->y, rect.right - rect.left, rect.bottom - rect.top,
			SWP_NOZORDER | SWP_NOACTIVATE
		);
		return 0;
	}

	case WM_CLOSE:
		DestroyWindow(hWnd);
		return 0;

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;

	case WM_SIZE: {
		if (!app->context.swapChain.Get()) {
			return 0;
		}

		HRESULT hr = app->context.resize(LOWORD(lParam), HIWORD(lParam));
		if (FAILED(hr)) {
			printWindowsError(hr);
			PostQuitMessage(0);
		}

		return 0;
	}

	case WM_DPICHANGED: {
		app->dpi = LOWORD(wParam);
		RECT *rect = (LPRECT)lParam;
		SetWindowPos(
			hWnd, HWND_TOP,
			rect->left, rect->top, rect->right - rect->left, rect->bottom - rect->top,
			SWP_NOZORDER | SWP_NOACTIVATE
		);

		RECT client;
		GetClientRect(hWnd, &client);
		app->width = client.right - client.left;
		app->height = client.bottom - client.top;
		return 0;
	}

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}