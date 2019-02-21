#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <WICTextureLoader.h>
#include <dxgi.h>
#include <sstream>
#include <dwrite.h>
#include <d2d1.h>

#pragma comment(lib, "d3d10_1.lib")
#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "DXGI.lib")
#pragma comment(lib, "D2D1.lib")
#pragma comment(lib, "dwrite.lib")

#if defined(DEBUG) | defined(_DEBUG)
#ifndef HR
#define HR(x) { HRESULT hr = (x); if(FAILED(hr)) { printf("Failed at File: %s on line: %d", __FILE__, __LINE__); } }
#endif
#else
#ifndef HR
#define HR(x)(x)
#endif
#endif

using namespace DirectX;

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

// DirectX Rendering Pipeline
/*
	1. Input Assembler (IA) Stage
		- Reads Geometric Data, Vertices, and Indices.
		- Uses the data to create Geometric Primitives like Triangles, Squares, Lines, Points, which will be used by other stages.
		- Indices define how the Primitives should be put together by the Vertices.

	2. Vertex Shader (VS) Stage*
		- Where all the vertices go through after the primitives have been assembled.
		- Every vertex drawn will go through here. With the VS, we can do  things like Transforms, Scales, Lighting, Displacement Mapping for Textures, etc.
		- Shaders are written in HLSL.

	These next 3 stages work together to implement Tesselation.
	Tesselation is traking a primitive object and dividing it into many smaller sections to increase the models detail really fast.

	3. Hull Shader (HS) Stage*
		- Calculates how and where to add new vertices to a primitive to make it more detailed.

	4. Tesselator Shader (TS) Stage
		- Takes input from HS, and does the dividing of the primitive.

	5. Domain Shader (DS) Stage*
		- Takes positions of new vertices, and transforms them to create more detail.

	6. Geometry Shader (GS) Stage*
		- (Optional Stage)
		- Accepts primitives as input, to create or destroy primitives.

	7. Stream Output (SO) Stage
		- Obtains Vertex Data from the pipeline from GS or VS. 
		- Vertex Data is sent to memory from the SO to be put into one or more Vertex Buffers.
		- Vertex Data output are sent as Lists. Incompletes are never sent out. Silently discarded.

	8. Rasterizer (RS) Stage
		- Takes the Vector Information (Shapes and Primitives) and turns them into pixels by Interpolating per-vertex values across each primitive.
		- Also handles Clipping (cutting primitives outside the view of the screen) with the Viewport

	9. Pixel Shader (PS) Stage*
		- Does calculations and modifies each pixel that will be seen on the screen, such as lighting on a per pixel basis.
		- Invoked by the RS once per primitive per pixel. 
		- 1:1 Mapping on a pixel
		- Calculate the final color of each pixel.

	10. Output Merger (OM) Stage
		- Final stage.
		- Takes the pixel fragments and depth/stencil buffers and determines which are actually written to the RenderTarget.

	Stages with * are programmable and created by us. The ones without, we do not program, but can change its settings via the Device Context.
*/

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

// Buffer to hold our vertex data
ID3D11Buffer *SquareVertexBuffer;
ID3D11VertexShader *VertexShader;
ID3D11PixelShader *PixelShader;

// Holds data of our indices
ID3D11Buffer *SquareIndexBuffer;

// Holds the information from our shaders
ID3D10Blob *VSBuffer;
ID3D10Blob *PSBuffer;

// Input (Vertex) Layout
ID3D11InputLayout *VertexLayout;

// Stores the Depth Stencil View and Buffer
ID3D11DepthStencilView *DepthStencilView;
ID3D11Texture2D *DepthStencilBuffer;


// Stores the constant buffer variables in our World View Projection Matrix to send to the Effect file
ID3D11Buffer *cbPerObjectBuffer;

// Matrices and Vectors of each space and the position, target, and direction of camera
XMMATRIX WVP;
XMMATRIX World;
XMMATRIX CameraView;
XMMATRIX CameraProjection;

XMVECTOR CameraPosition;
XMVECTOR CameraTarget;
XMVECTOR CameraUp;


XMMATRIX Cube1World;
XMMATRIX Cube2World;

