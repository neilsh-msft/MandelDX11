
// Coded by Jan Vlietinck, 18 Oct 2009, V 1.8
//http://users.skynet.be/fquake/

// force win version to Win 7
#define WINVER 0x0601  
#define _WIN32_WINNT 0x0601

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>

#include "resource.h"
#include "stdio.h"
#include "timer.h"
#include "math.h"

#include "CSMandelJulia_vector.fh"
#include "CSMandelJulia_scalar.fh"

#include "CSMandelJulia_vectorFloat.fh"
#include "CSMandelJulia_scalarFloat.fh"
#include "CSMandelJulia_vectorDouble.fh"
#include "CSMandelJulia_scalarDouble1.fh"
#include "CSMandelJulia_scalarDouble2.fh"


//-----------------------------------------------------------------------------
// Global variables
//-----------------------------------------------------------------------------
#define MAX_ITERATIONS 1024
int max_iterations = MAX_ITERATIONS;  // can be doubled with m key

WCHAR message[256];
float timer=0;

bool GPUcanDoDoubles;
bool useDoubles = false;
bool computeScalar = false;
bool scalarDouble1 = true;

Timer time;
int fps=0;
int cycle = -110;  // color cycling, with a,z keys

// center coordiantes and pixel step size, for mandel and julia
double a0[2],b0[2], da[2], db[2];


typedef struct
{
  double a0,b0, da,db;
  double ja0, jb0; // julia set point

  int max_iterations;
  int julia;  // julia or mandelbrot
  unsigned int cycle;
  
} MandelConstantsDoubles;

typedef struct
{
  float a0,b0, da,db;
  float ja0, jb0; // julia set point

  int max_iterations;
  int julia;  // julia or mandelbrot
  unsigned int cycle;
  
} MandelConstantsNoDoubles;


int ROWS, COLS;
#define HCOLS (COLS/2)
#define HROWS (ROWS/2)

int   mouseButtonState = 0;
float mouseDx =0;
float mouseDy =0;


int g_width, g_height; // size of window



#define SAFE_RELEASE(p)      { if(p) { (p)->Release(); (p)=NULL; } }


HINSTANCE                           g_hInst = NULL;
HWND                                g_hWnd = NULL;
D3D_DRIVER_TYPE                     g_driverType = D3D_DRIVER_TYPE_NULL;
ID3D11Device*                       g_pd3dDevice = NULL;
ID3D11DeviceContext*                g_pImmediateContext = NULL ;
D3D_FEATURE_LEVEL					g_featureLevel;
IDXGISwapChain*                     g_pSwapChain = NULL;


bool vsync = false; // redraw synced with screen refresh
bool julia = false; // mandelbrot or julia
bool juliaAnimate = false;
ID3D11ComputeShader*        g_pCSMandelJulia_scalarDouble1;
ID3D11ComputeShader*        g_pCSMandelJulia_scalarDouble2;
ID3D11ComputeShader*        g_pCSMandelJulia_vectorDouble;
ID3D11ComputeShader*        g_pCSMandelJulia_scalarFloat;
ID3D11ComputeShader*        g_pCSMandelJulia_vectorFloat;
ID3D11Buffer*		    				g_pcbFractal;       // constant buffer
ID3D11UnorderedAccessView*  g_pComputeOutput;  // compute output
  



//--------------------------------------------------------------------------------------
// Forward declarations
//--------------------------------------------------------------------------------------
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow );
HRESULT InitDevice();
void CleanupDevice();
LRESULT CALLBACK    WndProc( HWND, UINT, WPARAM, LPARAM );
void Render();


