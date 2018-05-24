#include <Windows.h>

#include "../Game/Common.h"
#include "../Game/imgui.h"

#include <GL/glew.h>
#include <GL/wglew.h>


#include <glm\fwd.hpp>
#include <glm\ext.hpp>


#pragma comment (lib, "opengl32.lib")
#pragma comment (lib, "Winmm.lib")

#define GLEW_STATIC
#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION

#include "stb\stb_image.h"

static bool windowActive = true;
static size_t screenWidth = 1920, screenHeight = 1080;
bool keyboard[256] = {};

struct VertexTN
{
	glm::vec3 p, n;
	glm::vec4 c;
	glm::vec2 t;
};

struct VAO
{
	GLuint vao;
	GLuint elementArrayBufferObject, vertexBufferObject;
	int numIndices;
};

struct InstanceData
{
	glm::mat4 modelMatrix;
	glm::vec4 colorModifier;
};

struct RendererData
{
	// pack all render-related data into this struct
	GLint myShader;
	VAO quadVAO;
	GLuint instanceDataBuffer;

	// map of all textures available
	GLuint textures[(int)Game::RenderCommands::TextureNames::COUNT];

	//imgui
	static constexpr int BuffersSize = 0x10000;


	GLuint vertexArray;
	GLuint vao;
	GLuint uniform;
	GLuint program;
};

struct FullFile
{
	uint8_t* data;
	size_t size;
	uint64_t lastWriteTime;
};

static HGLRC s_OpenGLRenderingContext = nullptr;
static HDC s_WindowHandleToDeviceContext;



inline FullFile ReadFullFile(const wchar_t* path)
{
	FullFile result;

	HANDLE hFile;
	OVERLAPPED ol = {};

	hFile = CreateFileW(
		path,               // file to open
		GENERIC_READ,          // open for reading
		FILE_SHARE_READ,       // share for reading
		NULL,                  // default security
		OPEN_EXISTING,         // existing file only
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, // normal file
		NULL);                 // no attr. template

	if (hFile != INVALID_HANDLE_VALUE)
	{
		DWORD fileSizeHigh;
		result.size = GetFileSize(hFile, &fileSizeHigh);
		FILETIME lastWriteTime;
		GetFileTime(hFile, NULL, NULL, &lastWriteTime);
		ULARGE_INTEGER li;
		li.HighPart = lastWriteTime.dwHighDateTime;
		li.LowPart = lastWriteTime.dwLowDateTime;
		result.lastWriteTime = li.QuadPart;

		result.data = new uint8_t[result.size + 1];

		if (ReadFileEx(hFile, result.data, (DWORD)result.size, &ol, nullptr))
		{
			result.data[result.size] = 0; // set the last byte to 0 to help strings
		}
		else
		{
			result.data = nullptr;
			result.size = 0;
		}
		CloseHandle(hFile);
	}
	else
	{
		result.data = nullptr;
		result.size = 0;
	}
	return result;
}

FullFile verShad = ReadFullFile(L"Simple.vs");

static const char* vertexShader = (char*)verShad.data;

FullFile pixShad = ReadFullFile(L"Simple.fs");

static const char* pixelShader = (char*)pixShad.data;


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


bool CompileShader(GLint &shaderId_, const char* shaderCode, GLenum shaderType) {
	shaderId_ = glCreateShader(shaderType);
	glShaderSource(shaderId_, 1, (const GLchar *const*)&shaderCode, nullptr);
	glCompileShader(shaderId_);

	GLint success = 0;
	glGetShaderiv(shaderId_, GL_COMPILE_STATUS, &success);

	{
		GLint maxLength = 0;
		glGetShaderiv(shaderId_, GL_INFO_LOG_LENGTH, &maxLength);

		if (maxLength > 0) {
			//The maxLength includes the NULL carachter
			GLchar *infoLog = new GLchar(maxLength);
			glGetShaderInfoLog(shaderId_, maxLength, &maxLength, &infoLog[0]);

			OutputDebugStringA(infoLog);
			OutputDebugStringA("\n");

			delete[] infoLog;
		}
	}
	return(success != GL_FALSE);
}

bool LinkShaders(GLint &programId_, GLint &vs, GLint &ps) {

	programId_ = glCreateProgram();

	glAttachShader(programId_, vs);
	glAttachShader(programId_, ps);
	glLinkProgram(programId_);

	GLint success = 0;
	glGetProgramiv(programId_, GL_LINK_STATUS, &success);
	{
		GLint maxLenght = 0;
		glGetProgramiv(programId_, GL_INFO_LOG_LENGTH, &maxLenght);

		if (maxLenght > 0) {
			//The maxLenght includes the NULL character
			GLchar *infoLog = new GLchar[maxLenght];
			glGetProgramInfoLog(programId_, maxLenght, &maxLenght, &infoLog[0]);

			OutputDebugStringA(infoLog);
			OutputDebugStringA("\n");

			delete[] infoLog;
		}
	}
	return (success != GL_FALSE);
}