XMMATRIX Rotation;
XMMATRIX Scale;
XMMATRIX Translation;
float Rot = 0.01f;

// Holds our texture we load
ID3D11ShaderResourceView *CubeTexture;

// Holds the sampler state info
ID3D11SamplerState *CubeTextureSamplerState;


ID3D11BlendState *TransparentBlendState;
ID3D11RasterizerState *CCCullMode;
ID3D11RasterizerState *CWCullMode;
ID3D11RasterizerState *NoCullMode;

ID3D10Device1 *D3D101Device;
IDXGIKeyedMutex *KeyedMutex1;
IDXGIKeyedMutex *KeyedMutex0;
ID2D1RenderTarget *D2DRenderTarget;
ID2D1SolidColorBrush *Brush;
ID3D11Texture2D *Backbuffer1;
ID3D11Texture2D *SharedTexture1;
ID3D11Buffer *D2DVertBuffer;
ID3D11Buffer *D2DIndexBuffer;
ID3D11ShaderResourceView *D2DTexture;
IDWriteFactory *DWriteFactory;
IDWriteTextFormat *TextFormat;

std::wstring PrintText;

ID3D11Buffer *cbPerFrameBuffer;
ID3D11PixelShader *D2D_PS;
ID3D10Blob *D2D_PS_Buffer;

// Defines our constant buffer in code is the same layout of the structure of the buffer in the effect file
struct cbPerObject
{
	XMMATRIX WVP;
	XMMATRIX World;
};
cbPerObject cbPerObj;

float Red = 0.0f;
float Green = 0.0f;
float Blue = 0.0f;
int ColorModR = 1;
int ColorModG = 1;
int ColorModB = 1;


double CountsPerSecond = 0.0;
__int64 CounterStart = 0;

int FrameCount = 0;
int FPS = 0;

__int64 FrameTimeOld = 0;
double FrameTime;

// Initializes D3D
bool InitializeD3D(HINSTANCE Instance);

// Releases objects to prevent memory leaks
void ReleaseObjects();
bool InitScene();
void UpdateScene(double time);
void DrawScene();

void StartTimer();
double GetTime();
double GetFrameTime();

struct Vertex
{
	Vertex() { }
	Vertex(float x, float y, float z, 
		   float u, float v,
			float nx, float ny, float nz) : 
		pos(x, y, z),
		texCoord(u, v),
		normal(nx, ny, nz) { }

	XMFLOAT3 pos;
	XMFLOAT2 texCoord;
	XMFLOAT3 normal;
};

struct Light
{
	Light() { ZeroMemory(this, sizeof(Light)); }
	XMFLOAT3 dir;
	float pad;
	XMFLOAT4 ambient;
	XMFLOAT4 diffuse;
};

Light light;

struct cbPerFrame
{
	Light light;
};

cbPerFrame constBufferPerFrame;



// Tells us what our Vertex structure consists of and what to do with each component of our Vertex structure
// Each element describes one element in the vertex structure.
// If we added a color, we would have a color below as well as position.
D3D11_INPUT_ELEMENT_DESC Layout[] = 
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

UINT NumLayoutElements = ARRAYSIZE(Layout);


