#include <windows.h>
#include <d3d11.h>
#include <stdio.h>

#pragma comment(lib, "d3d11.lib")

#if defined(DEBUG) | defined(_DEBUG)
#ifndef HR
#define HR(x) { HRESULT hr = (x); if(FAILED(hr)) { printf("Failed at File: %s on line: %d", __FILE__, __LINE__); } }
#endif
#else
#ifndef HR
#define HR(x)(x)
#endif
#endif


// Window Information
//////////////////////////////////////////////////////////////
LPCTSTR WindowClassName = "FirstWindow";
HWND WindowHandle = NULL;
const int Width = 800;
const int Height = 600;


bool InitializeWindow(HINSTANCE Instance, int ShowWindow, int Width, int Height, bool Windowed);
int MessageLoop();
LRESULT CALLBACK WindowProcedure(HWND Window, UINT Msg, WPARAM WParam, LPARAM LParam);
//////////////////////////////////////////////////////////////


// DirectX Information
//////////////////////////////////////////////////////////////
// SwapChain: Used to change the backbuffer <-> front buffer. (Double Buffering to prevent scanlines/tearing)
IDXGISwapChain *SwapChain;

// Device: Represents our hardware (GPU)
// Assists in loading of models, etc
ID3D11Device *D3D11Device;

// Device Context: Split from ID3D11Device to call the Rendering methods
// while the Device calls everything else. Helps with multi-threading.
// The Device loads the model or object, while the DeviceContext continues to render our scene.
ID3D11DeviceContext *D3D11DeviceContext;

// RenderTargetView: 2D Texture (or Backbuffer), that is written to in order to render.
// This texture is sent to the output merger state of the pipeline as our Render Target and is rendered to the screen.
ID3D11RenderTargetView *RenderTargetView;

float Red = 0.0f;
float Green = 0.0f;
float Blue = 0.0f;
int ColorModR = 1;
int ColorModG = 1;
int ColorModB = 1;

// Initializes D3D
bool InitializeD3D(HINSTANCE Instance);

// Releases objects to prevent memory leaks
void ReleaseObjects();
bool InitScene();
void UpdateScene();
void DrawScene();

//////////////////////////////////////////////////////////////


int WINAPI WinMain(HINSTANCE Instance, HINSTANCE PrevInstance, LPSTR CommandLine, int ShowCmd)
{
	if(!InitializeWindow(Instance, ShowCmd, Width, Height, true))
	{
		MessageBox(0, "Error Initializing Window.", "Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	if(!InitializeD3D(Instance))
	{
		MessageBox(0, "Error Initializing D3D.", "Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	if(!InitScene())
	{
		MessageBox(0, "Error Initializing Scene.", "Error", MB_OK | MB_ICONERROR);
		return 0;
	}

	MessageLoop();

	ReleaseObjects();

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
			UpdateScene();
			DrawScene();
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

bool InitializeD3D(HINSTANCE Instance)
{
	// Describe our Backbuffer.
	DXGI_MODE_DESC BufferDesc = {};
	{
		// Width and Height of Resolution
		BufferDesc.Width = Width;
		BufferDesc.Height = Height;
		// RefreshRate (Rational), 60/1 or 60Hz
		BufferDesc.RefreshRate.Numerator = 60;
		BufferDesc.RefreshRate.Denominator = 1;
		// DXGI_FORMAT enum type. We use DXGI_FORMAT_R8G8B8A8_UNORM to represent
		// 32-bit unsigned integer, 8bits for each RGBA
		BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		// DXGI_MODE_SCANLINE_ORDER enum type. Describes the manner in which the rasterizer will render onto the surface.
		// Since we use Double Buffering, this wont be noticeable, so we can set to unspecified.
		BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		// DXGI_MODE_SCALING enum type. Explains how an image is stretched to fit monitor resolution.
		// UNSPECIFIED, CENTERED, STRETCHED
		BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
	}

	// Describe our SwapChain
	DXGI_SWAP_CHAIN_DESC SwapChainDesc = {};
	{
		// Describes our backbuffer above
		SwapChainDesc.BufferDesc = BufferDesc;
		// DXGI_SAMPLE_DESC struct to describe Multisampling.
		// Multisampling smoothes the choppiness in lines or edges created because pixels on monitors are not infinitely small.
		// Since they are little blocks, you can see choppiness in diagonal lines and edges on a screen.
		SwapChainDesc.SampleDesc.Count = 1;
		SwapChainDesc.SampleDesc.Quality = 0;
		// DXGI_USAGE enum type. Describes the access the CPU has to the Surface of the Backbuffer.
		// DXGI_USAGE_RENDER_TARGET_OUTPUT so we can render to the Backbuffer.
		SwapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		// Number of Backbuffers we will use. 1 => Double Buffers, 2 => Triple Buffering, n => x Buffering
		SwapChainDesc.BufferCount = 1;
		SwapChainDesc.OutputWindow = WindowHandle;

		SwapChainDesc.Windowed = TRUE;
		// DXGI_SWAP_EFFECT enum type. Describes what the Display Driver should do with the Front Buffer after swapping it to the back.
		// We use DISCARD to let the driver decide what is most efficient to do with it.
		SwapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
	}

	// Create the D3D Device, Device Context, and Swap Chain
	HR(D3D11CreateDeviceAndSwapChain(0,
		D3D_DRIVER_TYPE_HARDWARE,
		0,
		0,
		0,
		0,
		D3D11_SDK_VERSION,
		&SwapChainDesc,
		&SwapChain,
		&D3D11Device,
		0,
		&D3D11DeviceContext));


	// Create our Backbuffer to create our RenderTargetView
	ID3D11Texture2D *Backbuffer;
	HR(SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&Backbuffer))

	// Create our RenderTargetView, to send to the Output Merger state of the pipeline.
	HR(D3D11Device->CreateRenderTargetView(Backbuffer, 0, &RenderTargetView));
	Backbuffer->Release();

	// Bind the RenderTargetView to the Output Merger state of the pipeline. 
	// Also will bind our Depth/Stencil Buffer as well, but set to 0 for now, since it hasn't been created
	// NumViews is 1 since we only have 1 RenderTarget to bind
	D3D11DeviceContext->OMSetRenderTargets(1, &RenderTargetView, 0);

	return true;
}

void ReleaseObjects()
{
	SwapChain->Release();
	D3D11Device->Release();
	D3D11DeviceContext->Release();
}

bool InitScene()
{
	return true;
}

void UpdateScene()
{
	Red += ColorModR * 0.00005f;
	Green += ColorModG * 0.00002f;
	Blue += ColorModB * 0.00001f;

	if (Red >= 1.0f || Red <= 0.0f)
		ColorModR *= -1;
	if (Green >= 1.0f || Green <= 0.0f)
		ColorModG *= -1;
	if (Blue >= 1.0f || Blue <= 0.0f)
		ColorModB *= -1;;
}

void DrawScene()
{
	// Clear backbuffer
	FLOAT bgColor[4] = { Red, Green, Blue, 1.0f };
	D3D11DeviceContext->ClearRenderTargetView(RenderTargetView, bgColor);

	// Swap the front buffer with the backbuffer
	SwapChain->Present(0, 0);
}