inline VAO CreateVertexArrayObject(const VertexTN* vertexs, int numVertexs, const uint16_t * indices, int numIndices)
{
	VAO vao = {};
	vao.numIndices = numIndices;


	glGenVertexArrays(1, &vao.vao);
	glGenBuffers(1, &vao.elementArrayBufferObject);
	glGenBuffers(1, &vao.vertexBufferObject);

	glBindVertexArray(vao.vao);

	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vao.elementArrayBufferObject); // create an element array buffer (index buffer)
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(uint16_t) * numIndices, indices, GL_STATIC_DRAW); // fill buffer data
																								   // size of data				ptr to data    usage

	glBindBuffer(GL_ARRAY_BUFFER, vao.vertexBufferObject); // create an array buffer (vertex buffer)
	glBufferData(GL_ARRAY_BUFFER, sizeof(VertexTN) * numVertexs, vertexs, GL_STATIC_DRAW); // fill buffer data


	glVertexAttribPointer(0, // Vertex Attrib Index
		3, GL_FLOAT, // 3 floats
		GL_FALSE, // no normalization
		sizeof(VertexTN), // offset from a vertex to the next
		(GLvoid*)offsetof(VertexTN, p) // offset from the start of the buffer to the first vertex
	); // positions
	glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(VertexTN), (GLvoid*)offsetof(VertexTN, c)); // colors
	glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(VertexTN), (GLvoid*)offsetof(VertexTN, t)); // textures
	glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(VertexTN), (GLvoid*)offsetof(VertexTN, n)); // normals

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	// reset default opengl state
	glBindVertexArray(0);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glDisableVertexAttribArray(0);
	glDisableVertexAttribArray(1);
	glDisableVertexAttribArray(2);

	return vao;
}

//Texture Loader
GLuint LoadTexture(const char* path)
{
	GLuint result;
	int x, y, comp;
	unsigned char *data = stbi_load(path, &x, &y, &comp, 4);


	glGenTextures(1, &result);
	glBindTexture(GL_TEXTURE_2D, result);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_2D, GL_GENERATE_MIPMAP, GL_TRUE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_SRGB8_ALPHA8, x, y, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

	return result;
}

void init(Game::Input &inputData, Game::GameData *&gameData, Game::RenderCommands &renderCommands, RendererData &rendererData, LARGE_INTEGER &l_LastFrameTime)
{
	//WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB
	//Init Time
	QueryPerformanceCounter(&l_LastFrameTime);

	//Init gameData
	renderCommands.orthoWidth = screenWidth;
	renderCommands.orthoHeight = screenHeight;
	inputData.windowHalfSize = { screenWidth / 2.0, screenHeight / 2.0 };

	//Init textures
	rendererData.textures[(int)Game::RenderCommands::TextureNames::BALLS] = LoadTexture("ball.png");
	rendererData.textures[(int)Game::RenderCommands::TextureNames::PLAYER] = LoadTexture("player.png");

	//Init Render Data
	GLint vs = 0, ps = 0;
	if (CompileShader(vs, vertexShader, GL_VERTEX_SHADER) && CompileShader(ps, pixelShader, GL_FRAGMENT_SHADER) && LinkShaders(rendererData.myShader, vs, ps))
	{

		glGenBuffers(1, &rendererData.instanceDataBuffer); // creates a buffer. The buffer id is written to the variable "instanceDataBuffer"
		glBindBuffer(GL_UNIFORM_BUFFER, rendererData.instanceDataBuffer); // sets "instanceDataBuffer" as the current buffer
		glBufferData(  // sets the buffer content
			GL_UNIFORM_BUFFER,		// type of buffer
			sizeof(InstanceData),	// size of the buffer content
			nullptr,				// content of the buffer
			GL_DYNAMIC_DRAW);		// usage of the buffer. DYNAMIC -> will change frequently. DRAW -> from CPU to GPU

									// we initialize the buffer without content so we can later call "glBufferSubData". Here we are only reserving the size.

		glBindBuffer(GL_UNIFORM_BUFFER, 0); // set no buffer as the current buffer

		VertexTN vtxs[4] = {
			{ { -0.5f, -0.5f, 0 },{ 0, 0, 1 },{ 1, 1, 1, 1 },{ 0,0 } },
		{ { +0.5f, -0.5f, 0 },{ 0, 0, 1 },{ 1, 1, 1, 1 },{ 1,0 } },
		{ { -0.5f, +0.5f, 0 },{ 0, 0, 1 },{ 1, 1, 1, 1 },{ 0,1 } },
		{ { +0.5f, +0.5f, 0 },{ 0, 0, 1 },{ 1, 1, 1, 1 },{ 1,1 } },
		};

		uint16_t idxs[6] = {
			0, 1, 3,
			0, 3, 2
		};

		rendererData.quadVAO = CreateVertexArrayObject(vtxs, 4, idxs, 6);
	}

	if (vs > 0)
		glDeleteShader(vs);
	if (ps > 0)
		glDeleteShader(ps);

}