bool InitD2D_D3D11_DWrite(IDXGIAdapter1 *Adapter);
void InitD2DScreenTexture();
void RenderText(std::wstring text, int inInt);


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
			FrameCount++;
			if(GetTime() > 1.0f)
			{
				FPS = FrameCount;
				FrameCount = 0;
				StartTimer();
			}
			FrameTime = GetFrameTime();

			UpdateScene(FrameTime);
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
	IDXGIFactory1 *DXGIFactory;
	HR(CreateDXGIFactory1(__uuidof(IDXGIFactory1), (void **)&DXGIFactory));
	IDXGIAdapter1 *Adapter;
	HR(DXGIFactory->EnumAdapters1(0, &Adapter));
	DXGIFactory->Release();

	// Describe our Backbuffer.
	DXGI_MODE_DESC BufferDesc = {};
	{
		// Width and Height of Resolution
		BufferDesc.Width = Width;
		BufferDesc.Height = Height;
		// RefreshRate (Rational), 60/1 or 60Hz
		BufferDesc.RefreshRate.Numerator = 60;
		BufferDesc.RefreshRate.Denominator = 1;
		BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
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
	HR(D3D11CreateDeviceAndSwapChain(Adapter,
		D3D_DRIVER_TYPE_UNKNOWN,
		0,
		D3D11_CREATE_DEVICE_DEBUG | D3D11_CREATE_DEVICE_BGRA_SUPPORT,
		0,
		0,
		D3D11_SDK_VERSION,
		&SwapChainDesc,
		&SwapChain,
		&D3D11Device,
		0,
		&D3D11DeviceContext));

	InitD2D_D3D11_DWrite(Adapter);

	Adapter->Release();

	// Create our Backbuffer to create our RenderTargetView
	HR(SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&Backbuffer1))
	HR(D3D11Device->CreateRenderTargetView(Backbuffer1, 0, &RenderTargetView));

	// Create our Depth/Stencil Desc
	D3D11_TEXTURE2D_DESC DepthStencilDesc = {};
	{
		DepthStencilDesc.Width = Width;
		DepthStencilDesc.Height = Height;
		DepthStencilDesc.MipLevels = 1;
		DepthStencilDesc.ArraySize = 1;
		DepthStencilDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		DepthStencilDesc.SampleDesc.Count = 1;
		DepthStencilDesc.SampleDesc.Quality = 0;
		DepthStencilDesc.Usage = D3D11_USAGE_DEFAULT;
		DepthStencilDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	}

	// Create the Depth/Stencil buffer and bind it to the OM stage of the Pipeline.
	D3D11Device->CreateTexture2D(&DepthStencilDesc, 0, &DepthStencilBuffer);
	D3D11Device->CreateDepthStencilView(DepthStencilBuffer, 0, &DepthStencilView);

	// Bind the RenderTargetView to the Output Merger state of the pipeline. 
	// NumViews is 1 since we only have 1 RenderTarget to bind
	D3D11DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);

	return true;
}

void ReleaseObjects()
{
	SwapChain->Release();
	D3D11Device->Release();
	D3D11DeviceContext->Release();
	RenderTargetView->Release();
	SquareVertexBuffer->Release();
	SquareIndexBuffer->Release();
	VertexShader->Release();
	PixelShader->Release();
	VSBuffer->Release();
	PSBuffer->Release();
	VertexLayout->Release();
	DepthStencilView->Release();
	DepthStencilBuffer->Release();
	cbPerObjectBuffer->Release();
	TransparentBlendState->Release();
	CCCullMode->Release();
	CWCullMode->Release();
	NoCullMode->Release();

	D3D101Device->Release();
	KeyedMutex1->Release();
	KeyedMutex0->Release();
	D2DRenderTarget->Release();
	Brush->Release();
	Backbuffer1->Release();
	SharedTexture1->Release();
	DWriteFactory->Release();
	TextFormat->Release();
	D2DTexture->Release();

	cbPerFrameBuffer->Release();
}