//--------------------------------------------------------------------------------------
// Entry point to the program. Initializes everything and goes into a message processing 
// loop. Idle time is used to render the scene.
//--------------------------------------------------------------------------------------
//***********************************************************************************
int WINAPI wWinMain( HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow )
//***********************************************************************************
{

  COLS = (LONG)::GetSystemMetrics( SM_CXSCREEN );
  ROWS = (LONG)::GetSystemMetrics( SM_CYSCREEN );

  SetCursorPos(HCOLS, HROWS); // set mouse to middle screen


  if( FAILED( InitWindow( hInstance, nCmdShow ) ) )
      return 0;

  if( FAILED( InitDevice() ) )
  {
      CleanupDevice();
      return 0;
  }

  // Main message loop
  MSG msg = {0};
  while( WM_QUIT != msg.message )
  {
      if( PeekMessage( &msg, NULL, 0, 0, PM_REMOVE ) )
      {
          TranslateMessage( &msg );
          DispatchMessage( &msg );
      }
      else
      {
          Render();
      }
  }

  CleanupDevice();

  return ( int )msg.wParam;
}




//--------------------------------------------------------------------------------------
// Register class and create window
//--------------------------------------------------------------------------------------
//***********************************************************************************
HRESULT InitWindow( HINSTANCE hInstance, int nCmdShow )
//***********************************************************************************
{
    // Register class
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof( WNDCLASSEX );
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.cbClsExtra = 0;
    wcex.cbWndExtra = 0;
    wcex.hInstance = hInstance;
    wcex.hIcon = LoadIcon( hInstance, ( LPCTSTR )IDI_MAIN_ICON );
    wcex.hCursor = LoadCursor( NULL, IDC_ARROW );
    wcex.hbrBackground = ( HBRUSH )( COLOR_WINDOW + 1 );
    wcex.lpszMenuName = NULL;
    wcex.lpszClassName = L"MandelWindowClass";
    wcex.hIconSm = LoadIcon( wcex.hInstance, ( LPCTSTR )IDI_MAIN_ICON );
    if( !RegisterClassEx( &wcex ) )
        return E_FAIL;

    // Create window
    g_hInst = hInstance;
    RECT rc = { 0, 0, COLS, ROWS };
    AdjustWindowRect( &rc, WS_OVERLAPPEDWINDOW, FALSE );

    g_hWnd = CreateWindow( L"MandelWindowClass", L"Mandel DX11", WS_OVERLAPPEDWINDOW, 0, 0, rc.right - rc.left, rc.bottom - rc.top, NULL, NULL, hInstance, NULL );


    if( !g_hWnd )
        return E_FAIL;

    ShowWindow( g_hWnd, nCmdShow );

    return S_OK;
}


//***********************************************************************************
void Resize()
//***********************************************************************************
{
  
  if ( g_pd3dDevice == NULL)
    return;

  HRESULT hr = S_OK;

  RECT rc;
  GetClientRect( g_hWnd, &rc );
  UINT width = rc.right - rc.left;
  UINT height = rc.bottom - rc.top;

  g_height = height;
  g_width  = width;


  SAFE_RELEASE(g_pComputeOutput);  // release first else resize problem

  DXGI_SWAP_CHAIN_DESC sd;
  g_pSwapChain->GetDesc(&sd);
  hr = g_pSwapChain->ResizeBuffers(sd.BufferCount, width, height, sd.BufferDesc.Format, 0);

  ID3D11Texture2D* pTexture;
  hr = g_pSwapChain->GetBuffer( 0, __uuidof( ID3D11Texture2D ), ( LPVOID* )&pTexture );

  D3D11_TEXTURE2D_DESC desc = { 0 };
  pTexture->GetDesc(&desc);

  // create shader unordered access view on back buffer for compute shader to write into texture
  hr = g_pd3dDevice->CreateUnorderedAccessView( pTexture, NULL, &g_pComputeOutput );


  pTexture->Release();



  // Setup the viewport
  D3D11_VIEWPORT vp;
  vp.Width = width;
  vp.Height = height;
  vp.MinDepth = 0.0f;
  vp.MaxDepth = 1.0f;
  vp.TopLeftX = 0;
  vp.TopLeftY = 0;
  g_pImmediateContext->RSSetViewports( 1, &vp );

  // mandel
  a0[0] = -0.5f;
  b0[0] = -0;
  da[0] = da[1] =  4.0f/g_width;
  db[0] = db[1] = da[0];//2.0f/g_height;

  // julia
  a0[1] = 0;
  b0[1] = 0;

}



