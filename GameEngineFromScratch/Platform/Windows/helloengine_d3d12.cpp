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
#include "Mesh.h"

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
uint32_t g_nCbvSrvDescriptorSize;

ComPtr<ID3D12Resource> g_pVertexBuffer;
D3D12_VERTEX_BUFFER_VIEW	g_VertexBufferView;

ComPtr<ID3D12DescriptorHeap>	g_pDsvHeap;
ComPtr<ID3D12DescriptorHeap>	g_pCbvSrvHeap;
ComPtr<ID3D12DescriptorHeap>	g_pSamplerHeap;
ComPtr<ID3D12Resource>			g_pIndexBuffer;
D3D12_INDEX_BUFFER_VIEW			g_IndexBufferView;
ComPtr<ID3D12Resource>			g_pTextureBuffer;
ComPtr<ID3D12Resource>			g_pDepthStrncilBuffer;
ComPtr<ID3D12Resource>			g_pConstantUploadBuffer;

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

struct SimpleMeshVertex {
	XMFLOAT3 m_position;
	XMFLOAT3 m_normal;
	XMFLOAT4 m_tangent;
	XMFLOAT2 m_uv;
};

SimpleMesh torus;

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

XMMATRIX g_mWorldToViewMatrix;
XMMATRIX g_mViewToWorldMatrix;
XMMATRIX g_mLightToWorldMatrix;
XMMATRIX g_mProjectionMatrix;
XMMATRIX g_mViewProjectionMatrix;

void InitConstant() {
	g_mViewToWorldMatrix = XMMatrixIdentity();
	const XMVECTOR lightPositionX = XMVectorSet(-1.5f, 4.0f, 9.0f, 1.0f);
	const XMVECTOR lightTargetX = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	const XMVECTOR lightUpX = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_mLightToWorldMatrix = XMMatrixInverse(nullptr, XMMatrixLookAtRH(lightPositionX, lightTargetX, lightUpX));

	const float g_depthNear = 1.0f;
	const float g_depthFar = 100.0f;
	const float aspect = static_cast<float>(nScreenWidth) / static_cast<float>(nScreenHeight);
	g_mProjectionMatrix = XMMatrixPerspectiveOffCenterRH(-aspect, aspect, -1, 1, g_depthNear, g_depthFar);

	const XMVECTOR eyePos = XMVectorSet(0.0f, 0.0f, 2.5f, 1.0f);
	const XMVECTOR lookAtPos = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
	const XMVECTOR upVec = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	g_mWorldToViewMatrix = XMMatrixLookAtRH(eyePos, lookAtPos, upVec);
	g_mViewToWorldMatrix = XMMatrixInverse(nullptr, g_mWorldToViewMatrix);

	g_mViewProjectionMatrix = g_mWorldToViewMatrix * g_mProjectionMatrix;
}