bool InitScene()
{
	InitD2DScreenTexture();

	// Compile Shaders From File
	HR(D3DCompileFromFile(L"Effects.fx", 0, 0, "VS", "vs_5_0", 0, 0, &VSBuffer, 0));
	HR(D3DCompileFromFile(L"Effects.fx", 0, 0, "PS", "ps_5_0", 0, 0, &PSBuffer, 0));
	HR(D3DCompileFromFile(L"Effects.fx", 0, 0, "D2D_PS", "ps_5_0", 0, 0, &D2D_PS_Buffer, 0));

	// Create the Shader object
	HR(D3D11Device->CreateVertexShader(VSBuffer->GetBufferPointer(), VSBuffer->GetBufferSize(), 0, &VertexShader));
	HR(D3D11Device->CreatePixelShader(PSBuffer->GetBufferPointer(), PSBuffer->GetBufferSize(), 0, &PixelShader));
	HR(D3D11Device->CreatePixelShader(D2D_PS_Buffer->GetBufferPointer(), D2D_PS_Buffer->GetBufferSize(), 0, &D2D_PS));

	// Now that the shaders are compiled and created, need to set them as our Pipelines current shader.
	D3D11DeviceContext->VSSetShader(VertexShader, 0, 0);
	D3D11DeviceContext->PSSetShader(PixelShader, 0, 0);

	light.dir = XMFLOAT3(0.25f, 0.5f, -1.0f);
	light.ambient = XMFLOAT4(0.2f, 0.2f, 0.2f, 1.0f);
	light.diffuse = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

	// Where the points meet
	Vertex Vertices[] =
	{
		// Front Face
		Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 1.0f,-1.0f, -1.0f, -1.0f),
		Vertex(-1.0f,  1.0f, -1.0f, 0.0f, 0.0f,-1.0f,  1.0f, -1.0f),
		Vertex(1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f,  1.0f, -1.0f),
		Vertex(1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f),

		// Back Face
		Vertex(-1.0f, -1.0f, 1.0f, 1.0f, 1.0f,-1.0f, -1.0f, 1.0f),
		Vertex(1.0f, -1.0f, 1.0f, 0.0f, 1.0f, 1.0f, -1.0f, 1.0f),
		Vertex(1.0f,  1.0f, 1.0f, 0.0f, 0.0f, 1.0f,  1.0f, 1.0f),
		Vertex(-1.0f,  1.0f, 1.0f, 1.0f, 0.0f,-1.0f,  1.0f, 1.0f),

		// Top Face
		Vertex(-1.0f, 1.0f, -1.0f, 0.0f, 1.0f,-1.0f, 1.0f, -1.0f),
		Vertex(-1.0f, 1.0f,  1.0f, 0.0f, 0.0f,-1.0f, 1.0f,  1.0f),
		Vertex(1.0f, 1.0f,  1.0f, 1.0f, 0.0f, 1.0f, 1.0f,  1.0f),
		Vertex(1.0f, 1.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f, -1.0f),

		// Bottom Face
		Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f,-1.0f, -1.0f, -1.0f),
		Vertex(1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, -1.0f),
		Vertex(1.0f, -1.0f,  1.0f, 0.0f, 0.0f, 1.0f, -1.0f,  1.0f),
		Vertex(-1.0f, -1.0f,  1.0f, 1.0f, 0.0f,-1.0f, -1.0f,  1.0f),

		// Left Face
		Vertex(-1.0f, -1.0f,  1.0f, 0.0f, 1.0f,-1.0f, -1.0f,  1.0f),
		Vertex(-1.0f,  1.0f,  1.0f, 0.0f, 0.0f,-1.0f,  1.0f,  1.0f),
		Vertex(-1.0f,  1.0f, -1.0f, 1.0f, 0.0f,-1.0f,  1.0f, -1.0f),
		Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 1.0f,-1.0f, -1.0f, -1.0f),

		// Right Face
		Vertex(1.0f, -1.0f, -1.0f, 0.0f, 1.0f, 1.0f, -1.0f, -1.0f),
		Vertex(1.0f,  1.0f, -1.0f, 0.0f, 0.0f, 1.0f,  1.0f, -1.0f),
		Vertex(1.0f,  1.0f,  1.0f, 1.0f, 0.0f, 1.0f,  1.0f,  1.0f),
		Vertex(1.0f, -1.0f,  1.0f, 1.0f, 1.0f, 1.0f, -1.0f,  1.0f),
	};

	// Allows to use part of another triangle to use on another
	DWORD Indices[] =
	{
		// Front Face
		0,  1,  2,
		0,  2,  3,

		// Back Face
		4,  5,  6,
		4,  6,  7,

		// Top Face
		8,  9, 10,
		8, 10, 11,

		// Bottom Face
		12, 13, 14,
		12, 14, 15,

		// Left Face
		16, 17, 18,
		16, 18, 19,

		// Right Face
		20, 21, 22,
		20, 22, 23
	};

	// Describe our Index Buffer
	D3D11_BUFFER_DESC IndexBufferDesc = {};
	IndexBufferDesc.ByteWidth = sizeof(DWORD) * 12 * 3;
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IndexBufferData = {};
	// The data we want in our buffer
	IndexBufferData.pSysMem = Indices;
	HR(D3D11Device->CreateBuffer(&IndexBufferDesc, &IndexBufferData, &SquareIndexBuffer));

	// Bind the Index Buffer in the IA (first stage)
	D3D11DeviceContext->IASetIndexBuffer(SquareIndexBuffer, DXGI_FORMAT_R32_UINT, 0);

	// Describe our Vertex Buffer
	D3D11_BUFFER_DESC VertexBufferDesc = {};
	VertexBufferDesc.ByteWidth = sizeof(Vertex) * 24;
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	// Data we want in our buffer
	D3D11_SUBRESOURCE_DATA VertexBufferData = {};
	// The data we want in our buffer
	VertexBufferData.pSysMem = Vertices;
	HR(D3D11Device->CreateBuffer(&VertexBufferDesc, &VertexBufferData, &SquareVertexBuffer));

	// Now we need to bind our Vertex Buffer to the IA (first stage)
	UINT Stride = sizeof(Vertex);
	UINT Offset = 0;
	D3D11DeviceContext->IASetVertexBuffers(0, 1, &SquareVertexBuffer, &Stride, &Offset);

	// Create the Input Layout
	HR(D3D11Device->CreateInputLayout(Layout, NumLayoutElements, VSBuffer->GetBufferPointer(), VSBuffer->GetBufferSize(), &VertexLayout));
	// Bind the Layout to the IA as the Active Layout.
	D3D11DeviceContext->IASetInputLayout(VertexLayout);

	// Set the Primitive Topology of the IA
	// Create a triangle; Every 3 vertices will make a triangle. 
	D3D11DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// Create and set our viewport.
	// Tells the RS stage what to draw.
	// Creates a square in pixels which the rasterizer uses to find where to display our geometry on the client area of our window.
	D3D11_VIEWPORT Viewport = {};
	Viewport.TopLeftX = 0;
	Viewport.TopLeftY = 0;
	Viewport.Width = Width;
	Viewport.Height = Height;
	Viewport.MinDepth = 0.0f; // Closest value in Depth
	Viewport.MaxDepth = 1.0f; // Furthest value in Depth

	D3D11DeviceContext->RSSetViewports(1, &Viewport);

	// Create Constant Buffer
	D3D11_BUFFER_DESC ConstantBufferDesc = {};
	ConstantBufferDesc.ByteWidth = sizeof(cbPerObject);
	ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	HR(D3D11Device->CreateBuffer(&ConstantBufferDesc, 0, &cbPerObjectBuffer));

	ConstantBufferDesc = {};
	ConstantBufferDesc.ByteWidth = sizeof(cbPerFrame);
	ConstantBufferDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	HR(D3D11Device->CreateBuffer(&ConstantBufferDesc, 0, &cbPerFrameBuffer));

	// Setup Camera
	CameraPosition = XMVectorSet(0.0f, 3.0f, -8.0f, 0.0f);
	CameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	CameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	// Create the Viewspace
	CameraView = XMMatrixLookAtLH(CameraPosition, CameraTarget, CameraUp);
	
	// Create projection space
	CameraProjection = XMMatrixPerspectiveFovLH((0.4f * 3.14f), (float)Width / Height, 1.0f, 1000.0f);

	// Setup the Rasterizer for Wireframe
	D3D11_RASTERIZER_DESC RasterizerDesc = {};
	RasterizerDesc.FillMode = D3D11_FILL_SOLID;
	RasterizerDesc.CullMode = D3D11_CULL_NONE;
	HR(D3D11Device->CreateRasterizerState(&RasterizerDesc, &NoCullMode));

	// Load Texture File
	HR(CreateWICTextureFromFile(D3D11Device, L"test.png", NULL, &CubeTexture));

	// Describe Sample State (How the shader will render the texture)
	D3D11_SAMPLER_DESC SamplerDesc = {};
	SamplerDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
	SamplerDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	SamplerDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
	SamplerDesc.MinLOD = 0;
	SamplerDesc.MaxLOD = D3D11_FLOAT32_MAX;

	// Create the Sampler
	HR(D3D11Device->CreateSamplerState(&SamplerDesc, &CubeTextureSamplerState));

	D3D11_BLEND_DESC BlendDesc = {};

	D3D11_RENDER_TARGET_BLEND_DESC RenderTargetBlendDesc = {};
	RenderTargetBlendDesc.BlendEnable = true;
	RenderTargetBlendDesc.SrcBlend = D3D11_BLEND_SRC_COLOR;
	RenderTargetBlendDesc.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	RenderTargetBlendDesc.BlendOp = D3D11_BLEND_OP_ADD;
	RenderTargetBlendDesc.SrcBlendAlpha = D3D11_BLEND_ONE;
	RenderTargetBlendDesc.DestBlendAlpha = D3D11_BLEND_ZERO;
	RenderTargetBlendDesc.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	RenderTargetBlendDesc.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	BlendDesc.AlphaToCoverageEnable = false;
	BlendDesc.RenderTarget[0] = RenderTargetBlendDesc;

	D3D11Device->CreateBlendState(&BlendDesc, &TransparentBlendState);

	D3D11_RASTERIZER_DESC CMDesc = {};
	CMDesc.FillMode = D3D11_FILL_SOLID;
	CMDesc.CullMode = D3D11_CULL_BACK;
	CMDesc.FrontCounterClockwise = true;
	HR(D3D11Device->CreateRasterizerState(&CMDesc, &CCCullMode));
	CMDesc.FrontCounterClockwise = false;
	HR(D3D11Device->CreateRasterizerState(&CMDesc, &CWCullMode));


	return true;
}