//--------------------------------------------------------------------------------------
// Create Direct3D device and swap chain
//--------------------------------------------------------------------------------------
//***********************************************************************************
HRESULT InitDevice()
//***********************************************************************************
{
    HRESULT hr = S_OK;

    RECT rc;
    GetClientRect( g_hWnd, &rc );
    UINT width = rc.right - rc.left;
    UINT height = rc.bottom - rc.top;

    UINT createDeviceFlags = 0;
#ifdef _DEBUG
    createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

    D3D_DRIVER_TYPE driverTypes[] =
    {
#ifdef WARP
      D3D_DRIVER_TYPE_REFERENCE,
#else
      D3D_DRIVER_TYPE_HARDWARE,        
#endif
    };
    UINT numDriverTypes = sizeof( driverTypes ) / sizeof( driverTypes[0] );

    DXGI_SWAP_CHAIN_DESC sd;
    ZeroMemory( &sd, sizeof( sd ) );
    sd.BufferCount = 1;
    sd.BufferDesc.Width = width;
    sd.BufferDesc.Height = height;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT | DXGI_USAGE_UNORDERED_ACCESS | DXGI_USAGE_SHADER_INPUT;
    sd.OutputWindow = g_hWnd;
    sd.SampleDesc.Count = 1;
    sd.SampleDesc.Quality = 0;
    sd.Windowed = TRUE;
    //sd.Windowed = FALSE;
    //sd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	const D3D_FEATURE_LEVEL FeatureLevels[] = { /* D3D_FEATURE_LEVEL_11_1, */ D3D_FEATURE_LEVEL_11_0,
		D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0 };