void BuildTorusMesh(
	float outerRadius, float innerRadius,
	uint16_t outerQuads, uint16_t innerQuads,
	float outerRepeats, float innerRepeats,
	SimpleMesh* pDestMesh)
{
	const uint32_t outerVertices = outerQuads + 1;
	const uint32_t innerVertices = innerQuads + 1;
	const uint32_t vertices = outerVertices * innerVertices;
	const uint32_t numInnerQuadsFullStripes = 1;
	const uint32_t innerQuadsLastStripe = 0;
	const uint32_t triangles = 2 * outerQuads * innerQuads; // 2 triangles per quad

	pDestMesh->m_vertexCount = vertices;
	pDestMesh->m_vertexStride = sizeof(SimpleMeshVertex);
	pDestMesh->m_vertexAttributeCount = kVertexElemCount;
	pDestMesh->m_vertexBufferSize = pDestMesh->m_vertexCount * pDestMesh->m_vertexStride;

	pDestMesh->m_indexCount = triangles * 3;            // 3 vertices per triangle
	pDestMesh->m_indexType = IndexSize::kIndexSize16;  // whenever possible, use smaller index 
													   // to save memory and enhance cache performance.
	pDestMesh->m_primitiveType = PrimitiveType::kPrimitiveTypeTriList;
	pDestMesh->m_indexBufferSize = pDestMesh->m_indexCount * sizeof(uint16_t);

	// build vertices 
	pDestMesh->m_vertexBuffer = new uint8_t[pDestMesh->m_vertexBufferSize];

	SimpleMeshVertex* outV = static_cast<SimpleMeshVertex*>(pDestMesh->m_vertexBuffer);
	const XMFLOAT2 textureScale = XMFLOAT2(outerRepeats / (outerVertices - 1.0f), innerRepeats / (innerVertices - 1.0f));
	for (uint32_t o = 0; o < outerVertices; ++o)
	{
		const float outerTheta = o * 2 * XM_PI / (outerVertices - 1);
		const XMMATRIX outerToWorld = XMMatrixTranslation(outerRadius, 0, 0) * XMMatrixRotationZ(outerTheta);

		for (uint32_t i = 0; i < innerVertices; ++i)
		{
			const float innerTheta = i * 2 * XM_PI / (innerVertices - 1);
			const XMMATRIX innerToOuter = XMMatrixTranslation(innerRadius, 0, 0) * XMMatrixRotationY(innerTheta);
			const XMMATRIX localToWorld = innerToOuter * outerToWorld;
			XMVECTOR v = XMVectorSet(0.0f, 0.0f, 0.0f, 1.0f);
			v = XMVector4Transform(v, localToWorld);
			XMStoreFloat3(&outV->m_position, v);
			v = XMVectorSet(1.0f, 0.0f, 0.0f, 0.0f);
			v = XMVector4Transform(v, localToWorld);
			XMStoreFloat3(&outV->m_normal, v);
			v = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
			v = XMVector4Transform(v, localToWorld);
			XMStoreFloat4(&outV->m_tangent, v);
			outV->m_uv.x = o * textureScale.x;
			outV->m_uv.y = i * textureScale.y;
			++outV;
		}
	}

	// build indices
	pDestMesh->m_indexBuffer = new uint8_t[pDestMesh->m_indexBufferSize];

	uint16_t* outI = static_cast<uint16_t*>(pDestMesh->m_indexBuffer);
	uint16_t const numInnerQuadsStripes = numInnerQuadsFullStripes + (innerQuadsLastStripe > 0 ? 1 : 0);
	for (uint16_t iStripe = 0; iStripe < numInnerQuadsStripes; ++iStripe)
	{
		uint16_t const innerVertex0 = iStripe * innerQuads;

		for (uint16_t o = 0; o < outerQuads; ++o)
		{
			for (uint16_t i = 0; i < innerQuads; ++i)
			{
				const uint16_t index[4] = {
					static_cast<uint16_t>((o + 0) * innerVertices + innerVertex0 + (i + 0)),
					static_cast<uint16_t>((o + 0) * innerVertices + innerVertex0 + (i + 1)),
					static_cast<uint16_t>((o + 1) * innerVertices + innerVertex0 + (i + 0)),
					static_cast<uint16_t>((o + 1) * innerVertices + innerVertex0 + (i + 1)),
				};
				outI[0] = index[0];
				outI[1] = index[2];
				outI[2] = index[1];
				outI[3] = index[1];
				outI[4] = index[2];
				outI[5] = index[3];
				outI += 6;
			}
		}
	}
}

const uint32_t nTextureWidth = 512;
const uint32_t nTextureHeight = 512;
const uint32_t nTexturePixelSize = 4;