void UpdateScene(double time)
{
	Rot += 1.0f * time;
	if (Rot > 6.28f) // 2pi
		Rot = 0.0f;

	Cube1World = XMMatrixIdentity();

	// Cube1 World Space Matrix
	XMVECTOR rotAxis = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	Rotation = XMMatrixRotationAxis(rotAxis, Rot);
	Translation = XMMatrixTranslation(0.0f, 0.0f, 4.0f);
	Cube1World = Translation * Rotation;

	Cube2World = XMMatrixIdentity();
	Rotation = XMMatrixRotationAxis(rotAxis, -Rot);
	Scale = XMMatrixScaling(1.3f, 1.3f, 1.3f);
	Cube2World = Rotation * Scale;
}

void DrawScene()
{
	// Clear backbuffer
	FLOAT bgColor[4] = { Red, Green, Blue, 0.0f };
	D3D11DeviceContext->ClearRenderTargetView(RenderTargetView, bgColor);
	D3D11DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	constBufferPerFrame.light = light;
	D3D11DeviceContext->UpdateSubresource(cbPerFrameBuffer, 0, NULL, &constBufferPerFrame, 0, 0);
	D3D11DeviceContext->PSSetConstantBuffers(0, 1, &cbPerFrameBuffer);

	D3D11DeviceContext->VSSetShader(VertexShader, 0, 0);
	D3D11DeviceContext->PSSetShader(PixelShader, 0, 0);

	D3D11DeviceContext->OMSetRenderTargets(1, &RenderTargetView, DepthStencilView);
	D3D11DeviceContext->OMSetBlendState(0, 0, 0xffffffff);

	D3D11DeviceContext->IASetIndexBuffer(SquareIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	UINT Stride = sizeof(Vertex);
	UINT Offset = 0;
	D3D11DeviceContext->IASetVertexBuffers(0, 1, &SquareVertexBuffer, &Stride, &Offset);


	WVP = Cube1World * CameraView * CameraProjection;
	cbPerObj.World = XMMatrixTranspose(Cube1World);
	cbPerObj.WVP = XMMatrixTranspose(WVP);
	D3D11DeviceContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0);
	D3D11DeviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);
	D3D11DeviceContext->PSSetShaderResources(0, 1, &CubeTexture);
	D3D11DeviceContext->PSSetSamplers(0, 1, &CubeTextureSamplerState);

	D3D11DeviceContext->RSSetState(CWCullMode);
	D3D11DeviceContext->DrawIndexed(36, 0, 0);

	WVP = Cube2World * CameraView * CameraProjection;
	cbPerObj.World = XMMatrixTranspose(Cube2World);
	cbPerObj.WVP = XMMatrixTranspose(WVP);
	D3D11DeviceContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0);
	D3D11DeviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);
	D3D11DeviceContext->PSSetShaderResources(0, 1, &CubeTexture);
	D3D11DeviceContext->PSSetSamplers(0, 1, &CubeTextureSamplerState);

	D3D11DeviceContext->RSSetState(CWCullMode);
	D3D11DeviceContext->DrawIndexed(36, 0, 0);

	RenderText(L"FPS: ", FPS);

	// Swap the front buffer with the backbuffer
	SwapChain->Present(0, 0);
}