/*
	hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, createDeviceFlags, FeatureLevels, _countof(FeatureLevels),
		D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
	if (hr == E_INVALIDARG)
	{
		// DirectX 11.0 Runtime doesn't recognize D3D_FEATURE_LEVEL_11_1 as a valid value
		hr = D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, &FeatureLevels[1], _countof(FeatureLevels) - 1,
			D3D11_SDK_VERSION, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext);
	}

	if (FAILED(hr))
	{
		printf("Failed creating Direct3D 11 device %08X\n", hr);
		return -1;
	}

	// Verify compute shader is supported
	if (g_pd3dDevice->GetFeatureLevel() < D3D_FEATURE_LEVEL_11_0)
	{
		D3D11_FEATURE_DATA_D3D10_X_HARDWARE_OPTIONS hwopts = { 0 };
		(void)g_pd3dDevice->CheckFeatureSupport(D3D11_FEATURE_D3D10_X_HARDWARE_OPTIONS, &hwopts, sizeof(hwopts));
		if (!hwopts.ComputeShaders_Plus_RawAndStructuredBuffers_Via_Shader_4_x)
		{
			g_pd3dDevice->Release();
			printf("DirectCompute is not supported by this device\n");
			return -1;
		}
	}


	IDXGIDevice * pDXGIDevice = nullptr;
	hr = g_pd3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void **)&pDXGIDevice);
	if (FAILED(hr))
		return hr;

	IDXGIAdapter * pDXGIAdapter = nullptr;
	hr = pDXGIDevice->GetAdapter(&pDXGIAdapter);
	if (FAILED(hr))
		return hr;

	IDXGIFactory * pIDXGIFactory = nullptr;
	hr = pDXGIAdapter->GetParent(__uuidof(IDXGIFactory), (void **)&pIDXGIFactory);
	if (FAILED(hr))
		return hr;

	hr = pIDXGIFactory->CreateSwapChain(g_pd3dDevice, &sd, &g_pSwapChain);
	if (FAILED(hr))
		return hr;
*/
/*
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = { 0 };

	swapChainDesc.Width = static_cast<UINT>(m_d3dRenderTargetSize.Width);
	swapChainDesc.Height = static_cast<UINT>(m_d3dRenderTargetSize.Height);
	swapChainDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1; // Don't use multi-sampling.
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_FOREGROUND_LAYER;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;
	swapChainDesc.Scaling = DXGI_SCALING_NONE;

	ComPtr<IDXGISwapChain1> swapChain;
	HRESULT hr = dxgiFactory->CreateSwapChainForCoreWindow(
		m_d3dDevice.Get(),
		reinterpret_cast<IUnknown*>(m_window.Get()),
		&swapChainDesc,
		nullptr,
		&swapChain
	);
*/

    for( UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++ )
    {
        g_driverType = driverTypes[driverTypeIndex];
        hr = D3D11CreateDeviceAndSwapChain( nullptr, g_driverType, NULL, createDeviceFlags, FeatureLevels, _countof(FeatureLevels),
                                            D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice, &g_featureLevel, &g_pImmediateContext ); 
											// win10 doesn't like the swap chain descriptor
        if( SUCCEEDED( hr ) )
            break;
    }
    if( FAILED( hr ) )
        return hr;

    // check if GPU supports doubles
    D3D11_FEATURE_DATA_DOUBLES fdDoubleSupport;
    g_pd3dDevice->CheckFeatureSupport( D3D11_FEATURE_DOUBLES, &fdDoubleSupport, sizeof(fdDoubleSupport) );
    GPUcanDoDoubles = fdDoubleSupport.DoublePrecisionFloatShaderOps;


    //GPUcanDoDoubles = false;

    // Create constant buffer
    D3D11_BUFFER_DESC Desc;
    Desc.Usage = D3D11_USAGE_DYNAMIC;
    Desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
    Desc.MiscFlags = 0;
    Desc.ByteWidth = ((( (GPUcanDoDoubles) ? sizeof(MandelConstantsDoubles) : sizeof(MandelConstantsNoDoubles) ) + 15)/16)*16; // must be multiple of 16 bytes
    g_pd3dDevice->CreateBuffer( &Desc, NULL, &g_pcbFractal);
 


    // Create compute shader
    DWORD dwShaderFlags = D3DCOMPILE_ENABLE_STRICTNESS;
    #if defined( DEBUG ) || defined( _DEBUG )
//      dwShaderFlags |= D3D10_SHADER_DEBUG;
      dwShaderFlags |= D3DCOMPILE_DEBUG;
