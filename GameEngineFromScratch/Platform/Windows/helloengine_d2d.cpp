#include <Windows.h>
#include <windowsx.h>
#include <tchar.h>

#include <d2d1.h>

ID2D1Factory			*pFactory = nullptr;
ID2D1HwndRenderTarget	*pRenderTarget = nullptr;
ID2D1SolidColorBrush	*pLightSlateGrayBrush = nullptr;
ID2D1SolidColorBrush	*pCornFlowerBlueBrush = nullptr;

template<class T>
inline void SafeRelease(T **ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != nullptr)
	{
		(*ppInterfaceToRelease)->Release();
		(*ppInterfaceToRelease) = nullptr;
	}
}

HRESULT CreateGraphicsResources(HWND hWnd)
{
	HRESULT hr = S_OK;
	if (pRenderTarget == nullptr)
	{
		RECT rc;
		GetClientRect(hWnd, &rc);

		D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

		hr = pFactory->CreateHwndRenderTarget(
			D2D1::RenderTargetProperties(),
			D2D1::HwndRenderTargetProperties(hWnd, size),
			&pRenderTarget
		);

		if (SUCCEEDED(hr))
		{
			hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::LightSlateGray), &pLightSlateGrayBrush);
		}

		if (SUCCEEDED(hr))
		{
			hr = pRenderTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::CornflowerBlue), &pCornFlowerBlueBrush);
		}

		return hr;
	}
}

void DiscardGraphicsResources()
{
	SafeRelease(&pRenderTarget);
	SafeRelease(&pLightSlateGrayBrush);
	SafeRelease(&pCornFlowerBlueBrush);
}

LRESULT CALLBACK WindowProc(HWND hWnd,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);

int WINAPI WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR lpCmdLine,
	int nCmdShow)
{
	HWND hWnd;
	WNDCLASSEX wc;

	//初始化COM
	if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) return -1;

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
		_T("Engine(Direct 2D)"),
		WS_OVERLAPPEDWINDOW,
		300,
		300,
		800,
		600,
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
	CoUninitialize();

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
		if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &pFactory)))
		{
			result = -1;
			return result;
		}

		wasHandled = true;
		result = 0;
	}break;

	case WM_PAINT:
	{
		//PAINTSTRUCT ps;
		//HDC hdc = BeginPaint(hWnd, &ps);
		//RECT rect = { 20, 20, 60, 80 };
		//HBRUSH brush = (HBRUSH)GetStockObject(BLACK_BRUSH);

		//FillRect(hdc, &rect, brush);

		//EndPaint(hWnd, &ps);

		HRESULT hr = CreateGraphicsResources(hWnd);
		if (SUCCEEDED(hr))
		{
			PAINTSTRUCT ps;
			BeginPaint(hWnd, &ps);

			pRenderTarget->BeginDraw();

			pRenderTarget->Clear(D2D1::ColorF(D2D1::ColorF::White));

			D2D1_SIZE_F rtSize = pRenderTarget->GetSize();

			//grid bg
			int width = static_cast<int>(rtSize.width);
			int height = static_cast<int>(rtSize.height);

			for (int x = 0; x < width; x += 10)
			{
				pRenderTarget->DrawLine(
					D2D1::Point2F(static_cast<FLOAT>(x), 0.0f),
					D2D1::Point2F(static_cast<FLOAT>(x), rtSize.height),
					pLightSlateGrayBrush,
					0.5f
				);
			}

			for (int y = 0; y < height; y += 10)
			{
				pRenderTarget->DrawLine(
					D2D1::Point2F(0.0f, static_cast<FLOAT>(y)),
					D2D1::Point2F(rtSize.width, static_cast<FLOAT>(y)),
					pLightSlateGrayBrush,
					0.5f
				);
			}

			D2D1_RECT_F rectangle1 = D2D1::RectF(
				rtSize.width / 2 - 50.0f,
				rtSize.height / 2 - 50.0f,
				rtSize.width / 2 + 50.0f,
				rtSize.height / 2 + 50.0f
			);

			D2D1_RECT_F rectangle2 = D2D1::RectF(
				rtSize.width / 2 - 100.0f,
				rtSize.height / 2 - 100.0f,
				rtSize.width / 2 + 100.0f,
				rtSize.height / 2 + 100.0f
			);

			pRenderTarget->FillRectangle(&rectangle1, pLightSlateGrayBrush);
			pRenderTarget->FillRectangle(&rectangle2, pLightSlateGrayBrush);

			hr = pRenderTarget->EndDraw();
			if (FAILED(hr) || hr == D2DERR_RECREATE_TARGET)
			{
				DiscardGraphicsResources();
			}
			EndPaint(hWnd, &ps);
		}
		wasHandled = true;
	}break;

	case WM_SIZE:	//处理窗口尺寸变化
	{
		if (pRenderTarget != nullptr)
		{
			RECT rc;
			GetClientRect(hWnd, &rc);

			D2D1_SIZE_U size = D2D1::SizeU(rc.right - rc.left, rc.bottom - rc.top);

			pRenderTarget->Resize(size);
		}
		wasHandled = true;
	}break;

	case WM_DESTROY:
	{
		DiscardGraphicsResources();
		if (pFactory)
		{
			pFactory->Release();
			pFactory = nullptr;
		}
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