bool InitD2D_D3D11_DWrite(IDXGIAdapter1 *Adapter)
{
	D3D10CreateDevice1(Adapter, D3D10_DRIVER_TYPE_HARDWARE, NULL, D3D10_CREATE_DEVICE_DEBUG | D3D10_CREATE_DEVICE_BGRA_SUPPORT,
		D3D10_FEATURE_LEVEL_10_1, D3D10_1_SDK_VERSION, &D3D101Device);

	D3D11_TEXTURE2D_DESC SharedTextureDesc = {};
	SharedTextureDesc.Width = Width;
	SharedTextureDesc.Height = Height;
	SharedTextureDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	SharedTextureDesc.MipLevels = 1;
	SharedTextureDesc.ArraySize = 1;
	SharedTextureDesc.SampleDesc.Count = 1;
	SharedTextureDesc.Usage = D3D11_USAGE_DEFAULT;
	SharedTextureDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET;
	SharedTextureDesc.MiscFlags = D3D11_RESOURCE_MISC_SHARED_KEYEDMUTEX;
	HR(D3D11Device->CreateTexture2D(&SharedTextureDesc, NULL, &SharedTexture1));
	HR(SharedTexture1->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&KeyedMutex1));

	IDXGIResource *SharedResource10;
	HANDLE SharedHandle10;

	HR(SharedTexture1->QueryInterface(__uuidof(IDXGIResource), (void **)&SharedResource10));
	HR(SharedResource10->GetSharedHandle(&SharedHandle10));
	SharedResource10->Release();

	IDXGISurface1 *SharedSurface10;
	HR(D3D101Device->OpenSharedResource(SharedHandle10, __uuidof(IDXGISurface1), (void **)(&SharedSurface10)));
	HR(SharedSurface10->QueryInterface(__uuidof(IDXGIKeyedMutex), (void **)&KeyedMutex0));

	ID2D1Factory *D2DFactory;
	HR(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, __uuidof(ID2D1Factory), (void **)&D2DFactory));

	D2D1_RENDER_TARGET_PROPERTIES RenderTargetProperties = {};
	RenderTargetProperties.type = D2D1_RENDER_TARGET_TYPE_HARDWARE;
	RenderTargetProperties.pixelFormat = D2D1::PixelFormat(DXGI_FORMAT_UNKNOWN, D2D1_ALPHA_MODE_PREMULTIPLIED);

	HR(D2DFactory->CreateDxgiSurfaceRenderTarget(SharedSurface10, &RenderTargetProperties, &D2DRenderTarget));
	SharedSurface10->Release();
	D2DFactory->Release();

	HR(D2DRenderTarget->CreateSolidColorBrush(D2D1::ColorF(1.0f, 1.0f, 0.0f, 1.0f), &Brush));
	HR(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown **>(&DWriteFactory)));

	HR(DWriteFactory->CreateTextFormat(L"Script",
		NULL,
		DWRITE_FONT_WEIGHT_REGULAR,
		DWRITE_FONT_STYLE_NORMAL,
		DWRITE_FONT_STRETCH_NORMAL,
		24.0f,
		L"en-us",
		&TextFormat));

	HR(TextFormat->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING));
	HR(TextFormat->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR));

	D3D101Device->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_POINTLIST);

	return true;
}