void render(RendererData &rendererData, Game::RenderCommands &renderCommands) {
	glm::mat4 projection = glm::ortho(-(screenWidth / 2.0f), (screenWidth / 2.0f), -(screenHeight / 2.0f), (screenHeight / 2.0f), -5.0f, 5.0f);
	// preparation:
	glClearColor(0, 0.1, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT);

	glUseProgram(rendererData.myShader);
	glActiveTexture(GL_TEXTURE0 + 0);
	// render all sprites
	for (auto sprite : renderCommands.sprites)
	{
		glBindTexture(GL_TEXTURE_2D, rendererData.textures[(int)sprite.texture]); // get the right texture

																				  // create the model matrix, with a scale and a translation.
		glm::mat4 model = glm::scale(glm::translate(glm::mat4(1.0f), glm::vec3(sprite.position, 0)), glm::vec3(sprite.size, 0));
		// you may add a rotation, between the scale and the translation. rotate arround the "z" axis.

		// the transform is the addition of the model transformation and the projection
		InstanceData instanceData = { projection * model , glm::vec4(1,1,1,1) };

		glEnable(GL_BLEND);
		glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

		glBindBuffer(GL_UNIFORM_BUFFER, rendererData.instanceDataBuffer);
		glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(InstanceData), (GLvoid*)&instanceData);

		glBindBufferBase(GL_UNIFORM_BUFFER, 0, rendererData.instanceDataBuffer);

		glBindVertexArray(rendererData.quadVAO.vao);
		glDrawElements(GL_TRIANGLES, rendererData.quadVAO.numIndices, GL_UNSIGNED_SHORT, 0);
	}

	SwapBuffers(s_WindowHandleToDeviceContext);
}


void initIMGUI(RendererData &renderer) {
	//uniform buffer
	glGenBuffers(1, &renderer.uniform);

	glBindBuffer(GL_UNIFORM_BUFFER, renderer.uniform);
	glBufferData(GL_UNIFORM_BUFFER, sizeof(glm::vec2), nullptr, GL_DYNAMIC_DRAW);

	//imgui texture

	glGenTextures((int)Game::RenderCommands::TextureNames::COUNT, renderer.textures);

	// TODO reload imgui
	uint8_t* pixels;
	int width, height;
	ImGui::GetIO().Fonts[0].GetTexDataAsRGBA32(&pixels, &width, &height, nullptr);

	glBindTexture(GL_TEXTURE_2D, renderer.textures[(int)Game::RenderCommands::TextureNames::IMGUI]);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

	GLenum formats[4] = { GL_RED, GL_RG, GL_RGB, GL_RGBA };

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
		(GLsizei)width, (GLsizei)height,
		0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);

	//imgui vertex buffer

	glGenVertexArrays(1, &renderer.vao);
	glGenBuffers(GL_ARRAY_BUFFER, &renderer.vertexArray);
	
	

	glBindBuffer(GL_ARRAY_BUFFER, renderer.vertexArray);
	glBufferData(GL_ARRAY_BUFFER,
		sizeof(ImDrawVert) * RendererData::BuffersSize,
		nullptr, GL_DYNAMIC_DRAW);

	glBindVertexArray(renderer.vao);

	glVertexAttribPointer(0, 2, GL_FLOAT, false,
		sizeof(ImDrawVert),
		(GLvoid*)offsetof(ImDrawVert, pos));
	glVertexAttribPointer(1, 2, GL_FLOAT, false,
		sizeof(ImDrawVert),
		(GLvoid*)offsetof(ImDrawVert, uv));
	glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, true,
		sizeof(ImDrawVert),
		(GLvoid*)offsetof(ImDrawVert, col));

	glEnableVertexAttribArray(0);
	glEnableVertexAttribArray(1);
	glEnableVertexAttribArray(2);

	//mapa tecles imgui
	//ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.KeyMap[ImGuiKey_Tab] = VK_TAB;                     // Keyboard mapping. ImGui will use those indices to peek into the io.KeyDown[] array.
	io.KeyMap[ImGuiKey_LeftArrow] = VK_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = VK_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = VK_UP;
	io.KeyMap[ImGuiKey_DownArrow] = VK_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = VK_PRIOR;
	io.KeyMap[ImGuiKey_PageDown] = VK_NEXT;
	io.KeyMap[ImGuiKey_Home] = VK_HOME;
	io.KeyMap[ImGuiKey_End] = VK_END;
	io.KeyMap[ImGuiKey_Delete] = VK_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = VK_BACK;
	io.KeyMap[ImGuiKey_Enter] = VK_RETURN;
	io.KeyMap[ImGuiKey_Escape] = VK_ESCAPE;
	io.KeyMap[ImGuiKey_A] = 'A';
	io.KeyMap[ImGuiKey_C] = 'C';
	io.KeyMap[ImGuiKey_V] = 'V';
	io.KeyMap[ImGuiKey_X] = 'X';
	io.KeyMap[ImGuiKey_Y] = 'Y';
	io.KeyMap[ImGuiKey_Z] = 'Z';
}


