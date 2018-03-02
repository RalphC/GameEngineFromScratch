#include <Windows.h>
#include <windowsx.h>
#include <tchar.h>
#include <stdint.h>

#include <d3d11.h>
#include <d3d11_1.h>
#include <d3dcompiler.h>
//#include <dxerr.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

const uint32_t SCREEN_WIDTH = 800;
const uint32_t SCREEN_HEIGHT = 600;

IDXGISwapChain *g_pSwapchain = nullptr;
ID3D11Device *g_pDev = nullptr;
ID3D11DeviceContext *g_pDevcon = nullptr;

ID3D11RenderTargetView *g_pRTView = nullptr;

ID3D11InputLayout *g_pLayout = nullptr;
ID3D11VertexShader *g_pVS = nullptr;
ID3D11PixelShader *g_pPS = nullptr;

ID3D11Buffer *g_pVBuffer = nullptr;

struct VERTEX {
	XMFLOAT3 Position;
	XMFLOAT4 Color;
};


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
	HRESULT hr;
	ID3D11Texture2D *pBackBuff;

	g_pSwapchain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&pBackBuff);

	g_pDev->CreateRenderTargetView(pBackBuff, NULL, &g_pRTView);

	pBackBuff->Release();

	g_pDevcon->OMSetRenderTargets(1, &g_pRTView, NULL);
}

void SetViewPort() {
	D3D11_VIEWPORT viewport;
	ZeroMemory(&viewport, sizeof(D3D11_VIEWPORT));

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = SCREEN_WIDTH;
	viewport.Height = SCREEN_HEIGHT;

	g_pDevcon->RSSetViewports(1, &viewport);
}

void InitPipeline() {
	ID3DBlob *VS, *PS;

	D3DReadFileToBlob(L"copy.vso", &VS);
	D3DReadFileToBlob(L"copy.vso", &PS);

	g_pDev->CreateVertexShader(VS->GetBufferPointer(), VS->GetBufferSize(), NULL, &g_pVS);
	g_pDev->CreatePixelShader(PS->GetBufferPointer(), PS->GetBufferSize(), NULL, &g_pPS);

	g_pDevcon->VSSetShader(g_pVS, 0, 0);
	g_pDevcon->PSSetShader(g_pPS, 0, 0);

	//在UE4里放在了FD3D11VertexElements里
	D3D11_INPUT_ELEMENT_DESC ied[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
		{"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	g_pDev->CreateInputLayout(ied, 2, VS->GetBufferPointer(), VS->GetBufferSize(), &g_pLayout);
	g_pDevcon->IASetInputLayout(g_pLayout);

	VS->Release();
	PS->Release();
}

void InitGrapghics() {
	VERTEX OurVertices[] = {
		{ XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(0.45f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-0.45f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
	};

	D3D11_BUFFER_DESC bd;
	ZeroMemory(&bd, sizeof(D3D11_BUFFER_DESC));

	bd.Usage = D3D11_USAGE_DYNAMIC;
	bd.ByteWidth = sizeof(VERTEX) * 3;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	g_pDev->CreateBuffer(&bd, NULL, &g_pVBuffer);

	D3D11_MAPPED_SUBRESOURCE ms;
	g_pDevcon->Map(g_pVBuffer, NULL, D3D11_MAP_WRITE_DISCARD, NULL, &ms);
	memcpy(ms.pData, OurVertices, sizeof(VERTEX) * 3);
	g_pDevcon->Unmap(g_pVBuffer, NULL);
}

HRESULT CreateGraphicsResources(HWND hWnd)
{
	HRESULT hr = S_OK;

	if (g_pSwapchain == nullptr)
	{
		DXGI_SWAP_CHAIN_DESC scd;

		ZeroMemory(&scd, sizeof(DXGI_SWAP_CHAIN_DESC));

		scd.BufferCount = 1;
		scd.BufferDesc.Width = SCREEN_WIDTH;
		scd.BufferDesc.Height = SCREEN_HEIGHT;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.RefreshRate.Numerator = 60;
		scd.BufferDesc.RefreshRate.Denominator = 1;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		scd.OutputWindow = hWnd;
		scd.SampleDesc.Count = 4;
		scd.Windowed = true;
		scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

		const D3D_FEATURE_LEVEL FeatureLevels[] = {
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0,
			D3D_FEATURE_LEVEL_10_1,
			D3D_FEATURE_LEVEL_10_0,
		};

		D3D_FEATURE_LEVEL FeatureLevelSupported;

		hr = D3D11CreateDeviceAndSwapChain(
			NULL,
			D3D_DRIVER_TYPE_HARDWARE,
			NULL,
			0,
			FeatureLevels,
			_countof(FeatureLevels),
			D3D11_SDK_VERSION,
			&scd,
			&g_pSwapchain,
			&g_pDev,
			&FeatureLevelSupported,
			&g_pDevcon
		);

		if (FAILED(hr))
		{
			//DXTRACE_MSG
		}

		if (SUCCEEDED(hr))
		{
			CreateRenderTarget();
			SetViewPort();
			InitPipeline();
			InitGrapghics();
		}
	}

	return hr;
}

void DiscardGraphicsResources()
{
	SafeRelease(&g_pLayout);
	SafeRelease(&g_pVS);
	SafeRelease(&g_pPS);
	SafeRelease(&g_pVBuffer);
	SafeRelease(&g_pSwapchain);
	SafeRelease(&g_pRTView);
	SafeRelease(&g_pDev);
	SafeRelease(&g_pDevcon);
}

void RenderFrame()
{
	const FLOAT clearColor[] = { 0.0f, 0.2f, 0.4f, 1.0f };
	g_pDevcon->ClearRenderTargetView(g_pRTView, clearColor);

	UINT stride = sizeof(VERTEX);
	UINT offset = 0;

	g_pDevcon->IASetVertexBuffers(0, 1, &g_pVBuffer, &stride, &offset);
	g_pDevcon->Draw(3, 0);

	g_pSwapchain->Present(0, 0);
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

	//初始化COM
	//if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return -1;

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
		_T("Engine(Direct 3D 11)"),
		WS_OVERLAPPEDWINDOW,
		100,
		100,
		SCREEN_WIDTH,
		SCREEN_HEIGHT,
		NULL,
		NULL,
		hInstance,
		NULL);

	ShowWindow(hWnd, nCmdShow);

	MSG msg;

	//while (GetMessage(&msg, NULL, 0, 0))
	while (GetMessage(&msg, nullptr, 0, 0))
	{
		TranslateMessage(&msg);

		DispatchMessage(&msg);
	}

	//清理COM
	//CoUninitialize();

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
		result = CreateGraphicsResources(hWnd);
		RenderFrame();
		wasHandled = true;
	}break;

	case WM_SIZE:	//处理窗口尺寸变化
	{
		if (g_pSwapchain != nullptr)
		{
			DiscardGraphicsResources();
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