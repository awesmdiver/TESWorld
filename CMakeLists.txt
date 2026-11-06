cmake_minimum_required(VERSION 3.28)

project(tesworld VERSION 0.1.0 LANGUAGES CXX)

# Each target beyond core can be left out, with what it needs: a consumer takes only the dependencies it uses.
option(TESWORLD_WITH_VFS "Build tesworld::vfs (lz4)" ON)
option(TESWORLD_WITH_TEXTURE "Build tesworld::texture (DirectXTex, nlohmann-json; needs vfs)" ON)
option(TESWORLD_WITH_ATLAS "Build tesworld::atlas (xatlas)" ON)
option(TESWORLD_WITH_NIF "Build tesworld::nif (links nifly, which is GPL-3.0)" ON)
option(TESWORLD_WITH_RENDER "Build tesworld::render (a CPU renderer; needs texture)" ON)
option(TESWORLD_WITH_GPU "Build tesworld::gpu (BasicRHI/OpenRenderGraph, D3D12 and Vulkan; vcpkg feature 'gpu'; needs texture)" OFF)
option(TESWORLD_WITH_PROFILING "Compile in BasicTelemetry/Tracy zones (vcpkg feature 'profiling'); off ships nothing" OFF)
option(TESWORLD_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})
option(TESWORLD_BUILD_TOOLS "Build the command-line tools" ${PROJECT_IS_TOP_LEVEL})
if(TESWORLD_WITH_GPU AND NOT TESWORLD_WITH_TEXTURE)
	message(FATAL_ERROR "tesworld::gpu bakes textures: set TESWORLD_WITH_TEXTURE too")
endif()
if(TESWORLD_WITH_TEXTURE AND NOT TESWORLD_WITH_VFS)
	message(FATAL_ERROR "tesworld::texture reads through tesworld::vfs: set TESWORLD_WITH_VFS too")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)
find_package(TBB CONFIG REQUIRED)
find_package(ZLIB REQUIRED)

# Warnings are errors when tesworld is built on its own; a project that includes it keeps its own rules.
function(tesworld_warnings target)
	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8 /Zc:__cplusplus /wd4324 $<$<BOOL:${PROJECT_IS_TOP_LEVEL}>:/WX>)
	else()
		target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-missing-field-initializers $<$<BOOL:${PROJECT_IS_TOP_LEVEL}>:-Werror>)
	endif()
endfunction()