void InitD2DScreenTexture()
{
	//Create the vertex buffer
	Vertex v[] =
	{
		// Front Face
		Vertex(-1.0f, -1.0f, -1.0f, 0.0f, 1.0f,-1.0f, -1.0f, -1.0f),
		Vertex(-1.0f,  1.0f, -1.0f, 0.0f, 0.0f,-1.0f,  1.0f, -1.0f),
		Vertex(1.0f,  1.0f, -1.0f, 1.0f, 0.0f, 1.0f,  1.0f, -1.0f),
		Vertex(1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f, -1.0f, -1.0f),
	};

	DWORD indices[] = {
		// Front Face
		0,  1,  2,
		0,  2,  3,
	};

	D3D11_BUFFER_DESC IndexBufferDesc = {};
	IndexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	IndexBufferDesc.ByteWidth = sizeof(DWORD) * 2 * 3;
	IndexBufferDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

	D3D11_SUBRESOURCE_DATA IndexInitData;
	IndexInitData.pSysMem = indices;
	D3D11Device->CreateBuffer(&IndexBufferDesc, &IndexInitData, &D2DIndexBuffer);

	D3D11_BUFFER_DESC VertexBufferDesc = {};
	VertexBufferDesc.Usage = D3D11_USAGE_DEFAULT;
	VertexBufferDesc.ByteWidth = sizeof(Vertex) * 4;
	VertexBufferDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA VertexInitData;
	VertexInitData.pSysMem = v;
	D3D11Device->CreateBuffer(&VertexBufferDesc, &VertexInitData, &D2DVertBuffer);

	D3D11Device->CreateShaderResourceView(SharedTexture1, NULL, &D2DTexture);
}

