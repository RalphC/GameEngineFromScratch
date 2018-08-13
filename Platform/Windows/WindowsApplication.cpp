#include <tchar.h>
#include "WindowsApplication.hpp"

using namespace My;

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

	hWnd = CreateWindowEx(
		0,
		_T("GameEngine"),
		m_Config.appName,
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		m_Config.screenWidth,
		m_Config.screenHeight,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	ShowWindow(hWnd, SW_SHOW);

	m_hWnd = hWnd;

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