inline void RenderDearImgui(const RendererData &renderer)
{
	ImDrawData* draw_data = ImGui::GetDrawData();

	// Avoid rendering when minimized, scale coordinates for retina displays (screen coordinates != framebuffer coordinates)
	ImGuiIO& io = ImGui::GetIO();
	int fb_width = (int)(io.DisplaySize.x * io.DisplayFramebufferScale.x);
	int fb_height = (int)(io.DisplaySize.y * io.DisplayFramebufferScale.y);
	if (fb_width == 0 || fb_height == 0)
		return;
	draw_data->ScaleClipRects(io.DisplayFramebufferScale);

	// We are using the OpenGL fixed pipeline to make the example code simpler to read!
	// Setup render state: alpha-blending enabled, no face culling, no depth testing, scissor enabled, vertex/texcoord/color pointers.
	GLint last_texture; glGetIntegerv(GL_TEXTURE_BINDING_2D, &last_texture);
	GLint last_viewport[4]; glGetIntegerv(GL_VIEWPORT, last_viewport);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_CULL_FACE);
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_SCISSOR_TEST);

	GLuint program = renderer.program;
	glUseProgram(program);
	glBindBufferBase(GL_UNIFORM_BUFFER, 0, renderer.uniform);
	//glUseProgram(0); // You may want this if using this code in an OpenGL 3+ context


	// Setup viewport, orthographic projection matrix
	glViewport(0, 0, (GLsizei)fb_width, (GLsizei)fb_height);

	glBindBuffer(GL_UNIFORM_BUFFER, renderer.uniform);
	glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(glm::vec2), (GLvoid*)&io.DisplaySize);
	glBindBuffer(GL_UNIFORM_BUFFER, 0);


	auto texture = renderer.textures[(int)Game::RenderCommands::TextureNames::IMGUI];


	glActiveTexture(GL_TEXTURE0);
	glBindVertexArray(renderer.vao);
	glBindBuffer(GL_ARRAY_BUFFER, renderer.vertexArray);

	// Render command lists
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		const ImDrawVert* vtx_buffer = &cmd_list->VtxBuffer.front();
		const ImDrawIdx* idx_buffer = &cmd_list->IdxBuffer.front();

		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(ImDrawVert) * cmd_list->VtxBuffer.size(), (GLvoid*)vtx_buffer);

		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.size(); cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback)
			{
				pcmd->UserCallback(cmd_list, pcmd);
			}
			else
			{
				if (pcmd->TextureId == nullptr)
				{
					glBindTexture(GL_TEXTURE_2D, texture);
				}
				else
				{
					glBindTexture(GL_TEXTURE_2D, (GLuint)(intptr_t)pcmd->TextureId);
				}
				glScissor((int)pcmd->ClipRect.x, (int)(fb_height - pcmd->ClipRect.w), (int)(pcmd->ClipRect.z - pcmd->ClipRect.x), (int)(pcmd->ClipRect.w - pcmd->ClipRect.y));
				static_assert(sizeof(ImDrawIdx) == 2 || sizeof(ImDrawIdx) == 4, "indexes are 2 or 4 bytes long");
				glDrawElements(GL_TRIANGLES, (GLsizei)pcmd->ElemCount, sizeof(ImDrawIdx) == 2 ? GL_UNSIGNED_SHORT : GL_UNSIGNED_INT, idx_buffer);
			}
			idx_buffer += pcmd->ElemCount;
		}
	}

	glBindVertexArray(0);
	glBindBuffer(GL_ARRAY_BUFFER, 0);

	// Restore modified state
	glBindTexture(GL_TEXTURE_2D, (GLuint)last_texture);
	glDisable(GL_SCISSOR_TEST);
	glViewport(last_viewport[0], last_viewport[1], (GLsizei)last_viewport[2], (GLsizei)last_viewport[3]);
}

