#include <Windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <exception>

#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>

#include <wrl\client.h>

#include "d3dx12.h"

namespace My {
	class com_exception : public std::exception
	{
	public:
		com_exception(HRESULT hr) : result(hr) {}

		virtual const char* what() const override {
			static char s_str[64] = { 0 };
			sprintf_s(s_str, "Failure with HRESULT of %08X", static_cast<unsigned int>(result));
			return s_str;
		}

	private:
		HRESULT result;
	};

	inline void ThrowIfFailed(HRESULT hr) {
		if (FAILED(hr)) {
			throw com_exception(hr);
		}
	}
}

using namespace DirectX;
using namespace DirectX::PackedVector;
using namespace Microsoft::WRL;
using namespace std;
using namespace My;

const uint32_t nScreenWidth = 960;
const uint32_t nScreenHeight = 480;

const uint32_t nFrameCount = 2;
const bool bUseWarpDevice = true;

D3D12_VIEWPORT g_ViewPort = { 0.0f, 0.0f, static_cast<float>(nScreenWidth), static_cast<float>(nScreenHeight) };

D3D12_RECT g_ScissorRect = { 0, 0, nScreenWidth, nScreenHeight };

ComPtr<IDXGISwapChain3>			g_pSwapChain = nullptr;
ComPtr<ID3D12Device>			g_pDev = nullptr;
ComPtr<ID3D12Resource>			g_pRenderTargets[nFrameCount];
ComPtr<ID3D12CommandAllocator>	g_pCommandAllocator;
ComPtr<ID3D12CommandQueue>		g_pCommandQueue;
ComPtr<ID3D12RootSignature>		g_pRootSignature;
ComPtr<ID3D12DescriptorHeap>	g_pRtvHeap;
ComPtr<ID3D12PipelineState>		g_pPipelineState;

ComPtr<ID3D12GraphicsCommandList> g_pCommandList;

uint32_t g_nRtvDescriptorSize;

ComPtr<ID3D12Resource> g_pVertexBuffer;
D3D12_VERTEX_BUFFER_VIEW	g_VertexBufferView;

//同步
uint32_t g_nFrameIndex;
HANDLE g_hFenceEvent;
ComPtr<ID3D12Fence> g_pFence;
uint32_t g_nFenceValue;

//vertex buffer
struct VERTEX {
	XMFLOAT3 Position;
	XMFLOAT4 Color;
};

wstring g_AssetsPath;

std::wstring GetAssetFullPath(LPCWSTR assetName)
{
	return g_AssetsPath + assetName;
}

void GetAssetsPath(WCHAR* path, UINT pathSize)
{
	if (path == nullptr) {
		throw std::exception();
	}

	DWORD size = GetModuleFileNameW(nullptr, path, pathSize);
	if (size == 0 || size == pathSize) {
		throw std::exception();
	}

	WCHAR* lastSlash = wcsrchr(path, L'\\');
	if (lastSlash) {
		*(lastSlash + 1) = L'\0';
	}
}

void WaitForPreviousFrame() {
	const uint64_t fence = g_nFenceValue;
	ThrowIfFailed(g_pCommandQueue->Signal(g_pFence.Get(), fence));
	g_nFenceValue++;

	if (g_pFence->GetCompletedValue() < fence) {
		ThrowIfFailed(g_pFence->SetEventOnCompletion(fence, g_hFenceEvent));
		WaitForSingleObject(g_hFenceEvent, INFINITE);
	}

	g_nFrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();
}

template<class T>
inline void SafeRelease(T **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != nullptr)
	{
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = nullptr;
	}
}

void CreateRenderTarget() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = nFrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(g_pDev->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_pRtvHeap)));

	g_nRtvDescriptorSize = g_pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_pRtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (uint32_t i = 0; i < nFrameCount; i++) {
		ThrowIfFailed(g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_pRenderTargets[i])));
		g_pDev->CreateRenderTargetView(g_pRenderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, g_nRtvDescriptorSize);
	}
}

void InitPipeline() {
	ThrowIfFailed(g_pDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_pCommandAllocator)));

	CD3DX12_ROOT_SIGNATURE_DESC rsd;
	rsd.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	ThrowIfFailed(D3D12SerializeRootSignature(&rsd, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error));
	ThrowIfFailed(g_pDev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_pRootSignature)));