uint8_t* GenerateTextureData()
{
	const uint32_t nRowPitch = nTextureWidth * nTexturePixelSize;
	const uint32_t nCellPitch = nRowPitch >> 3;		// The width of a cell in the checkboard texture.
	const uint32_t nCellHeight = nTextureWidth >> 3;	// The height of a cell in the checkerboard texture.
	const uint32_t nTextureSize = nRowPitch * nTextureHeight;
	uint8_t* pData = new uint8_t[nTextureSize];

	for (uint32_t n = 0; n < nTextureSize; n += nTexturePixelSize)
	{
		uint32_t x = n % nRowPitch;
		uint32_t y = n / nRowPitch;
		uint32_t i = x / nCellPitch;
		uint32_t j = y / nCellHeight;

		if (i % 2 == j % 2)
		{
			pData[n] = 0x00;		// R
			pData[n + 1] = 0x00;	// G
			pData[n + 2] = 0x00;	// B
			pData[n + 3] = 0xff;	// A
		}
		else
		{
			pData[n] = 0xff;		// R
			pData[n + 1] = 0xff;	// G
			pData[n + 2] = 0xff;	// B
			pData[n + 3] = 0xff;	// A
		}
	}

	return pData;
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

void CreateDescriptorHeaps() {
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = nFrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(g_pDev->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_pRtvHeap)));

	g_nRtvDescriptorSize = g_pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//dpeth stencil view
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(g_pDev->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&g_pDsvHeap)));

	//shader resource view/constant buffer view
	D3D12_DESCRIPTOR_HEAP_DESC cbvSrvHeapDesc = {};
	cbvSrvHeapDesc.NumDescriptors = nFrameCount + 1;	//framecount + shader
	cbvSrvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	cbvSrvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(g_pDev->CreateDescriptorHeap(&cbvSrvHeapDesc, IID_PPV_ARGS(&g_pCbvSrvHeap)));

	//sampler
	D3D12_DESCRIPTOR_HEAP_DESC samplerHeapDesc = {};
	samplerHeapDesc.NumDescriptors = 1;
	samplerHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER;
	samplerHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(g_pDev->CreateDescriptorHeap(&samplerHeapDesc, IID_PPV_ARGS(&g_pSamplerHeap)));

	g_nCbvSrvDescriptorSize = g_pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	ThrowIfFailed(g_pDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_pCommandAllocator)));

}

void CreateRenderTarget() {
	/*D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = nFrameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(g_pDev->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&g_pRtvHeap)));

	g_nRtvDescriptorSize = g_pDev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);*/
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(g_pRtvHeap->GetCPUDescriptorHandleForHeapStart());

	for (uint32_t i = 0; i < nFrameCount; i++) {
		ThrowIfFailed(g_pSwapChain->GetBuffer(i, IID_PPV_ARGS(&g_pRenderTargets[i])));
		g_pDev->CreateRenderTargetView(g_pRenderTargets[i].Get(), nullptr, rtvHandle);
		rtvHandle.Offset(1, g_nRtvDescriptorSize);
	}
}

void InitPipeline() {
	//ThrowIfFailed(g_pDev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&g_pCommandAllocator)));

	//CD3DX12_ROOT_SIGNATURE_DESC rsd;
	//rsd.Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> signature;
	ComPtr<ID3DBlob> error;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(g_pDev->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData)))) {
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	};

	CD3DX12_DESCRIPTOR_RANGE1 ranges[3];
	ranges[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);
	ranges[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 1, 0);
	ranges[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, nFrameCount, 0, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC);

	CD3DX12_ROOT_PARAMETER1 rootParameters[3];
	rootParameters[0].InitAsDescriptorTable(1, &ranges[0], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[1].InitAsDescriptorTable(1, &ranges[1], D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameters[2].InitAsDescriptorTable(1, &ranges[2], D3D12_SHADER_VISIBILITY_ALL);


	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags =
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameters), rootParameters, 0, nullptr, rootSignatureFlags);

	ComPtr<ID3D10Blob> signature;

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signature, &error));
	ThrowIfFailed(g_pDev->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&g_pRootSignature)));

#if defined(_DEBUG)
	UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
	UINT compileFlags = 0;
