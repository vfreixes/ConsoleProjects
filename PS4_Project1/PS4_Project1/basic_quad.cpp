/* SIE CONFIDENTIAL
PlayStation(R)4 Programmer Tool Runtime Library Release 05.008.001
* Copyright (C) 2015 Sony Interactive Entertainment Inc.
* All Rights Reserved.
*/

///game update liniea 630

#include <stdio.h>
#include <stdlib.h>
#include <scebase.h>
#include <kernel.h>
#include <gnmx.h>
#include <video_out.h>
#include <toolkit/toolkit.h>
#include "../common/allocator.h"
#include "../common/shader_loader.h"

#include "std_cbuffer.h"

#include "../Game/Common.h"
#include <pad.h>

#define STBI_ONLY_PNG
#define STB_IMAGE_IMPLEMENTATION
#include <stb\stb_image.h>


using namespace sce;
using namespace sce::Gnmx;

size_t sceLibcHeapSize = 64 * 1024 * 1024;

bool readRawTextureData(const char *path, void *address, size_t size)
{
	bool success = false;

	FILE *fp = fopen(path, "rb");
	if( fp != NULL )
	{
		success = readFileContents(address, size, fp);
		fclose(fp);
	}

	return success;
}

int main(int argc, const char *argv[])
{
	static  uint32_t kDisplayBufferWidth			= 1920;
	static uint32_t kDisplayBufferHeight			= 1080;
	static const uint32_t kDisplayBufferCount			= 3;
	static const uint32_t kRenderContextCount			= 2;
	static const Gnm::ZFormat kZFormat					= Gnm::kZFormat32Float;
	static const Gnm::StencilFormat kStencilFormat		= Gnm::kStencil8;
	static const bool kHtileEnabled						= true;
	static const Vector4 kClearColor					= Vector4(0.5f, 0.5f, 0.5f, 1);
	static const size_t kOnionMemorySize				= 16 * 1024 * 1024;
	static const size_t kGarlicMemorySize				= 64 * 4 * 1024 * 1024;

#if SCE_GNMX_ENABLE_GFX_LCUE
	static const uint32_t kNumLcueBatches				= 100;
	static const size_t kDcbSizeInBytes					= 2 * 1024 * 1024;
#else
	static const uint32_t kCueRingEntries				= 64;
	static const size_t kDcbSizeInBytes					= 2 * 1024 * 1024;
	static const size_t kCcbSizeInBytes					=      256 * 1024;
#endif

	Game::GameData* gameData = Game::CreateGameData();
	Gnm::GpuMode gpuMode = Gnm::getGpuMode();
	int ret;

	// Initialize the WB_ONION memory allocator
	LinearAllocator onionAllocator;
	ret = onionAllocator.initialize(
		kOnionMemorySize, SCE_KERNEL_WB_ONION,
		SCE_KERNEL_PROT_CPU_RW | SCE_KERNEL_PROT_GPU_ALL);
	if( ret != SCE_OK )
		return ret;

	// Initialize the WC_GARLIC memory allocator
	// NOTE: CPU reads from GARLIC write-combined memory have a very low
	//       bandwidth so they are disabled for safety in this sample
	LinearAllocator garlicAllocator;
	ret = garlicAllocator.initialize(
		kGarlicMemorySize,
		SCE_KERNEL_WC_GARLIC,
		SCE_KERNEL_PROT_CPU_WRITE | SCE_KERNEL_PROT_GPU_ALL);
	if( ret != SCE_OK )
		return ret;

	// Open the video output port
	int videoOutHandle = sceVideoOutOpen(0, SCE_VIDEO_OUT_BUS_TYPE_MAIN, 0, NULL);
	if( videoOutHandle < 0 )
	{
		printf("sceVideoOutOpen failed: 0x%08X\n", videoOutHandle);
		return videoOutHandle;
	}

	// Initialize the flip rate: 0: 60Hz, 1: 30Hz or 2: 20Hz
	ret = sceVideoOutSetFlipRate(videoOutHandle, 0);
	if( ret != SCE_OK )
	{
		printf("sceVideoOutSetFlipRate failed: 0x%08X\n", ret);
		return ret;
	}

	// Create the event queue for used to synchronize with end-of-pipe interrupts
	SceKernelEqueue eopEventQueue;
	ret = sceKernelCreateEqueue(&eopEventQueue, "EOP QUEUE");
	if( ret != SCE_OK )
	{
		printf("sceKernelCreateEqueue failed: 0x%08X\n", ret);
		return ret;
	}

	// Register for the end-of-pipe events
	ret = Gnm::addEqEvent(eopEventQueue, Gnm::kEqEventGfxEop, NULL);
	if( ret != SCE_OK )
	{
		printf("Gnm::addEqEvent failed: 0x%08X\n", ret);
		return ret;
	}

	// Initialize the Toolkit module
	sce::Gnmx::Toolkit::Allocators toolkitAllocators;
	onionAllocator.getIAllocator(toolkitAllocators.m_onion);
	garlicAllocator.getIAllocator(toolkitAllocators.m_garlic);
	Toolkit::initializeWithAllocators(&toolkitAllocators);

	// Load the shader binaries from disk
	VsShader *vsShader = loadShaderFromFile<VsShader>("/app0/shader_vv.sb", toolkitAllocators);
	PsShader *psShader = loadShaderFromFile<PsShader>("/app0/shader_p.sb", toolkitAllocators);
	if( !vsShader || !psShader )
	{
		printf("Cannot load the shaders\n");
		return SCE_KERNEL_ERROR_EIO;
	}

	// Allocate the memory for the fetch shader
	void *fsMem = garlicAllocator.allocate(
		Gnmx::computeVsFetchShaderSize(vsShader),
		Gnm::kAlignmentOfFetchShaderInBytes);
	if( !fsMem )
	{
		printf("Cannot allocate the fetch shader memory\n");
		return SCE_KERNEL_ERROR_ENOMEM;
	}

	// Generate the fetch shader for the VS stage
    Gnm::FetchShaderInstancingMode *instancingData = NULL;
	uint32_t shaderModifier;
    Gnmx::generateVsFetchShader(fsMem, &shaderModifier, vsShader, instancingData, instancingData != nullptr ? 256 : 0);

	// Generate the shader input caches.
	// Using a pre-calculated shader input cache is optional with CUE but it
	// normally reduces the CPU time necessary to build the command buffers.
	Gnmx::InputOffsetsCache vsInputOffsetsCache, psInputOffsetsCache;
	Gnmx::generateInputOffsetsCache(&vsInputOffsetsCache, Gnm::kShaderStageVs, vsShader);
	Gnmx::generateInputOffsetsCache(&psInputOffsetsCache, Gnm::kShaderStagePs, psShader);

	// For simplicity reasons the sample uses a single GfxContext for each
	// frame. Implementing more complex schemes where multipleGfxContext-s
	// are submitted in each frame is possible as well, but it is out of the
	// scope for this basic sample.
	typedef struct RenderContext
	{
		Gnmx::GnmxGfxContext    gfxContext;
#if SCE_GNMX_ENABLE_GFX_LCUE
		void                   *resourceBuffer;
		void                   *dcbBuffer;
#else
		void                   *cueHeap;
		void                   *dcbBuffer;
		void                   *ccbBuffer;
#endif
		volatile uint32_t      *contextLabel;
	}
	RenderContext;

	typedef struct DisplayBuffer
	{
		Gnm::RenderTarget       renderTarget;
		int                     displayIndex;
	}
	DisplayBuffer;

	enum RenderContextState
	{
		kRenderContextFree = 0,
		kRenderContextInUse,
	};

	RenderContext renderContexts[kRenderContextCount];
	RenderContext *renderContext = renderContexts;
	uint32_t renderContextIndex = 0;

	// Initialize all the render contexts
	for(uint32_t i=0; i<kRenderContextCount; ++i)
	{
#if SCE_GNMX_ENABLE_GFX_LCUE
		// Calculate the size of the resource buffer for the given number of draw calls
		const uint32_t resourceBufferSizeInBytes =
			Gnmx::LightweightConstantUpdateEngine::computeResourceBufferSize(
				Gnmx::LightweightConstantUpdateEngine::kResourceBufferGraphics,
				kNumLcueBatches);

		// Allocate the LCUE resource buffer memory
		renderContexts[i].resourceBuffer = garlicAllocator.allocate(
			resourceBufferSizeInBytes, Gnm::kAlignmentOfBufferInBytes);

		if( !renderContexts[i].resourceBuffer )
		{
			printf("Cannot allocate the LCUE resource buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Allocate the draw command buffer
		renderContexts[i].dcbBuffer = onionAllocator.allocate(
			kDcbSizeInBytes,
			Gnm::kAlignmentOfBufferInBytes);

		if( !renderContexts[i].dcbBuffer )
		{
			printf("Cannot allocate the draw command buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Initialize the GfxContext used by this rendering context
		renderContexts[i].gfxContext.init(
			renderContexts[i].dcbBuffer,		// Draw command buffer address
			kDcbSizeInBytes,					// Draw command buffer size in bytes
			renderContexts[i].resourceBuffer,	// Resource buffer address
			resourceBufferSizeInBytes,			// Resource buffer address in bytes
			NULL);								// Global resource table

#else // SCE_GNMX_ENABLE_GFX_LCUE

		// Allocate the CUE heap memory
		renderContexts[i].cueHeap = garlicAllocator.allocate(
			Gnmx::ConstantUpdateEngine::computeHeapSize(kCueRingEntries),
			Gnm::kAlignmentOfBufferInBytes);

		if( !renderContexts[i].cueHeap )
		{
			printf("Cannot allocate the CUE heap memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Allocate the draw command buffer
		renderContexts[i].dcbBuffer = onionAllocator.allocate(
			kDcbSizeInBytes,
			Gnm::kAlignmentOfBufferInBytes);

		if( !renderContexts[i].dcbBuffer )
		{
			printf("Cannot allocate the draw command buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Allocate the constants command buffer
		renderContexts[i].ccbBuffer = onionAllocator.allocate(
			kCcbSizeInBytes,
			Gnm::kAlignmentOfBufferInBytes);

		if( !renderContexts[i].ccbBuffer )
		{
			printf("Cannot allocate the constants command buffer memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		// Initialize the GfxContext used by this rendering context
		renderContexts[i].gfxContext.init(
			renderContexts[i].cueHeap,
			kCueRingEntries,
			renderContexts[i].dcbBuffer,
			kDcbSizeInBytes,
			renderContexts[i].ccbBuffer,
			kCcbSizeInBytes);
#endif	// SCE_GNMX_ENABLE_GFX_LCUE

		renderContexts[i].contextLabel = (volatile uint32_t*) onionAllocator.allocate(4, 8);
		if( !renderContexts[i].contextLabel )
		{
			printf("Cannot allocate a GPU label\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		renderContexts[i].contextLabel[0] = kRenderContextFree;
	}

	DisplayBuffer displayBuffers[kDisplayBufferCount];
	DisplayBuffer *backBuffer = displayBuffers;
	uint32_t backBufferIndex = 0;

	// Convenience array used by sceVideoOutRegisterBuffers()
	void *surfaceAddresses[kDisplayBufferCount];

	SceVideoOutResolutionStatus status;
	if (SCE_OK == sceVideoOutGetResolutionStatus(videoOutHandle, &status) && status.fullHeight > 1080)
	{
		kDisplayBufferWidth *= 2;
		kDisplayBufferHeight *= 2;
	}

	// Initialize all the display buffers
	for(uint32_t i=0; i<kDisplayBufferCount; ++i)
	{
		// Compute the tiling mode for the render target
		Gnm::TileMode tileMode;
		Gnm::DataFormat format = Gnm::kDataFormatB8G8R8A8UnormSrgb;
		ret = GpuAddress::computeSurfaceTileMode(
			gpuMode, // NEO or base
			&tileMode,										// Tile mode pointer
			GpuAddress::kSurfaceTypeColorTargetDisplayable,	// Surface type
			format,											// Surface format
			1);												// Elements per pixel
		if( ret != SCE_OK )
		{
			printf("GpuAddress::computeSurfaceTileMode: 0x%08X\n", ret);
			return ret;
		}

		// Initialize the render target descriptor

		Gnm::RenderTargetSpec spec;
		spec.init();
		spec.m_width = kDisplayBufferWidth;
		spec.m_height = kDisplayBufferHeight;
		spec.m_pitch = 0;
		spec.m_numSlices = 1;
		spec.m_colorFormat = format;
		spec.m_colorTileModeHint = tileMode;
		spec.m_minGpuMode = gpuMode;
		spec.m_numSamples = Gnm::kNumSamples1;
		spec.m_numFragments = Gnm::kNumFragments1;
		spec.m_flags.enableCmaskFastClear  = 0;
		spec.m_flags.enableFmaskCompression = 0;
		ret = displayBuffers[i].renderTarget.init(&spec);
		if (ret != SCE_GNM_OK)
			return ret;

		Gnm::SizeAlign sizeAlign = displayBuffers[i].renderTarget.getColorSizeAlign();
		// Allocate the render target memory
		surfaceAddresses[i] = garlicAllocator.allocate(sizeAlign);
		if( !surfaceAddresses[i] )
		{
			printf("Cannot allocate the render target memory\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}
		displayBuffers[i].renderTarget.setAddresses(surfaceAddresses[i], 0, 0);

		displayBuffers[i].displayIndex = i;
	}

	// Initialization the VideoOut buffer descriptor. The pixel format must
	// match with the render target data format, which in this case is
	// Gnm::kDataFormatB8G8R8A8UnormSrgb
	SceVideoOutBufferAttribute videoOutBufferAttribute;
	sceVideoOutSetBufferAttribute(
		&videoOutBufferAttribute,
		SCE_VIDEO_OUT_PIXEL_FORMAT_B8_G8_R8_A8_SRGB,
		SCE_VIDEO_OUT_TILING_MODE_TILE,
		SCE_VIDEO_OUT_ASPECT_RATIO_16_9,
		backBuffer->renderTarget.getWidth(),
		backBuffer->renderTarget.getHeight(),
		backBuffer->renderTarget.getPitch());

	// Register the buffers to the slot: [0..kDisplayBufferCount-1]
	ret = sceVideoOutRegisterBuffers(
		videoOutHandle,
		0, // Start index
		surfaceAddresses,
		kDisplayBufferCount,
		&videoOutBufferAttribute);
	if( ret != SCE_OK )
	{
		printf("sceVideoOutRegisterBuffers failed: 0x%08X\n", ret);
		return ret;
	}

	// Compute the tiling mode for the depth buffer
	Gnm::DataFormat depthFormat = Gnm::DataFormat::build(kZFormat);
	Gnm::TileMode depthTileMode;
	ret = GpuAddress::computeSurfaceTileMode(
		gpuMode, // NEO or Base
		&depthTileMode,									// Tile mode pointer
		GpuAddress::kSurfaceTypeDepthOnlyTarget,		// Surface type
		depthFormat,									// Surface format
		1);												// Elements per pixel
	if( ret != SCE_OK )
	{
		printf("GpuAddress::computeSurfaceTileMode: 0x%08X\n", ret);
		return ret;
	}

	// Initialize the depth buffer descriptor
	Gnm::DepthRenderTarget depthTarget;
	Gnm::SizeAlign stencilSizeAlign;
	Gnm::SizeAlign htileSizeAlign;


	Gnm::DepthRenderTargetSpec spec;
	spec.init();
	spec.m_width = kDisplayBufferWidth;
	spec.m_height = kDisplayBufferHeight;
	spec.m_pitch = 0;
	spec.m_numSlices = 1;
	spec.m_zFormat = depthFormat.getZFormat();
	spec.m_stencilFormat = kStencilFormat;
	spec.m_minGpuMode = gpuMode;
	spec.m_numFragments = Gnm::kNumFragments1;
	spec.m_flags.enableHtileAcceleration =  kHtileEnabled ? 1 : 0;
	

	ret = depthTarget.init(&spec);
	if (ret != SCE_GNM_OK)
		return ret;

	Gnm::SizeAlign depthTargetSizeAlign = depthTarget.getZSizeAlign();



	// Initialize the HTILE buffer, if enabled
	if( kHtileEnabled )
	{
		htileSizeAlign = depthTarget.getHtileSizeAlign();
		void *htileMemory = garlicAllocator.allocate(htileSizeAlign);
		if( !htileMemory )
		{
			printf("Cannot allocate the HTILE buffer\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}

		depthTarget.setHtileAddress(htileMemory);
	}

	// Initialize the stencil buffer, if enabled
	void *stencilMemory = NULL;
	stencilSizeAlign = depthTarget.getStencilSizeAlign();
	if( kStencilFormat != Gnm::kStencilInvalid )
	{
		stencilMemory = garlicAllocator.allocate(stencilSizeAlign);
		if( !stencilMemory )
		{
			printf("Cannot allocate the stencil buffer\n");
			return SCE_KERNEL_ERROR_ENOMEM;
		}
	}

	// Allocate the depth buffer
	void *depthMemory = garlicAllocator.allocate(depthTargetSizeAlign);
	if( !depthMemory )
	{
		printf("Cannot allocate the depth buffer\n");
		return SCE_KERNEL_ERROR_ENOMEM;
	}
	depthTarget.setAddresses(depthMemory, stencilMemory);

	// Initialize a Gnm::Texture object
	Gnm::Texture texture;
	{

		Gnm::TextureSpec spec;
		spec.init();
		spec.m_textureType =  Gnm::kTextureType2d ;
		spec.m_width = 512;
		spec.m_height = 512;
		spec.m_depth = 1;
		spec.m_pitch = 0;
		spec.m_numMipLevels = 1;
		spec.m_numSlices = 1;
		spec.m_format = Gnm::kDataFormatR8G8B8A8UnormSrgb;
		spec.m_tileModeHint = Gnm::kTileModeDisplay_LinearAligned;
		spec.m_minGpuMode = gpuMode;
		spec.m_numFragments = Gnm::kNumFragments1;
		int32_t status = texture.init(&spec);

		if (status != SCE_GNM_OK)
			return status;
	}

	Gnm::SizeAlign textureSizeAlign = texture.getSizeAlign();

	// Allocate the texture data using the alignment returned by initAs2d
	void *textureData = garlicAllocator.allocate(textureSizeAlign);
	if( !textureData )
	{
		printf("Cannot allocate the texture data\n");
		return SCE_KERNEL_ERROR_ENOMEM;
	}

	// Read the texture data
	if( !readRawTextureData("/app0/texture.raw", textureData, textureSizeAlign.m_size) )
	{
		printf("Cannot load the texture data\n");
		return SCE_KERNEL_ERROR_EIO;
	}

	// Set the base data address in the texture object
	texture.setBaseAddress(textureData);

	// Initialize the texture sampler
	Gnm::Sampler sampler;
	sampler.init();
	sampler.setMipFilterMode(Gnm::kMipFilterModeNone);
	sampler.setXyFilterMode(Gnm::kFilterModeBilinear, Gnm::kFilterModeBilinear);

	typedef struct Vertex
	{
		float x, y, z;	// Position
		float r, g, b;	// Color
		float u, v;		// UVs
	} Vertex;

	static const Vertex kVertexData[] =
	{
		// 2    3
		// +----+
		// |\   |
		// | \  |
		// |  \ |
		// |   \|
		// +----+
		// 0    1

		//   POSITION                COLOR               UV
		{-0.5f, -0.5f, 0.0f,    0.7f, 0.7f, 1.0f,    0.0f, 1.0f},
		{ 0.5f, -0.5f, 0.0f,    0.7f, 0.7f, 1.0f,    1.0f, 1.0f},
		{-0.5f,  0.5f, 0.0f,    0.7f, 1.0f, 1.0f,    0.0f, 0.0f},
		{ 0.5f,  0.5f, 0.0f,    1.0f, 0.7f, 1.0f,    1.0f, 0.0f},
	};
	static const uint16_t kIndexData[] =
	{
		0, 1, 2,
		1, 3, 2
	};
	enum VertexElements
	{
		kVertexPosition = 0,
		kVertexColor,
		kVertexUv,
		kVertexElemCount
	};
	static const uint32_t kIndexCount = sizeof(kIndexData) / sizeof(kIndexData[0]);

	// Allocate the vertex buffer memory
	Vertex *vertexData = static_cast<Vertex*>( garlicAllocator.allocate(
		sizeof(kVertexData), Gnm::kAlignmentOfBufferInBytes) );
	if( !vertexData )
	{
		printf("Cannot allocate vertex data\n");
		return SCE_KERNEL_ERROR_ENOMEM;
	}

	// Allocate the index buffer memory
	uint16_t *indexData = static_cast<uint16_t*>( garlicAllocator.allocate(
		sizeof(kIndexData), Gnm::kAlignmentOfBufferInBytes) );
	if( !indexData )
	{
		printf("Cannot allocate index data\n");
		return SCE_KERNEL_ERROR_ENOMEM;
	}

	// Copy the vertex/index data onto the GPU mapped memory
	memcpy(vertexData, kVertexData, sizeof(kVertexData));
	memcpy(indexData, kIndexData, sizeof(kIndexData));

	// Initialize the vertex buffers pointing to each vertex element
	Gnm::Buffer vertexBuffers[kVertexElemCount];

	vertexBuffers[kVertexPosition].initAsVertexBuffer(
		&vertexData->x,
		Gnm::kDataFormatR32G32B32Float,
		sizeof(Vertex),
		sizeof(kVertexData) / sizeof(Vertex));

	vertexBuffers[kVertexColor].initAsVertexBuffer(
		&vertexData->r,
		Gnm::kDataFormatR32G32B32Float,
		sizeof(Vertex),
		sizeof(kVertexData) / sizeof(Vertex));

	vertexBuffers[kVertexUv].initAsVertexBuffer(
		&vertexData->u,
		Gnm::kDataFormatR32G32Float,
		sizeof(Vertex),
		sizeof(kVertexData) / sizeof(Vertex));
	

	// init Gamepad
	// initialization
	
	int32_t player1GamepadHandle; // this will be the id by wich we refer to the gamepad

 
	SceUserServiceUserId userId; // id of the user

	ret = scePadInit(); // initialize 
	if (ret < 0) {
		/* Failed to obtain user ID value */
		return ret;
	}

	ret = sceUserServiceInitialize(nullptr); // initialize user library
	if (ret < 0) {
		return ret;
	}

	// Get user ID value
	ret = sceUserServiceGetInitialUser(&userId); // get user that opened the game
	if (ret < 0) {
		/* Failed to obtain user ID value */
		return ret;
	}

	// get gamepad that the user who opened the game is using
	player1GamepadHandle = scePadOpen(userId, SCE_PAD_PORT_TYPE_STANDARD, 0, NULL);
	if (player1GamepadHandle < 0) {
		/* Setting failed */
	}

	ScePadData prevScePad;
	// get gamepad state
	 ret = scePadReadState(player1GamepadHandle, &prevScePad);
	if (ret == SCE_OK) {
		// Data was successfully obtained
	}

	Gnm::Texture texture2;
	FILE *fp = fopen("/app0/ball.png", "rb");
	if (fp != NULL)
	{
		fseek(fp, 0L, SEEK_END);
		size_t size = ftell(fp);
		fseek(fp, 0L, SEEK_SET);

		uint8_t *buffer = new uint8_t[size];

		bool success = readFileContents(buffer, size, fp);
		if (success)
		{
			int x, y, comp;
			uint8_t *imageData = stbi_load_from_memory(buffer, size, &x, &y, &comp, 4);
			if (imageData != nullptr)
			{
				{

					Gnm::TextureSpec spec;
					spec.init();
					spec.m_textureType = Gnm::kTextureType2d;
					spec.m_width = x;
					spec.m_height = y;
					spec.m_depth = 1;
					spec.m_pitch = 0;
					spec.m_numMipLevels = 1;
					spec.m_numSlices = 1;
					spec.m_format = Gnm::kDataFormatR8G8B8A8UnormSrgb;
					spec.m_tileModeHint = Gnm::kTileModeDisplay_LinearAligned;
					spec.m_minGpuMode = gpuMode;
					spec.m_numFragments = Gnm::kNumFragments1;
					int32_t status = texture2.init(&spec);

					if (status != SCE_GNM_OK)
						return status;
				}

				Gnm::SizeAlign textureSizeAlign = texture2.getSizeAlign();

				// Allocate the texture data using the alignment returned by initAs2d
				uint8_t *textureData = (uint8_t*)garlicAllocator.allocate(textureSizeAlign);
				if (!textureData)
				{
					printf("Cannot allocate the texture data\n");
					return SCE_KERNEL_ERROR_ENOMEM;
				}

				uint32_t pitch = texture2.getPitch();

				for (int j = 0; j < y; ++j)
					for (int i = 0; i < x; ++i)
					{
						textureData[(j * pitch + i) * 4 + 0] = imageData[(j * x + i) * 4 + 0];
						textureData[(j * pitch + i) * 4 + 1] = imageData[(j * x + i) * 4 + 1];
						textureData[(j * pitch + i) * 4 + 2] = imageData[(j * x + i) * 4 + 2];
						textureData[(j * pitch + i) * 4 + 3] = imageData[(j * x + i) * 4 + 3];
					}

				texture2.setBaseAddress(textureData);


				stbi_image_free(imageData);
			}
		}
		delete[] buffer;
		fclose(fp);
	}
	//default gamepad color
	SceUserServiceUserColor userColor;
	ret = sceUserServiceGetUserColor(userId, &userColor);
	if (ret < 0) {
		/* Failed to obtain user ID value */
		return ret;
	}



	Game::Input input = {};
	input.dt = 0.016;
	input.windowHalfSize = { 940, 540 };
	ScePadData currentScePad;
	//////// GameUpdate
	bool changeColor = false;
	bool vibrate = false;
	for(uint32_t frameIndex = 0; frameIndex < 1000; )
	{

		///Get button Circle
		
		
		int ret = scePadReadState(player1GamepadHandle, &currentScePad);
		if (ret == SCE_OK) {
			// Data was successfully obtained

			// compare gamepad state with prev loop iteration state
			if ((currentScePad.buttons & SCE_PAD_BUTTON_CIRCLE) != 0 && (prevScePad.buttons & SCE_PAD_BUTTON_CIRCLE) == 0)
			{
				input.buttonPressed = true;
				input.direction = { 100, 100 };
			}
			if ((currentScePad.buttons & SCE_PAD_BUTTON_CROSS) != 0 && (prevScePad.buttons & SCE_PAD_BUTTON_CROSS) == 0)
			{
				changeColor = !changeColor;
			}
			if ((currentScePad.buttons & SCE_PAD_BUTTON_TRIANGLE) != 0 && (prevScePad.buttons & SCE_PAD_BUTTON_TRIANGLE) == 0)
			{
				vibrate = !vibrate;
			}

			// save prev state
			prevScePad = currentScePad;
		}
		if (changeColor)
		{
			ScePadLightBarParam param; // is an error that the largest "color" is less than 13 (can't "close" the light)
			param.r = 255;//rand() % 13 + 242;
			param.g = 20;//rand() % 13 + 242;
			param.b = 147;// rand() % 13 + 242;
			scePadSetLightBar(player1GamepadHandle, &param);
		}
		else
		{
			scePadResetLightBar(player1GamepadHandle);
		}

		if (vibrate) {
			ScePadVibrationParam param = {};
			param.smallMotor = (uint8_t)(127);
			param.largeMotor = (uint8_t)(127);
			scePadSetVibration(player1GamepadHandle, &param);
		}
		else {
			ScePadVibrationParam param = {};
			param.smallMotor = (uint8_t)(0);
			param.largeMotor = (uint8_t)(0);
			scePadSetVibration(player1GamepadHandle, &param);
		}

		// get the gamepad info (4 deadzones)
		ScePadControllerInformation controllerInfo;
		ret = scePadGetControllerInformation(player1GamepadHandle, &controllerInfo);
		if (ret < 0) return ret;

		// get the left stick position (with deadzone)
		int deadZoneMin = 0x80 - controllerInfo.stickInfo.deadZoneLeft;
		int deadZoneMax = 0x80 + controllerInfo.stickInfo.deadZoneLeft;

		glm::vec2 leftStick;
		if (currentScePad.leftStick.x < deadZoneMin) {
			input.direction.x = ((float)(currentScePad.leftStick.x / (float)deadZoneMin) - 1.0f) * 100;
		}
		else if (currentScePad.leftStick.x > deadZoneMax) {
			input.direction.x = ((float)((currentScePad.leftStick.x - deadZoneMax) / (float)(255 - deadZoneMax))) * 100;
		}
		else {
			input.direction.x = 0;
		}

		if (currentScePad.leftStick.y < deadZoneMin) {
			input.direction.y = -((float)(currentScePad.leftStick.y / (float)deadZoneMin) - 1.0f) * 100;
		}
		else if (currentScePad.leftStick.y > deadZoneMax) {
			input.direction.y = -((float)((currentScePad.leftStick.y - deadZoneMax) / (float)(255 - deadZoneMax))) * 100;
		}
		else {
			input.direction.y = 0;
		}

		
		Game::RenderCommands renderCommands = Game::Update(input, *gameData);
		Gnmx::GnmxGfxContext &gfxc = renderContext->gfxContext;

		// Wait until the context label has been written to make sure that the
		// GPU finished parsing the command buffers before overwriting them
		while( renderContext->contextLabel[0] != kRenderContextFree )
		{
			// Wait for the EOP event
			SceKernelEvent eopEvent;
			int count;
			ret = sceKernelWaitEqueue(eopEventQueue, &eopEvent, 1, &count, NULL);
			if( ret != SCE_OK )
			{
				printf("sceKernelWaitEqueue failed: 0x%08X\n", ret);
			}
		}

		// Reset the flip GPU label
		renderContext->contextLabel[0] = kRenderContextInUse;

		// Reset the graphical context and initialize the hardware state
		gfxc.reset();
		gfxc.initializeDefaultHardwareState();

		// In a real-world scenario, any rendering of off-screen buffers or
		// other compute related processing would go here

		// The waitUntilSafeForRendering stalls the GPU until the scan-out
		// operations on the current display buffer have been completed.
		// This command is not blocking for the CPU.
		//
		// NOTE
		// This command should be used right before writing the display buffer.
		//
		gfxc.waitUntilSafeForRendering(videoOutHandle, backBuffer->displayIndex);

		// Setup the viewport to match the entire screen.
		// The z-scale and z-offset values are used to specify the transformation
		// from clip-space to screen-space
		gfxc.setupScreenViewport(
			0,			// Left
			0,			// Top
			backBuffer->renderTarget.getWidth(),
			backBuffer->renderTarget.getHeight(),
			0.5f,		// Z-scale
			0.5f);		// Z-offset

		// Bind the render & depth targets to the context
		gfxc.setRenderTarget(0, &backBuffer->renderTarget);
		gfxc.setDepthRenderTarget(&depthTarget);

		// Clear the color and the depth target
		Toolkit::SurfaceUtil::clearRenderTarget(gfxc, &backBuffer->renderTarget, kClearColor);
		Toolkit::SurfaceUtil::clearDepthTarget(gfxc, &depthTarget, 1.f);

		// Enable z-writes using a less comparison function
		Gnm::DepthStencilControl dsc;
		dsc.init();
		dsc.setDepthControl(Gnm::kDepthControlZWriteEnable, Gnm::kCompareFuncLess);
		dsc.setDepthEnable(true);
		gfxc.setDepthStencilControl(dsc);

		// Cull clock-wise backfaces
		Gnm::PrimitiveSetup primSetupReg;
		primSetupReg.init();
		primSetupReg.setCullFace(Gnm::kPrimitiveSetupCullFaceBack);
		primSetupReg.setFrontFace(Gnm::kPrimitiveSetupFrontFaceCcw);
		gfxc.setPrimitiveSetup(primSetupReg);

		// Setup an additive blending mode
		Gnm::BlendControl blendControl;
		blendControl.init();
		blendControl.setBlendEnable(true);
		blendControl.setColorEquation(
			Gnm::kBlendMultiplierSrcAlpha,
			Gnm::kBlendFuncAdd,
			Gnm::kBlendMultiplierOneMinusSrcAlpha);
		gfxc.setBlendControl(0, blendControl);

		// Setup the output color mask
		gfxc.setRenderTargetMask(0xF);

		// Activate the VS and PS shader stages
		gfxc.setActiveShaderStages(Gnm::kActiveShaderStagesVsPs);
		gfxc.setVsShader(vsShader, shaderModifier, fsMem, &vsInputOffsetsCache);
		gfxc.setPsShader(psShader, &psInputOffsetsCache);

		// Setup the vertex buffer used by the ES stage (vertex shader)
		// Note that the setXxx methods of GfxContext which are used to set
		// shader resources (e.g.: V#, T#, S#, ...) map directly on the
		// Constants Update Engine. These methods do not directly produce PM4
		// packets in the command buffer. The CUE gathers all the resource
		// definitions and creates a set of PM4 packets later on in the
		// gfxc.drawXxx method.
		gfxc.setVertexBuffers(Gnm::kShaderStageVs, 0, kVertexElemCount, vertexBuffers);

		// render sprites in the ps4 modifying the sample
		for (Game::RenderCommands::Sprite &sprite : renderCommands.sprites)
		{
			ShaderConstants *constants = static_cast<ShaderConstants*>(
				gfxc.allocateFromCommandBuffer(sizeof(ShaderConstants), Gnm::kEmbeddedDataAlignment4));
			if (constants)
			{
				const float kAspectRatio = float(kDisplayBufferWidth) / float(kDisplayBufferHeight);
				const Matrix4 scaleMatrix = Matrix4::scale(Vector3(sprite.size.x, sprite.size.y, 1));
				const Matrix4 rotationMatrix = Matrix4::rotationZ(sprite.rotation);
				const Matrix4 translateMatrix = Matrix4::translation(Vector3(sprite.position.x, sprite.position.y, 0));
				//const Matrix4 orthoMatrix = Matrix4::orthographic(-100 * kAspectRatio, 100 * kAspectRatio, -100, 100, 0, 1);
				const Matrix4 projection = Matrix4::orthographic(-(float)input.windowHalfSize.x, (float)input.windowHalfSize.x, -(float)input.windowHalfSize.y, (float)input.windowHalfSize.y, -5.0f, 5.0f);
				constants->m_WorldViewProj = (projection * translateMatrix * rotationMatrix * scaleMatrix);

				Gnm::Buffer constBuffer;
				constBuffer.initAsConstantBuffer(constants, sizeof(ShaderConstants));
				gfxc.setConstantBuffers(Gnm::kShaderStageVs, 0, 1, &constBuffer);

				if (sprite.texture == Game::RenderCommands::TextureNames::PLAYER) {
					gfxc.setTextures(Gnm::kShaderStagePs, 0, 1, &texture);
				}
				else gfxc.setTextures(Gnm::kShaderStagePs, 0, 1, &texture2);
				gfxc.setSamplers(Gnm::kShaderStagePs, 0, 1, &sampler);
				// Submit a draw call
				gfxc.drawIndex(kIndexCount, indexData);
			}
			else
			{
				printf("Cannot allocate vertex shader constants\n");
			}

		}

		// Submit the command buffers, request a flip of the display buffer and
		// write the GPU label that determines the render context state (free)
		// and trigger a software interrupt to signal the EOP event queue
		// NOTE: for this basic sample we are submitting a single GfxContext
		// per frame. Submitting multiple GfxContext-s per frame is allowed.
		// Multiple contexts are processed in order, i.e.: they start in
		// submission order and end in submission order.
		ret = gfxc.submitAndFlipWithEopInterrupt(
			videoOutHandle,
			backBuffer->displayIndex,
			SCE_VIDEO_OUT_FLIP_MODE_VSYNC,
			0,
			sce::Gnm::kEopFlushCbDbCaches,
			const_cast<uint32_t*>(renderContext->contextLabel),
			kRenderContextFree,
			sce::Gnm::kCacheActionWriteBackAndInvalidateL1andL2);
		if( ret != sce::Gnm::kSubmissionSuccess )
		{
			// Analyze the error code to determine whether the command buffers
			// have been submitted to the GPU or not
			if( ret & sce::Gnm::kStatusMaskError )
			{
				// Error codes in the kStatusMaskError family block submissions
				// so we need to mark this render context as not-in-flight
				renderContext->contextLabel[0] = kRenderContextFree;
			}

			printf("GfxContext::submitAndFlip failed: 0x%08X\n", ret);
		}

		// Signal the system that every draw for this frame has been submitted.
		// This function gives permission to the OS to hibernate when all the
		// currently running GPU tasks (graphics and compute) are done.
		ret = Gnm::submitDone();
		if( ret != SCE_OK )
		{
			printf("Gnm::submitDone failed: 0x%08X\n", ret);
		}
		
		// Rotate the display buffers
		backBufferIndex = (backBufferIndex + 1) % kDisplayBufferCount;
		backBuffer = displayBuffers + backBufferIndex;

		// Rotate the render contexts
		renderContextIndex = (renderContextIndex + 1) % kRenderContextCount;
		renderContext = renderContexts + renderContextIndex;
	}

	// Wait for the GPU to be idle before deallocating its resources
	for(uint32_t i=0; i<kRenderContextCount; ++i)
	{
		if( renderContexts[i].contextLabel )
		{
			while( renderContexts[i].contextLabel[0] != kRenderContextFree )
			{
				sceKernelUsleep(1000);
			}
		}
	}

	// Unregister the EOP event queue
	ret = Gnm::deleteEqEvent(eopEventQueue, Gnm::kEqEventGfxEop);
	if( ret != SCE_OK )
	{
		printf("Gnm::deleteEqEvent failed: 0x%08X\n", ret);
	}

	// Destroy the EOP event queue
	ret = sceKernelDeleteEqueue(eopEventQueue);
	if( ret != SCE_OK )
	{
		printf("sceKernelDeleteEqueue failed: 0x%08X\n", ret);
	}

	// Terminate the video output
	ret = sceVideoOutClose(videoOutHandle);
	if( ret != SCE_OK )
	{
		printf("sceVideoOutClose failed: 0x%08X\n", ret);
	}

	// Releasing manually each allocated resource is not necessary as we are
	// terminating the linear allocators for ONION and GARLIC here.
	onionAllocator.terminate();
	garlicAllocator.terminate();

	return 0;
}