#endif


    ID3DBlob* pErrorBlob = NULL;
    ID3DBlob* pBlob = NULL;
    TCHAR      message[4096];
	LPCSTR profile = (g_pd3dDevice->GetFeatureLevel() >= D3D_FEATURE_LEVEL_11_0) ? "cs_5_0" : "cs_4_0";

    float dodouble = GPUcanDoDoubles;

    if (GPUcanDoDoubles)
    {
		
	  hr = g_pd3dDevice->CreateComputeShader((DWORD*)g_CSMandelJulia_vectorDouble, ARRAYSIZE(g_CSMandelJulia_vectorDouble), NULL, &g_pCSMandelJulia_vectorDouble);
	  hr = g_pd3dDevice->CreateComputeShader((DWORD*)g_CSMandelJulia_vectorFloat, ARRAYSIZE(g_CSMandelJulia_vectorFloat), NULL, &g_pCSMandelJulia_vectorFloat);
	  hr = g_pd3dDevice->CreateComputeShader((DWORD*)g_CSMandelJulia_scalarFloat, ARRAYSIZE(g_CSMandelJulia_scalarFloat), NULL, &g_pCSMandelJulia_scalarFloat);
	  hr = g_pd3dDevice->CreateComputeShader((DWORD*)g_CSMandelJulia_scalarDouble1, ARRAYSIZE(g_CSMandelJulia_scalarDouble1), NULL, &g_pCSMandelJulia_scalarDouble1);
	  hr = g_pd3dDevice->CreateComputeShader((DWORD*)g_CSMandelJulia_scalarDouble2, ARRAYSIZE(g_CSMandelJulia_scalarDouble2), NULL, &g_pCSMandelJulia_scalarDouble2);

/*
      hr = D3DCompileFromFile( L"mandel.hlsl", NULL, D3D_COMPILE_STANDARD_FILE_INCLUDE, "CSMandelJulia_scalarDouble1", profile, dwShaderFlags, 0, &pBlob, &pErrorBlob);
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_scalarDouble1 );
      SAFE_RELEASE(pBlob);
      SAFE_RELEASE(pErrorBlob);

      hr = D3DCompileFromFile( L"mandel.hlsl", NULL, NULL, "CSMandelJulia_scalarDouble2", "cs_5_0", dwShaderFlags, 0, &pBlob, &pErrorBlob);
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_scalarDouble2 );
      SAFE_RELEASE(pBlob);
      SAFE_RELEASE(pErrorBlob);

      hr = D3DCompileFromFile( L"mandel.hlsl", NULL, NULL, "CSMandelJulia_vectorDouble", "cs_5_0", dwShaderFlags, 0, &pBlob, &pErrorBlob);
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_vectorDouble );
      SAFE_RELEASE(pBlob);
      SAFE_RELEASE(pErrorBlob);

      hr = D3DCompileFromFile( L"mandel.hlsl", NULL, NULL, "CSMandelJulia_scalarFloat", "cs_5_0", dwShaderFlags, 0, &pBlob, &pErrorBlob);
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_scalarFloat );
      SAFE_RELEASE(pBlob);
      SAFE_RELEASE(pErrorBlob);

      hr = D3DCompileFromFile( L"mandel.hlsl", NULL, NULL, "CSMandelJulia_vectorFloat", "cs_5_0", dwShaderFlags, 0, &pBlob, &pErrorBlob);
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_vectorFloat );
      SAFE_RELEASE(pBlob);
      SAFE_RELEASE(pErrorBlob);
*/
    }
    else
    {
      hr = D3DCompileFromFile( L"mandel_float.hlsl", NULL, NULL, "CSMandelJulia_scalar", "cs_5_0", dwShaderFlags, 0, &pBlob, &pErrorBlob);
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_scalarFloat );
      SAFE_RELEASE(pBlob);
      SAFE_RELEASE(pErrorBlob);

      hr = D3DCompileFromFile( L"mandel_float.hlsl", NULL, NULL, "CSMandelJulia_vector", "cs_5_0", dwShaderFlags, 0, &pBlob, &pErrorBlob);
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_vectorFloat );
      SAFE_RELEASE(pBlob);
      SAFE_RELEASE(pErrorBlob);
    }


/*
    if (FAILED(hr))    // create from precompiled shader if failed to compile shader from source
    {
      if (pErrorBlob!=NULL)
      {
        swprintf( message, L"%S", pErrorBlob->GetBufferPointer());
        MessageBox(g_hWnd, message, L"Error", MB_OK);     
      }
      fp = fopen("CSMandelJulia_scalar.hlo","rb");
      if (fp!=NULL)
      {
        r = (int)fread(source, 1, 40000, fp);
        source[r] = 0;
        fclose(fp);
      }
      hr = g_pd3dDevice->CreateComputeShader(  source, r, NULL, &g_pCSMandelJulia_scalar );
    }
    else
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_scalar );


    SAFE_RELEASE(pBlob);
    SAFE_RELEASE(pErrorBlob);

    hr = D3DCompileFromFile( L"mandel.hlsl", NULL, NULL, "CSMandelJulia_vector", "cs_5_0", dwShaderFlags, NULL, NULL, &pBlob, &pErrorBlob, NULL );
    
    if (FAILED(hr))    // create from precompiled shader if failed to compile shader from source
    {
      if (pErrorBlob!=NULL)
      {
        swprintf( message, L"%S", pErrorBlob->GetBufferPointer());
        MessageBox(g_hWnd, message, L"Error", MB_OK);     
      }
      fp = fopen("CSMandelJulia_vector.hlo","rb");
      if (fp!=NULL)
      {
        r = (int)fread(source, 1, 40000, fp);
        source[r] = 0;
        fclose(fp);
      }
      hr = g_pd3dDevice->CreateComputeShader(  source, r, NULL, &g_pCSMandelJulia_vector );
    }
    else
      hr = g_pd3dDevice->CreateComputeShader( ( DWORD* )pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, &g_pCSMandelJulia_vector );

    SAFE_RELEASE(pBlob);
    SAFE_RELEASE(pErrorBlob);
*/



    // text rendering
    // Initialize the font
