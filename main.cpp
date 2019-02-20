#include <windows.h>

LPCTSTR WindowClassName = "FirstWindow";
HWND WindowHandle = NULL;

const int Width = 800;
const int Height = 600;

bool InitializeWindow(HINSTANCE Instance, int ShowWindow, int Width, int Height, bool Windowed);
int MessageLoop();
LRESULT CALLBACK WindowProcedure(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam);

int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCmd)
{
	if(!InitializeWindow(Instance, ShowCmd, Width, Height, true))
	{
		MessageBox(0, "Error Initializing Window.", "Error", MB_OK | MB_ICONERROR);
		return(0);
	}

	MessageLoop();

	return 0;
}

bool InitializeWindow(HINSTANCE Instance, int CmdShow, int Width, int Height, bool Windowed)
{
	WNDCLASSEX WindowClass = {};
	WindowClass.cbSize = sizeof(WNDCLASSEX);
	WindowClass.style = CS_HREDRAW | CS_VREDRAW;
	WindowClass.lpfnWndProc = WindowProcedure;
	WindowClass.hInstance = Instance;
	WindowClass.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	WindowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WindowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 2);
	WindowClass.lpszClassName = WindowClassName;
	WindowClass.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
	if(!RegisterClassEx(&WindowClass))
	{
		MessageBox(0, "Error Registering Window Class.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	WindowHandle = CreateWindowEx(0,
		WindowClassName,
		"Window Title",
		WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		Width, Height,
		0, 0, Instance, 0);

	if(!WindowHandle)
	{
		MessageBox(0, "Error Creating Window.", "Error", MB_OK | MB_ICONERROR);
		return false;
	}

	ShowWindow(WindowHandle, CmdShow);
	UpdateWindow(WindowHandle);

	return true;
}

int MessageLoop()
{
	MSG Message = {};
	while(true)
	{
		if(PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
		{
			if (Message.message == WM_QUIT) break;
			TranslateMessage(&Message);
			DispatchMessage(&Message);
		}
		else
		{
			// Run game code
		}
	}

	return Message.wParam;
}

LRESULT CALLBACK WindowProcedure(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam)
{
	switch(Msg)
	{
		case WM_KEYDOWN:
		{
			if (WParam == VK_ESCAPE)
			{
				DestroyWindow(Window);
			}
		} break;

		case WM_DESTROY:
		{
			PostQuitMessage(0);
		} break;
	}

	return DefWindowProc(Window, Msg, WParam, LParam);
}