#endif // defined(_DEBUG)

	ComPtr<ID3DBlob> vertexShader;
	ComPtr<ID3DBlob> pixelShader;

	D3DCompileFromFile(
		L"simple.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"VSMain",
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
		L"simple.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"PSMain",
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
		{"TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 40, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
	};

	//pso
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psod = {};
	psod.InputLayout = { ied, _countof(ied) };
	psod.pRootSignature = g_pRootSignature.Get();
	psod.VS = { reinterpret_cast<UINT*>(vertexShader->GetBufferPointer()), vertexShader->GetBufferSize() };
	psod.PS = { reinterpret_cast<UINT*>(pixelShader->GetBufferPointer()), pixelShader->GetBufferSize() };
	psod.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psod.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psod.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	//psod.DepthStencilState.DepthEnable = FALSE;
	//psod.DepthStencilState.StencilEnable = FALSE;
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

	//ThrowIfFailed(g_pCommandList->Close());
}

void InitGrapghics() {

	ComPtr<ID3D12Resource> pVertexBufferUploadHeap;
	ComPtr<ID3D12Resource> pIndexBufferUploadHeap;
	ComPtr<ID3D12Resource> pTextureUploadHeap;
	BuildTorusMesh(0.8f, 0.2f, 64, 32, 4, 1, &torus);

	/*VERTEX OurVertices[] = {
		{ XMFLOAT3(0.0f, 0.5f, 0.0f), XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(0.45f, -0.5f, 0.0f), XMFLOAT4(0.0f, 1.0f, 0.0f, 1.0f) },
		{ XMFLOAT3(-0.45f, -0.5f, 0.0f), XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f) },
	};

	const UINT vertexBufferSize = sizeof(OurVertices);*/



	//vertex buffer
	ThrowIfFailed(g_pDev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(torus.m_vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&g_pVertexBuffer)));

	ThrowIfFailed(g_pDev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(torus.m_vertexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&pVertexBufferUploadHeap)));

	D3D12_SUBRESOURCE_DATA vertexData = {};
	vertexData.pData = torus.m_vertexBuffer;
	vertexData.RowPitch = torus.m_vertexStride;
	vertexData.SlicePitch = vertexData.RowPitch;

	UpdateSubresources<1>(g_pCommandList.Get(), g_pVertexBuffer.Get(), pVertexBufferUploadHeap.Get(), 0, 0, 1, &vertexData);
	g_pCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			g_pVertexBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_VERTEX_AND_CONSTANT_BUFFER));


	g_VertexBufferView.BufferLocation = g_pVertexBuffer->GetGPUVirtualAddress();
	g_VertexBufferView.StrideInBytes = torus.m_vertexStride;
	g_VertexBufferView.SizeInBytes = torus.m_vertexBufferSize;

	//index buffer
	ThrowIfFailed(g_pDev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(torus.m_indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&g_pIndexBuffer)));

	ThrowIfFailed(g_pDev->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(torus.m_indexBufferSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&pIndexBufferUploadHeap)));

	D3D12_SUBRESOURCE_DATA indexData = {};
	indexData.pData = torus.m_indexBuffer;
	indexData.RowPitch = torus.m_indexType;
	indexData.SlicePitch = indexData.RowPitch;

	UpdateSubresources<1>(g_pCommandList.Get(), g_pIndexBuffer.Get(), pIndexBufferUploadHeap.Get(), 0, 0, 1, &indexData);
	g_pCommandList->ResourceBarrier(
		1,
		&CD3DX12_RESOURCE_BARRIER::Transition(
			g_pIndexBuffer.Get(),
			D3D12_RESOURCE_STATE_COPY_DEST,
			D3D12_RESOURCE_STATE_INDEX_BUFFER));


	g_IndexBufferView.BufferLocation = g_pIndexBuffer->GetGPUVirtualAddress();
	g_IndexBufferView.Format = DXGI_FORMAT_R16_UINT;
	g_IndexBufferView.SizeInBytes = torus.m_indexBufferSize;

	uint8_t* pTextureData = GenerateTextureData();

	//TODO
	

	//sync object
	ThrowIfFailed(g_pDev->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_pFence)));
	g_nFenceValue = 1;

	//sync event
	g_hFenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	if (g_hFenceEvent == nullptr) {
		ThrowIfFailed(HRESULT_FROM_WIN32(GetLastError()));
	}

	WaitForPreviousFrame();
	delete[] pTextureData;
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