# Profiling: include/tesworld/Profiling.h's zones. Off, they compile to nothing and nothing is linked; on, they are
# BasicTelemetry scopes passed through to Tracy (not on demand: a capture sees a run from its start).
add_library(tesworld_profiling INTERFACE)
add_library(tesworld::profiling ALIAS tesworld_profiling)
target_include_directories(tesworld_profiling INTERFACE
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
if(TESWORLD_WITH_PROFILING OR TESWORLD_WITH_GPU)
	include(cmake/TesworldTelemetry.cmake)
endif()
if(TESWORLD_WITH_PROFILING)
	target_link_libraries(tesworld_profiling INTERFACE BasicTelemetry::Tracy BasicTelemetry::Artifacts TBB::tbb)
	target_compile_definitions(tesworld_profiling INTERFACE TESWORLD_PROFILING=1)
endif()

# Core: the platform layer, plugins and load order, terrain and placement grouping. Depends on zlib and TBB only.
add_library(tesworld_core STATIC
	src/platform/FileByteView.cpp
	src/plugins/Plugins.cpp
	src/plugins/ModelPathNormalization.cpp
	src/grass/GrassCache.cpp
	src/seasons/Seasons.cpp
	src/terrain/HeightField.cpp
	src/scene/QuadGrid.cpp
)
add_library(tesworld::core ALIAS tesworld_core)
target_include_directories(tesworld_core PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
target_compile_features(tesworld_core PUBLIC cxx_std_23)
# TBB stays private: core's headers don't use it, so a consumer with its own TBB setup (SARP's renderer host) isn't
# handed another.
target_link_libraries(tesworld_core PRIVATE ZLIB::ZLIB TBB::tbb)
if(WIN32)
	target_compile_definitions(tesworld_core PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
endif()
target_link_libraries(tesworld_core PRIVATE tesworld_profiling)
tesworld_warnings(tesworld_core)

# A staged, budgeted work pipeline and a shared asset cache: the part of SARP's streaming that isn't its renderer.
add_library(tesworld_pipeline INTERFACE)
add_library(tesworld::pipeline ALIAS tesworld_pipeline)
target_link_libraries(tesworld_pipeline INTERFACE tesworld_core TBB::tbb)

# Jobs: work as a graph of queues that workers pull from (Jobs.h).
add_library(tesworld_jobs STATIC src/jobs/Graph.cpp)
add_library(tesworld::jobs ALIAS tesworld_jobs)
target_include_directories(tesworld_jobs PUBLIC
	$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:include>)
target_compile_features(tesworld_jobs PUBLIC cxx_std_23)
target_link_libraries(tesworld_jobs PUBLIC TBB::tbb PRIVATE tesworld_profiling)
tesworld_warnings(tesworld_jobs)

# The game's file system: loose files over BSA archives, read into memory.
if(TESWORLD_WITH_VFS)
	find_package(lz4 CONFIG REQUIRED)
	add_library(tesworld_vfs STATIC
		src/vfs/BsaArchive.cpp
		src/vfs/FileSystem.cpp
	)
	add_library(tesworld::vfs ALIAS tesworld_vfs)
	target_link_libraries(tesworld_vfs PUBLIC tesworld_core PRIVATE ZLIB::ZLIB lz4::lz4 TBB::tbb)
	target_link_libraries(tesworld_vfs PRIVATE tesworld_profiling)
	tesworld_warnings(tesworld_vfs)
endif()

# Textures: DDS decoding and encoding (DirectXTex), mip chains and sampling, SARP's stochastic texture artifacts, its
# terrain material evaluation (the CPU form of BasicRenderer's terrain shader) and Community Shaders' PBR data.
if(TESWORLD_WITH_TEXTURE)
	find_package(nlohmann_json CONFIG REQUIRED)
	find_package(directxtex CONFIG REQUIRED)
	add_library(tesworld_texture STATIC
		src/texture/Texture.cpp
		src/texture/Stochastic.cpp
		src/texture/TerrainMaterial.cpp
		src/texture/TerrainLayers.cpp
		src/texture/TerrainBake.cpp
		src/texture/TextureEncoder.cpp
		src/texture/CommunityShadersPBR.cpp
	)
	add_library(tesworld::texture ALIAS tesworld_texture)
	target_link_libraries(tesworld_texture PUBLIC tesworld_core tesworld_vfs PRIVATE Microsoft::DirectXTex nlohmann_json::nlohmann_json TBB::tbb)
	target_link_libraries(tesworld_texture PRIVATE tesworld_profiling)
	tesworld_warnings(tesworld_texture)
endif()

# Atlases: xatlas (at SARP's pin) charts and packs meshes and rectangles; a rasterizer bakes into the pages.
if(TESWORLD_WITH_ATLAS)
	FetchContent_Declare(xatlas
		GIT_REPOSITORY https://github.com/jpcy/xatlas.git
		GIT_TAG f700c7790aaa030e794b52ba7791a05c085faf0c
		SOURCE_SUBDIR do-not-build
		SYSTEM)
	FetchContent_MakeAvailable(xatlas)
	add_library(tesworld_xatlas STATIC "${xatlas_SOURCE_DIR}/source/xatlas/xatlas.cpp")
	target_include_directories(tesworld_xatlas SYSTEM PUBLIC "${xatlas_SOURCE_DIR}/source/xatlas")
	# One thread an atlas: xatlas would start a pool for every atlas, and tesworld packs many at once on TBB.
	target_compile_definitions(tesworld_xatlas PRIVATE XA_MULTITHREADED=0)
	add_library(tesworld_atlas STATIC src/atlas/Atlas.cpp)
	add_library(tesworld::atlas ALIAS tesworld_atlas)
	target_link_libraries(tesworld_atlas PUBLIC tesworld_core PRIVATE tesworld_xatlas TBB::tbb)
	target_link_libraries(tesworld_atlas PRIVATE tesworld_profiling)
	tesworld_warnings(tesworld_atlas)
endif()

if(TESWORLD_WITH_NIF)
	# nifly from upstream at a pinned commit; vcpkg's port is years behind.
	FetchContent_Declare(nifly
		GIT_REPOSITORY https://github.com/ousnius/nifly.git
		GIT_TAG cca0a770094bb962fb28ea1fec5ea903e68fda8e
		PATCH_COMMAND "${CMAKE_COMMAND}" -P "${CMAKE_CURRENT_LIST_DIR}/cmake/PatchNifly.cmake"
		EXCLUDE_FROM_ALL
		SYSTEM)
	set(BUILD_TESTING_SAVED ${BUILD_TESTING})
	set(BUILD_TESTING OFF)
	FetchContent_MakeAvailable(nifly)
	set(BUILD_TESTING ${BUILD_TESTING_SAVED})

	add_library(tesworld_nif STATIC
		src/nif/NifReader.cpp
		src/nif/NifTextureRewrite.cpp
		src/nif/NifWriter.cpp
		src/nif/NifWriterReference.cpp
	)
	add_library(tesworld::nif ALIAS tesworld_nif)
	target_link_libraries(tesworld_nif PUBLIC tesworld_core PRIVATE nifly)
	target_link_libraries(tesworld_nif PRIVATE tesworld_profiling)
	tesworld_warnings(tesworld_nif)
endif()

if(TESWORLD_WITH_RENDER AND TESWORLD_WITH_TEXTURE)
	# Draws models (nif::Model, read by tesworld::nif) into images: billboards, previews. The interface leaves room
	# for a GPU backend.
	add_library(tesworld_render STATIC src/render/Render.cpp src/render/ViewRenderer.cpp)
	add_library(tesworld::render ALIAS tesworld_render)
	target_link_libraries(tesworld_render PUBLIC tesworld_core tesworld_texture TBB::tbb)
	target_link_libraries(tesworld_render PRIVATE tesworld_profiling)
	tesworld_warnings(tesworld_render)
endif()

if(TESWORLD_WITH_GPU)
	# Compute on the GPU when there is one: a headless device and OpenRenderGraph host (Gpu.h), and the terrain bake on it
	# (TerrainBakeGpu.h). Its headers keep BasicRHI and OpenRenderGraph out of sight: they're linked privately.
	include(cmake/TesworldGpuDependencies.cmake)
	include(cmake/TesworldShaders.cmake)
	set(TESWORLD_SHADER_DIR "${PROJECT_SOURCE_DIR}/src/gpu/shaders")
	file(GLOB TESWORLD_SHADER_HEADERS CONFIGURE_DEPENDS "${TESWORLD_SHADER_DIR}/*.hlsli")
	set(TESWORLD_SHADERS "")
	tesworld_gpu_shader(TESWORLD_SHADERS NAME TerrainBake SOURCE "${TESWORLD_SHADER_DIR}/TerrainBake.hlsl" ENTRY BakeCS DEPENDS ${TESWORLD_SHADER_HEADERS})
	tesworld_gpu_shader(TESWORLD_SHADERS NAME HalfSize SOURCE "${TESWORLD_SHADER_DIR}/Mips.hlsl" ENTRY HalfSizeCS DEPENDS ${TESWORLD_SHADER_HEADERS})
	tesworld_gpu_shader(TESWORLD_SHADERS NAME Bc7Mode6 SOURCE "${TESWORLD_SHADER_DIR}/Bc7Mode6.hlsl" ENTRY Bc7CS DEPENDS ${TESWORLD_SHADER_HEADERS})
	tesworld_gpu_shader(TESWORLD_SHADERS NAME Fill SOURCE "${TESWORLD_SHADER_DIR}/Fill.hlsl" ENTRY FillCS DEPENDS ${TESWORLD_SHADER_HEADERS})
	if(TESWORLD_WITH_RENDER)
		tesworld_gpu_shader(TESWORLD_SHADERS NAME View.VS SOURCE "${TESWORLD_SHADER_DIR}/View.hlsl" ENTRY ViewVS PROFILE vs_6_6 DEPENDS ${TESWORLD_SHADER_HEADERS})
		tesworld_gpu_shader(TESWORLD_SHADERS NAME View.PS SOURCE "${TESWORLD_SHADER_DIR}/View.hlsl" ENTRY ViewPS PROFILE ps_6_6 DEPENDS ${TESWORLD_SHADER_HEADERS})
		tesworld_gpu_shader(TESWORLD_SHADERS NAME ViewResolve SOURCE "${TESWORLD_SHADER_DIR}/ViewResolve.hlsl" ENTRY ResolveCS DEPENDS ${TESWORLD_SHADER_HEADERS})
	endif()

	add_library(tesworld_gpu STATIC
		src/gpu/Compute.cpp
		src/gpu/Context.cpp
		src/gpu/ImageFrames.cpp
		src/gpu/TerrainBakeGpu.cpp
		src/gpu/TextureEncoderGpu.cpp
	)
	add_library(tesworld::gpu ALIAS tesworld_gpu)
	tesworld_embed_shaders(tesworld_gpu TESWORLD_SHADERS)
	target_include_directories(tesworld_gpu PRIVATE src/gpu)
	target_link_libraries(tesworld_gpu PUBLIC tesworld_core tesworld_texture
		PRIVATE OpenRenderGraph::OpenRenderGraph BasicRHI::BasicRHI TBB::tbb tesworld_profiling)
	# Views rendered on the GPU (RenderGpu.h), with tesworld::render's CPU renderer behind them.
	if(TESWORLD_WITH_RENDER)
		target_sources(tesworld_gpu PRIVATE src/gpu/ViewRendererGpu.cpp)
		target_link_libraries(tesworld_gpu PUBLIC tesworld_render)
	endif()
	if(WIN32)
		target_compile_definitions(tesworld_gpu PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
	endif()
	tesworld_warnings(tesworld_gpu)
endif()

if(TESWORLD_BUILD_TOOLS AND TESWORLD_WITH_TEXTURE)
	add_executable(tesworld-extract tools/Extract.cpp)
	target_link_libraries(tesworld-extract PRIVATE tesworld_vfs)
	tesworld_warnings(tesworld-extract)
	add_executable(tesworld-dds2png tools/DdsToPng.cpp)
	target_link_libraries(tesworld-dds2png PRIVATE tesworld_texture ZLIB::ZLIB)
	if(TESWORLD_WITH_NIF)
		add_executable(tesworld-nifdump tools/NifDump.cpp)
		target_link_libraries(tesworld-nifdump PRIVATE nifly)
		add_executable(tesworld-topdown tools/RenderTopDown.cpp)
		target_link_libraries(tesworld-topdown PRIVATE tesworld_nif tesworld_texture tesworld_vfs ZLIB::ZLIB)

		if(TESWORLD_WITH_RENDER)
			add_executable(tesworld-render tools/Render.cpp)
			target_link_libraries(tesworld-render PRIVATE tesworld_nif tesworld_render tesworld_vfs ZLIB::ZLIB)
		endif()
	endif()
endif()

if(TESWORLD_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
endif()
