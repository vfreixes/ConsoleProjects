#include <Windows.h>

#include "../Game/Common.h"

#include <GL/glew.h>
#include <GL/wglew.h>

#include <glm\glm.hpp>
#include <glm\fwd.hpp>
#include <glm\ext.hpp>


#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "Winmm.lib")

static HGLRC s_OpenGLRenderingContext = nullptr;
static HDC s_WindowHandleToDeviceContext;

static bool windowActive = true;
static size_t screenWidth = 1280, screenHeight = 720;


inline void GLAPIENTRY openGLDebugCallback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar* message, const void* userParam)
{
	OutputDebugStringA("openGL Debug Callback : [");
	switch (type)
	{
	case GL_DEBUG_TYPE_ERROR:
		OutputDebugStringA("ERROR");
		break;
	case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
		OutputDebugStringA("DEPRECATED_BEHAVIOR");
		break;
	case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
		OutputDebugStringA("UNDEFINED_BEHAVIOR");
		break;
	case GL_DEBUG_TYPE_PORTABILITY:
		OutputDebugStringA("PORTABILITY");
		break;
	case GL_DEBUG_TYPE_PERFORMANCE:
		OutputDebugStringA("PERFORMANCE");
		break;
	case GL_DEBUG_TYPE_OTHER:
		OutputDebugStringA("OTHER");
		break;
	default:
		OutputDebugStringA("????");
		break;
	}
	OutputDebugStringA(":");
	switch (severity)
	{
	case GL_DEBUG_SEVERITY_LOW:
		OutputDebugStringA("LOW");
		break;
	case GL_DEBUG_SEVERITY_MEDIUM:
		OutputDebugStringA("MEDIUM");
		break;
	case GL_DEBUG_SEVERITY_HIGH:
		OutputDebugStringA("HIGH");
		break;
	case GL_DEBUG_SEVERITY_NOTIFICATION:
		OutputDebugStringA("NOTIFICATION");
		break;
	default:
		OutputDebugStringA("????");
		break;
	}
	OutputDebugStringA(":");
	char buffer[512];
	sprintf_s(buffer, "%d", id);
	OutputDebugStringA(buffer);
	OutputDebugStringA("] ");
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_CREATE:
	{
		s_WindowHandleToDeviceContext = GetDC(hWnd);

		PIXELFORMATDESCRIPTOR pfd =
		{
			sizeof(PIXELFORMATDESCRIPTOR),
			1,
			PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,    //Flags
			PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
			32,                        //Colordepth of the framebuffer.
			0, 0, 0, 0, 0, 0,
			0,
			0,
			0,
			0, 0, 0, 0,
			24,                        //Number of bits for the depthbuffer
			8,                        //Number of bits for the stencilbuffer
			0,                        //Number of Aux buffers in the framebuffer.
			PFD_MAIN_PLANE,
			0,
			0, 0, 0
		};


		int letWindowsChooseThisPixelFormat;
		letWindowsChooseThisPixelFormat = ChoosePixelFormat(s_WindowHandleToDeviceContext, &pfd);
		SetPixelFormat(s_WindowHandleToDeviceContext, letWindowsChooseThisPixelFormat, &pfd);

		HGLRC tmpContext = wglCreateContext(s_WindowHandleToDeviceContext);
		wglMakeCurrent(s_WindowHandleToDeviceContext, tmpContext);

		// init glew
		GLenum err = glewInit();
		if (err != GLEW_OK)
		{
			assert(false); // TODO
		}

		int attribs[] =
		{
			WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
			WGL_CONTEXT_MINOR_VERSION_ARB, 1,
			WGL_CONTEXT_FLAGS_ARB,
			0 //WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
#if _DEBUG
			| WGL_CONTEXT_DEBUG_BIT_ARB
#endif
			, 0
		};

		s_OpenGLRenderingContext = wglCreateContextAttribsARB(s_WindowHandleToDeviceContext, 0, attribs);

		wglMakeCurrent(NULL, NULL);
		wglDeleteContext(tmpContext);

		wglMakeCurrent(s_WindowHandleToDeviceContext, s_OpenGLRenderingContext);

		if (GLEW_ARB_debug_output)
		{
			glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
			glDebugMessageCallbackARB(openGLDebugCallback, nullptr);
			GLuint unusedIds = 0;
			glDebugMessageControl(GL_DONT_CARE,
				GL_DONT_CARE,
				GL_DONT_CARE,
				0,
				&unusedIds,
				true);
		}

		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
		glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

#ifndef _DEBUG
		ToggleFullscreen(hWnd);
#endif

	}
	break;
	case WM_DESTROY:
		wglDeleteContext(s_OpenGLRenderingContext);
		s_OpenGLRenderingContext = nullptr;
		PostQuitMessage(0);
		break;
	case WM_KEYDOWN:
	{
		switch (wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			return 0;
		}
		break;
	}
	case WM_SIZE:
	{
		screenWidth = LOWORD(lParam);  // Macro to get the low-order word.
		screenHeight = HIWORD(lParam); // Macro to get the high-order word.
		break;
	}
	case WM_ACTIVATE:
	{
		windowActive = wParam != WA_INACTIVE;
		break;
	}
	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

int __stdcall WinMain(__in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in_opt LPSTR lpCmdLine, __in int nShowCmd) {
	Game::GameData *gameData = Game::CreateGameData();
	// load window stuff
	MSG msg = { 0 };
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
	wc.lpszClassName = L"Hello World";
	wc.style = CS_OWNDC;
	if (!RegisterClass(&wc))
		return 1;
	HWND hWnd = CreateWindowW(wc.lpszClassName, L"Le GamuX", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, screenWidth, screenHeight, 0, 0, hInstance, 0);

	Game::Input input = {};
	Game::RenderCommands renderCommands = Game::Update(input, *gameData);
	bool quit = false;
	do
	{
		MSG msg = {};
		while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				quit = true;
			}
			else
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		glClearColor(0, 1, 1, 1);
		glClear(GL_COLOR_BUFFER_BIT);

		SwapBuffers(s_WindowHandleToDeviceContext);
	} while (!quit);

	Game::DestroyGameData(gameData);

	OutputDebugStringW(L"HelloWorld");

	return 0;
}