/*
    HDC hDC = GetDC( NULL );
    int nHeight = -MulDiv( 12, GetDeviceCaps(hDC, LOGPIXELSY), 72 );
    ReleaseDC( NULL, hDC );

    D3DX10CreateFont( g_pd3dDevice, nHeight, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                                OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                                L"Arial", &g_pFont10 );
    D3DX10CreateSprite( g_pd3dDevice, 512, &g_pSprite10 );
*/
    Resize();

    return S_OK;
}



//--------------------------------------------------------------------------------------
// Clean up the objects we've created
//--------------------------------------------------------------------------------------
//***********************************************************************************
void CleanupDevice()
//***********************************************************************************
{
	/*
	  SAFE_RELEASE( g_pFont10 );
	  SAFE_RELEASE( g_pSprite10 );
  */

	SAFE_RELEASE(g_pCSMandelJulia_scalarFloat);
	SAFE_RELEASE(g_pCSMandelJulia_vectorFloat);
	SAFE_RELEASE(g_pCSMandelJulia_scalarDouble1);
	SAFE_RELEASE(g_pCSMandelJulia_scalarDouble2);
	SAFE_RELEASE(g_pCSMandelJulia_vectorDouble);
	SAFE_RELEASE(g_pComputeOutput);
	SAFE_RELEASE(g_pcbFractal);

	if (g_pImmediateContext) g_pImmediateContext->ClearState();

	if (g_pSwapChain)
	{
		g_pSwapChain->SetFullscreenState(false, NULL); // switch back to full screen else not working ok
		g_pSwapChain->Release();
	}

	if (g_pImmediateContext) g_pImmediateContext->Release();
	if (g_pd3dDevice) g_pd3dDevice->Release();
}