void manageInput(bool &quit) {
	MSG msg = {};
	while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
	{
		bool processed = false;
		if (msg.message == WM_QUIT)
		{
			quit = true;
			processed = true;
		}
		else if (msg.message == WM_KEYDOWN)
		{
			keyboard[msg.wParam & 255] = true;
			processed = true;
			if (msg.wParam == VK_ESCAPE)
			{
				PostQuitMessage(0);
			}
		}
		else if (msg.message == WM_KEYUP)
		{
			keyboard[msg.wParam & 255] = false;
			processed = true;
		}

		if (!processed)
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

int __stdcall WinMain(__in HINSTANCE hInstance, __in_opt HINSTANCE hPrevInstance, __in_opt LPSTR lpCmdLine, __in int nShowCmd) {
	
	// load window stuff
	MSG msg = { 0 };
	WNDCLASS wc = { 0 };
	wc.lpfnWndProc = WndProc;
	wc.hInstance = hInstance;
	wc.hbrBackground = (HBRUSH)COLOR_BACKGROUND;
	wc.lpszClassName = L"Billar";
	wc.style = CS_OWNDC;
	if (!RegisterClass(&wc))
		return 1;
	HWND hWnd = CreateWindowW(wc.lpszClassName, L"Billar", WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, screenWidth, screenHeight, 0, 0, hInstance, 0);
	
	//Time
	LARGE_INTEGER l_LastFrameTime;
	int64_t l_PerfCountFrequency;


	Game::GameData *gameData = Game::CreateGameData();
	Game::Input input = {};
	Game::RenderCommands renderCommands = Game::Update(input, *gameData);
	
	RendererData rendererData;
	rendererData.instanceDataBuffer = 0;
	rendererData.myShader = 0;
	rendererData.vao = 0;
	rendererData.vertexArray = 0;
	

	init(input, gameData, renderCommands, rendererData, l_LastFrameTime);
	ImGui::CreateContext();
	initIMGUI(rendererData);
	bool quit = false;
	
	do
	{
		ImGui::NewFrame();
		manageInput(quit);

		if (keyboard['W'])
		{
			input.direction.y = 10;
		}
		if (keyboard['S'])
		{
			input.direction.y = -10;
		}
		if (keyboard['A'])
		{
			input.direction.x = -10;
		}
		if (keyboard['D'])
		{
			input.direction.x = 10;
		}
		if (keyboard[VK_RETURN]) {
			input.direction = { 100, 100 };
		}

		renderCommands = Game::Update(input, *gameData);

		if (s_OpenGLRenderingContext != nullptr) {
			render(rendererData, renderCommands);
			ImGui::Render();
			RenderDearImgui(rendererData);
		}


		LARGE_INTEGER PerfCountFrequencyResult;
		QueryPerformanceFrequency(&PerfCountFrequencyResult);
		l_PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

		int64_t l_TicksPerFrame = l_PerfCountFrequency / 60;

		LARGE_INTEGER l_CurrentTime;
		QueryPerformanceCounter(&l_CurrentTime);
		//float Result = ((float)(l_CurrentTime.QuadPart - l_LastFrameTime.QuadPart) / (float)l_PerfCountFrequency);

		if (l_LastFrameTime.QuadPart + l_TicksPerFrame > l_CurrentTime.QuadPart)
		{
			int64_t ticksToSleep = l_LastFrameTime.QuadPart + l_TicksPerFrame - l_CurrentTime.QuadPart;
			int64_t msToSleep = 1000 * ticksToSleep / l_PerfCountFrequency;
			if (msToSleep > 0)
			{
				Sleep((DWORD)msToSleep);
			}
			continue;
		}
		while (l_LastFrameTime.QuadPart + l_TicksPerFrame <= l_CurrentTime.QuadPart)
		{
			l_LastFrameTime.QuadPart += l_TicksPerFrame;
		}

		input.dt = (double)l_TicksPerFrame / (double)l_PerfCountFrequency;
	} while (!quit);

	Game::DestroyGameData(gameData);

	OutputDebugStringW(L"HelloWorld");

	return 0;
}