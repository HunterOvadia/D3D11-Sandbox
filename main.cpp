#include <windows.h>
#include <d3d11.h>
#include <stdio.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d3dcompiler.lib")

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

// Defines our constant buffer in code is the same layout of the structure of the buffer in the effect file
struct cbPerObject
{
	XMMATRIX WVP;
};
cbPerObject cbPerObj;

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

struct Vertex
{
	Vertex() { }
	Vertex(float x, float y, float z, 
		   float cr, float cg, float cb, float ca) : 
		pos(x, y, z),
		color(cr, cg, cb, ca) { }

	DirectX::XMFLOAT3 pos;
	DirectX::XMFLOAT4 color;
};

// Tells us what our Vertex structure consists of and what to do with each component of our Vertex structure
// Each element describes one element in the vertex structure.
// If we added a color, we would have a color below as well as position.
D3D11_INPUT_ELEMENT_DESC Layout[] = 
{
	{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
};

UINT NumLayoutElements = ARRAYSIZE(Layout);

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
}

bool InitScene()
{
	// Compile Shaders From File
	HR(D3DCompileFromFile(L"Effects.fx", 0, 0, "VS", "vs_5_0", 0, 0, &VSBuffer, 0));
	HR(D3DCompileFromFile(L"Effects.fx", 0, 0, "PS", "ps_5_0", 0, 0, &PSBuffer, 0));

	// Create the Shader object
	HR(D3D11Device->CreateVertexShader(VSBuffer->GetBufferPointer(), VSBuffer->GetBufferSize(), 0, &VertexShader));
	HR(D3D11Device->CreatePixelShader(PSBuffer->GetBufferPointer(), PSBuffer->GetBufferSize(), 0, &PixelShader));

	// Now that the shaders are compiled and created, need to set them as our Pipelines current shader.
	D3D11DeviceContext->VSSetShader(VertexShader, 0, 0);
	D3D11DeviceContext->PSSetShader(PixelShader, 0, 0);

	// Where the points meet
	Vertex Vertices[] =
	{
		Vertex(-1.0f, -1.0f, -1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
		Vertex(-1.0f, +1.0f, -1.0f, 0.0f, 1.0f, 0.0f, 1.0f),
		Vertex(+1.0f, +1.0f, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f),
		Vertex(+1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 0.0f, 1.0f),
		Vertex(-1.0f, -1.0f, +1.0f, 0.0f, 1.0f, 1.0f, 1.0f),
		Vertex(-1.0f, +1.0f, +1.0f, 1.0f, 1.0f, 1.0f, 1.0f),
		Vertex(+1.0f, +1.0f, +1.0f, 1.0f, 0.0f, 1.0f, 1.0f),
		Vertex(+1.0f, -1.0f, +1.0f, 1.0f, 0.0f, 0.0f, 1.0f),
	};

	// Allows to use part of another triangle to use on another
	DWORD Indices[] =
	{
		// front face
		0, 1, 2,
		0, 2, 3,

		// back face
		4, 6, 5,
		4, 7, 6,

		// left face
		4, 5, 1,
		4, 1, 0,

		// right face
		3, 2, 6,
		3, 6, 7,

		// top face
		1, 5, 6,
		1, 6, 2,

		// bottom face
		4, 0, 3,
		4, 3, 7
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
	VertexBufferDesc.ByteWidth = sizeof(Vertex) * 8;
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

	// Setup Camera
	CameraPosition = XMVectorSet(0.0f, 3.0f, -8.0f, 0.0f);
	CameraTarget = XMVectorSet(0.0f, 0.0f, 0.0f, 0.0f);
	CameraUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	// Create the Viewspace
	CameraView = XMMatrixLookAtLH(CameraPosition, CameraTarget, CameraUp);
	
	// Create projection space
	CameraProjection = XMMatrixPerspectiveFovLH((0.4f * 3.14f), (float)Width / Height, 1.0f, 1000.0f);

	return true;
}

void UpdateScene()
{
	Rot += .0005f;
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
	FLOAT bgColor[4] = { Red, Green, Blue, 1.0f };
	D3D11DeviceContext->ClearRenderTargetView(RenderTargetView, bgColor);

	// Clear the Depth/Stencil View
	D3D11DeviceContext->ClearDepthStencilView(DepthStencilView, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// Create the WVP Matrix
	WVP = Cube1World * CameraView * CameraProjection;
	// Update the Constant Buffer
	// Need to Transpose (swap rows and columns), when sending them to Effect files
	cbPerObj.WVP = XMMatrixTranspose(WVP);
	// Update the effects file constant buffer with ours so they match
	D3D11DeviceContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0);
	// Set the Vertex Shader Constant buffer to ours
	D3D11DeviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);
	// Draw our cube (indexed)
	D3D11DeviceContext->DrawIndexed(36, 0, 0);

	WVP = Cube2World * CameraView * CameraProjection;
	cbPerObj.WVP = XMMatrixTranspose(WVP);
	// Update the effects file constant buffer with ours so they match
	D3D11DeviceContext->UpdateSubresource(cbPerObjectBuffer, 0, NULL, &cbPerObj, 0, 0);
	// Set the Vertex Shader Constant buffer to ours
	D3D11DeviceContext->VSSetConstantBuffers(0, 1, &cbPerObjectBuffer);
	// Draw our cube (indexed)
	D3D11DeviceContext->DrawIndexed(36, 0, 0);


	// Swap the front buffer with the backbuffer
	SwapChain->Present(0, 0);
}