//--------------------------------------------------------------------------------------
// Called every time the application receives a message
//--------------------------------------------------------------------------------------
//***********************************************************************************
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
//***********************************************************************************
{
	PAINTSTRUCT ps;
	HDC hdc;

	// Consolidate the mouse button messages and pass them to the right volume renderer
	if (msg == WM_LBUTTONDOWN ||
		msg == WM_LBUTTONUP ||
		msg == WM_LBUTTONDBLCLK ||
		msg == WM_MBUTTONDOWN ||
		msg == WM_MBUTTONUP ||
		msg == WM_MBUTTONDBLCLK ||
		msg == WM_RBUTTONDOWN ||
		msg == WM_RBUTTONUP ||
		msg == WM_RBUTTONDBLCLK ||
		msg == WM_MOUSEWHEEL ||
		msg == WM_MOUSEMOVE)
	{
		int xPos = (short)LOWORD(lParam);
		int yPos = (short)HIWORD(lParam);
		mouseButtonState = LOWORD(wParam);

		// pan with mouse
		if (msg == WM_MOUSEMOVE)
		{
			mouseDx = (float)(xPos - g_width / 2) / (float)min(g_width, g_height) * 2;
			mouseDy = (float)(yPos - g_height / 2) / (float)min(g_width, g_height) * 2;
		}
	}
	else switch (msg)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_CREATE:
		break;

	case WM_KEYDOWN:
	{
		if ('D' == (int)wParam || 'd' == (int)wParam)
		{
			useDoubles = GPUcanDoDoubles;
			computeScalar = true;
		}

		if ('F' == (int)wParam || 'f' == (int)wParam)
		{
			useDoubles = false;
			computeScalar = false;
		}


		if ('E' == (int)wParam || 'e' == (int)wParam)
		{
			scalarDouble1 = !scalarDouble1;
		}


		// Handle key presses
		if ('A' == (int)wParam || 'a' == (int)wParam)
		{
			cycle++;
		}
		if ('Z' == (int)wParam || 'z' == (int)wParam)
		{
			cycle--;
		}
		if( 'B' == (int)wParam || 'b' == (int)wParam)
		{
		  vsync = !vsync;
		}
		if ('V' == (int)wParam || 'v' == (int)wParam)
		{
			computeScalar = false;   // toggle between scalar and vector compute shader with spacebar           
			if (useDoubles)
				computeScalar = true;
		}

		if ('S' == (int)wParam || 's' == (int)wParam)
		{
			computeScalar = true;;   // toggle between scalar and vector compute shader with spacebar           
		}
		if ('M' == (int)wParam || 'm' == (int)wParam)
		{
			// toggle between 1024 and 2048 maximum iterations
			max_iterations = (max_iterations == MAX_ITERATIONS) ? MAX_ITERATIONS * 2 : MAX_ITERATIONS;
		}

		switch (wParam)
		{
		case VK_SHIFT:
			if (julia)
				juliaAnimate = true;
			break;
		case VK_ESCAPE:
			SendMessage(hWnd, WM_CLOSE, 0, 0);
			break;

		case VK_SPACE:
			julia = !julia;   // toggle between mandelbrot and julia
			break;
		}
		break;
	}

	case WM_KEYUP:
	{
		if (wParam == VK_SHIFT && julia)
			juliaAnimate = false;
		break;
	}

	case WM_CLOSE:
	{
		DestroyWindow(hWnd);
		UnregisterClass(L"MandelWindowClass", NULL);
		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	case WM_SIZE:
		if (wParam != SIZE_MINIMIZED)
			Resize();
		break;

	default:
		return DefWindowProc(hWnd, msg, wParam, lParam);
	}

	return 0;
}