#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif // defined(_DEBUG)

	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

	D3DCompileFromFile(
		GetAssetFullPath(L"copy.vs").c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"vs_5_0",
		compileFlags,
		0,
		&vertexShader,
		&error
	);

	if (error) {
		OutputDebugString((LPCTSTR)error->GetBufferPointer());
		error.Reset();
		throw std::exception();
	}

	D3DCompileFromFile(
		GetAssetFullPath(L"copy.ps").c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_0",
		compileFlags,
		0,
		&pixelShader,
		&error
	);

	if (error) {
		OutputDebugString((LPCTSTR)error->GetBufferPointer());
		error.Reset();
		throw std::exception();
	}

	D3D12_INPUT_ELEMENT_DESC ied[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psod = {};
	psod.InputLayout = { ied, _countof(ied) };
	psod.pRootSignature = g_pRootSignature.Get();
	psod.VS = { reinterpret_cast<UINT*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	psod.PS = { reinterpret_cast<UINT*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
	psod.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psod.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psod.DepthStencilState.DepthEnable = FALSE;
	psod.DepthStencilState.StencilEnable = FALSE;
	psod.SampleMask = UINT_MAX;
	psod.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psod.NumRenderTargets = 1;
	psod.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psod.SampleDesc.Count = 1;
	ThrowIfFailed(g_pDev->CreateGraphicsPipelineState(&psod, IID_PPV_ARGS(&g_pPipelineState)));

	ThrowIfFailed(g_pDev->CreateCommandList(0,
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		g_pCommandAllocator.Get(),
		g_pPipelineState.Get(),
		IID_PPV_ARGS(&g_pCommandList)));

	ThrowIfFailed(g_pCommandList->Close());
}

void InitGrapghics() {
	VERTEX OurVertices[] = {
		{ XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(0.45f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-0.45f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
	};

	const UINT vertexBufferSize = sizeof(OurVertices);

	ThrowIfFailed(g_pDev->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&g_pVertexBuffer)));

	uint8_t* pVertexDataBegin;
	CD3DX12_RANGE readRange(0, 0);
	ThrowIfFailed(g_pVertexBuffer->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
	memcpy(pVertexDataBegin, OurVertices, vertexBufferSize);
	g_pVertexBuffer->Unmap(0, nullptr);

	g_VertexBufferView.BufferLocation = g_pVertexBuffer->GetGPUVirtualAddress();
	g_VertexBufferView.StrideInBytes = sizeof(VERTEX);
	g_VertexBufferView.SizeInBytes = vertexBufferSize;

	ThrowIfFailed(g_pDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_pFence)));
	g_nFenceValue = 1;

	g_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (g_hFenceEvent == nullptr) {
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	WaitForPreviousFrame();
}

void GetHardwareAdapter(IDXGIFactory4* pFactory, IDXGIAdapter1** ppAdapter) {
	IDXGIAdapter1* pAdapter = nullptr;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != pFactory->EnumAdapters1(adapterIndex, &pAdapter); ++adapterIndex) {
		DXGI_ADAPTER_DESC1 desc;
		pAdapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))) {
			break;
		}
	}

	*ppAdapter = pAdapter;
}

HRESULT CreateGraphicsResources(HWND hWnd)
{
	HRESULT hr = S_OK;

	if (g_pSwapChain.Get() == nullptr)
	{
#if defined(_DEBUG)
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
			debugController->EnableDebugLayer();
		}
#endif // defined(_DEBUG)

		ComPtr<IDXGIFactory4> factory;
		ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&factory)));

		if (bUseWarpDevice) {
			ComPtr<IDXGIAdapter> warpAdapter;
			ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

			ThrowIfFailed(D3D12CreateDevice(
				warpAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&g_pDev)));
		}
		else {
			ComPtr<IDXGIAdapter1> hardwareAdapter;
			GetHardwareAdapter(factory.Get(), &hardwareAdapter);

			ThrowIfFailed(D3D12CreateDevice(
				hardwareAdapter.Get(),
				D3D_FEATURE_LEVEL_11_0,
				IID_PPV_ARGS(&g_pDev)));
		}

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

		ThrowIfFailed(g_pDev->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&g_pCommandQueue)));


		DXGI_SWAP_CHAIN_DESC scd;

		ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

		scd.BufferCount = nFrameCount;
		scd.BufferDesc.Width = nScreenWidth;
		scd.BufferDesc.Height = nScreenHeight;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.RefreshRate.Numerator = 60;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		scd.OutputWindow = hWnd;
		scd.SampleDesc.Count = 1;
		scd.Windowed = true;
		scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		ComPtr<IDXGISwapChain> swapChain;
		ThrowIfFailed(factory->CreateSwapChain(
			g_pCommandQueue.Get(),
			&scd,
			&swapChain));

		ThrowIfFailed(swapChain.As(&g_pSwapChain));

		g_nFrameIndex = g_pSwapChain->GetCurrentBackBufferIndex();

		CreateRenderTarget();
		InitPipeline();
		InitGrapghics();
	}

	return hr;
}