void RenderText(std::wstring text, int inInt)
{
	KeyedMutex1->ReleaseSync(0);
	KeyedMutex0->AcquireSync(0, 5);
	D2DRenderTarget->BeginDraw();

	D2DRenderTarget->Clear(D2D1::ColorF(0.0f, 0.0f, 0.0f, 0.0f));

	std::wostringstream PrintString;
	PrintString << text << inInt;
	PrintText = PrintString.str();

	D2D1_COLOR_F FontColor = D2D1::ColorF(1.0f, 1.0f, 1.0f, 1.0f);
	Brush->SetColor(FontColor);

	D2D1_RECT_F LayoutRect = D2D1::RectF(0, 0, Width, Height);

	D2DRenderTarget->DrawText(PrintText.c_str(), wcslen(PrintText.c_str()), TextFormat, LayoutRect, Brush);

	D2DRenderTarget->EndDraw();

	KeyedMutex0->ReleaseSync(1);
	KeyedMutex1->AcquireSync(1, 5);

	D3D11DeviceContext->OMSetBlendState(TransparentBlendState, NULL, 0xffffffff);

	D3D11DeviceContext->IASetIndexBuffer(D2DIndexBuffer, DXGI_FORMAT_R32_UINT, 0);
	UINT Stride = sizeof(Vertex);
	UINT Offset = 0;
	D3D11DeviceContext->IASetVertexBuffers(0, 1, &D2DVertBuffer, &Stride, &Offset);

	WVP = XMMatrixIdentity();
	cbPerObj.World = XMMatrixTranspose(WVP);
	cbPerObj.WVP = XMMatrixTranspose(WVP);
	D3D11DeviceContext->UpdateSubresource(cbPerObjectBuffer, 0, 0, &cbPerObj, 0, 0);
	D3D11DeviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);

	D3D11DeviceContext->PSSetShaderResources(0, 1, &D2DTexture);
	D3D11DeviceContext->PSSetSamplers(0, 1, &CubeTextureSamplerState);

	D3D11DeviceContext->RSSetState(CWCullMode);
	D3D11DeviceContext->DrawIndexed(6, 0, 0);
}

void StartTimer()
{
	LARGE_INTEGER FrequencyCount;
	QueryPerformanceFrequency(&FrequencyCount);

	CountsPerSecond = double(FrequencyCount.QuadPart);

	QueryPerformanceFrequency(&FrequencyCount);
	CounterStart = FrequencyCount.QuadPart;
}

double GetTime()
{
	LARGE_INTEGER _CurrentTime;
	QueryPerformanceCounter(&_CurrentTime);
	return double(_CurrentTime.QuadPart - CounterStart) / CountsPerSecond;
}

double GetFrameTime()
{
	LARGE_INTEGER _CurrentTime;
	__int64 _TickCount;
	QueryPerformanceCounter(&_CurrentTime);

	_TickCount = _CurrentTime.QuadPart - FrameTimeOld;
	FrameTimeOld = _CurrentTime.QuadPart;

	if (_TickCount < 0.0f)
		_TickCount = 0.0f;

	return float(_TickCount) / CountsPerSecond;
}