//*************************************************************************
void Render()
//*************************************************************************
{
	// time step for zooming / panning depending on rendering frame rate to make interaction speed independent of frame rate
	static float elapsedPrev = 0;
	float t = time.Elapsed();
	float dt = (elapsedPrev == 0) ? 1 : (t - elapsedPrev) / 1000;
	elapsedPrev = t;

	// julia or mandelbrot
	int i = (julia && !juliaAnimate) ? 1 : 0;

	// pan
	if (sqrt(mouseDx*mouseDx + mouseDy*mouseDy) > 0.25f)
	{
		float dx = (int)(mouseDx*4.0f);  // truncate to integer
		float dy = (int)(mouseDy*4.0f);

		dx = dx * ceil(dt * 60);  // truncate to integer
		dy = dy * ceil(dt * 60);

		//  pan in pixel steps, so not to have jitter
		a0[i] += da[i] * dx;
		b0[i] += db[i] * dy;

		a0[i] = min(2, max(-2, a0[i]));  // don't move set out of view
		b0[i] = min(2, max(-2, b0[i]));
	}

	// zoom
	if (mouseButtonState & MK_LBUTTON)
	{
		da[i] = da[i] * (1.0f - dt);
		db[i] = db[i] * (1.0f - dt);
	}
	if (mouseButtonState & MK_RBUTTON)
	{
		da[i] = da[i] / (1.0f - dt);
		db[i] = db[i] / (1.0f - dt);
	}

	i = (julia) ? 1 : 0;

	// Fill constant buffer
	D3D11_MAPPED_SUBRESOURCE msr;
	g_pImmediateContext->Map(g_pcbFractal, 0, D3D11_MAP_WRITE_DISCARD, 0, &msr);
	if (GPUcanDoDoubles)
	{
		MandelConstantsDoubles mc;
		mc.a0 = a0[i] - da[i] * g_width / 2;
		mc.b0 = b0[i] - db[i] * g_height / 2;
		mc.da = da[i];
		mc.db = db[i];
		mc.ja0 = a0[0];      // julia point
		mc.jb0 = b0[0];
		mc.cycle = cycle;
		mc.julia = julia;
		mc.max_iterations = max_iterations;
		*(MandelConstantsDoubles *)msr.pData = mc;
	}
	else
	{
		MandelConstantsNoDoubles mc;
		mc.a0 = (float)(a0[i] - da[i] * g_width / 2);
		mc.b0 = (float)(b0[i] - db[i] * g_height / 2);
		mc.da = (float)da[i];
		mc.db = (float)db[i];
		mc.ja0 = (float)a0[0];      // julia point
		mc.jb0 = (float)b0[0];
		mc.cycle = cycle;
		mc.julia = julia;
		mc.max_iterations = max_iterations;
		*(MandelConstantsNoDoubles *)msr.pData = mc;
	}
	g_pImmediateContext->Unmap(g_pcbFractal, 0);

	// Set compute shader
	if (useDoubles && GPUcanDoDoubles)
	{
		if (computeScalar)
		{
			if (scalarDouble1)
				g_pImmediateContext->CSSetShader(g_pCSMandelJulia_scalarDouble1, NULL, 0);
			else
				g_pImmediateContext->CSSetShader(g_pCSMandelJulia_scalarDouble2, NULL, 0);  // Is faster, at least on Nvidia GPUs, results in buggy and slow execution on ATI
		}
		else
			g_pImmediateContext->CSSetShader(g_pCSMandelJulia_vectorDouble, NULL, 0);

	}
	else
	{
		if (computeScalar)
			g_pImmediateContext->CSSetShader(g_pCSMandelJulia_scalarFloat, NULL, 0);
		else
			g_pImmediateContext->CSSetShader(g_pCSMandelJulia_vectorFloat, NULL, 0);
	}

	// For CS output
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc;
	g_pComputeOutput->GetDesc(&desc);


	ID3D11UnorderedAccessView* aUAViews[1] = { g_pComputeOutput };
	g_pImmediateContext->CSSetUnorderedAccessViews(0, 1, aUAViews, (UINT*)(&aUAViews));

	// For CS constant buffer
	ID3D11Buffer* ppCB[1] = { g_pcbFractal };
	g_pImmediateContext->CSSetConstantBuffers(0, 1, ppCB);

	// Run the CS
	if (computeScalar)
		g_pImmediateContext->Dispatch((g_width + 15) / 16, (g_height + 15) / 16, 1);
	else
		g_pImmediateContext->Dispatch((g_width + 31) / 32, (g_height + 31) / 32, 1);

	// Present our back buffer to our front buffer
	g_pSwapChain->Present(vsync ? 1 : 0, 0);

	if ((t = time.Elapsed() / 1000) - timer > 1) // every second
	{
		float td = t - timer; // real time interval a bit more than a second
		timer = t;
		swprintf(message, L"Mandel V1.8 %s %s FPS %.2f     Keys:  D->doubles F->floats V->vector S->scalar  E->doubles1/2  M->1024/2048 max iterations, A/Z->cycle colors", (computeScalar) ? L"Scalar" : L"Vector", (useDoubles) ? ((scalarDouble1) ? L"(doubles1)" : L"(doubles2)") : L"(floats)", (float)fps / td);
		SetWindowText(g_hWnd, message);
		fps = 0;
	}
	fps++;
}
