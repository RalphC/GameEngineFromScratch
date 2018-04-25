#include <tchar.h>
#include "WindowsApplication.hpp"

using namespace My;

namespace My {
	GfxConfiguration config(8, 8, 8, 8, 32, 0, 0, 960, 540, L"Game Engine(Windows)");
	WindowsApplication	g_App(config);
	IApplication*		g_pApp = &g_App;
}

int My::WindowsApplication::Initialize() {
	int result;

	result = BaseApplication::Initialize();
	if (result != 0) {
		exit(result);
	}

	HINSTANCE hInstance = GetModuleHandle(NULL);

	HWND hWnd;
	WNDCLASSEX wc;

	ZeroMemory(&wc, sizeof(WNDCLASSEX));

	wc.cbSize = sizeof(WNDCLASSEX);
	wc.style = CS_HREDRAW | CS_VREDRAW;
	wc.lpfnWndProc = WindowProc;
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)COLOR_WINDOW;
	wc.lpszClassName = _T("GameEngineFromScratch");

	RegisterClassEx(&wc);

	hWnd = CreateWindowExW(
		0,
		L"GameEngine",
		m_config.appName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		m_config.screenWidth,
		m_config.screenHeight,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	ShowWindow(hWnd, SW_SHOW);

	return result;
}

void My::WindowsApplication::Finalize() {

}

void My::WindowsApplication::Tick() {
	MSG msg;
	if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}
}

LRESULT CALLBACK My::WindowsApplication::WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message)
	{
	case WM_PAINT:
	{
	}break;
	case WM_DESTROY:
	{
		PostQuitMessage(0);
		BaseApplication::m_bQuit = true;
		return 0;
	}break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}