void DiscardGraphicsResources()
{
	WaitForPreviousFrame();
	CloseHandle(g_hFenceEvent);
}

void PopulateCommandList() {
	ThrowIfFailed(g_pCommandAllocator->Reset());

	ThrowIfFailed(g_pCommandList->Reset(g_pCommandAllocator.Get(), g_pPipelineState.Get()));

	g_pCommandList->SetGraphicsRootSignature(g_pRootSignature.Get());
	g_pCommandList->RSSetViewports(1, &g_ViewPort);
	g_pCommandList->RSSetScissorRects(1, &g_ScissorRect);

	g_pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		g_pRenderTargets[g_nFrameIndex].Get(),
		D3D12_RESOURCE_STATE_PRESENT,
		D3D12_RESOURCE_STATE_RENDER_TARGET));

	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_pRtvHeap->GetCPUDescriptorHandleForHeapStart(), g_nFrameIndex, g_nRtvDescriptorSize);
	g_pCommandList->OMSetRenderTargets(1, &rtvHandle, FALSE, nullptr);

	const FLOAT clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };

	g_pCommandList->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);


	g_pCommandList->IASetVertexBuffers(0, 1, &g_VertexBufferView);
	g_pCommandList->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	g_pCommandList->DrawInstanced(3, 1, 0, 0);


	g_pCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		g_pRenderTargets[g_nFrameIndex].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET,
		D3D12_RESOURCE_STATE_PRESENT));

	ThrowIfFailed(g_pCommandList->Close());
}

void RenderFrame() {
	PopulateCommandList();

	ID3D12CommandList* ppCommandLists[] = { g_pCommandList.Get() };
	g_pCommandQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);

	ThrowIfFailed(g_pSwapChain->Present(1, 0));

	WaitForPreviousFrame();
}

LRESULT CALLBACK WindowProc(HWND hWnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);

//unicode version winmain
int WINAPI wWinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPWSTR lpwCmdLine,
	int nCmdShow)
{
	UNREFERENCED_PARAMETER(hPrevInstance);
	UNREFERENCED_PARAMETER(nCmdShow);

	HWND hWnd;
	WNDCLASSEX wc;

	WCHAR assetsPath[512];
	GetAssetsPath(assetsPath, _countof(assetsPath));
	g_AssetsPath = assetsPath;

	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = _T("WindowClass1");

	RegisterClassEx(&wc);

	hWnd = CreateWindowEx(0,
		_T("WindowClass1"),
		_T("Engine(Direct 3D 12)"),
		WS_OVERLAPPEDWINDOW,
		100,
		100,
		nScreenWidth,
		nScreenHeight,
		NULL,
		NULL,
		hInstance,
		NULL);

	ShowWindow(hWnd, nCmdShow);

	MSG msg;

	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);

		DispatchMessage(&msg);
	}

	return msg.wParam;
}

LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	LRESULT result = 0;
	bool wasHandled = false;

	switch (message)
	{
	case WM_CREATE:
	{
		//if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
		//{
		//	result = -1;
		//	return result;
		//}

		wasHandled = true;
		//result = 0;
	}break;

	case WM_PAINT:
	{
		CreateGraphicsResources(hWnd);
		RenderFrame();
		wasHandled = true;
	}break;

	case WM_SIZE:	//处理窗口尺寸变化
	{
		if (g_pSwapChain != nullptr)
		{
			DiscardGraphicsResources();
			g_pSwapChain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
		}
		wasHandled = true;
	}break;

	case WM_DESTROY:
	{
		DiscardGraphicsResources();
		PostQuitMessage(0);
		result = 0;
		wasHandled = true;
	}break;

	case WM_DISPLAYCHANGE:	//显示分辨率变化
	{
		InvalidateRect(hWnd, nullptr, false);
		wasHandled = true;
	}break;
	}

	//return DefWindowProc(hWnd, message, wParam, lParam);
	if (!wasHandled)
	{
		result = DefWindowProc(hWnd, message, wParam, lParam);
	}
	return result;
}