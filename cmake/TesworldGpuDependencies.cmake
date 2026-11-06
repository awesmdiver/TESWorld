# The GPU stack (TESWORLD_WITH_GPU): BasicRHI (D3D12 and Vulkan) and OpenRenderGraph over it, at FasterNGIO's pins.
# Each is made available before the next, so their own ../sibling fallbacks are never taken; a host that already
# provides a target keeps its own. Shaders are compiled at build time (TesworldShaders.cmake): no DXC at runtime.
include_guard(GLOBAL)

# D3D12 is Windows-only. OpenRenderGraph uses BasicRHI's Vulkan interop unconditionally, so Vulkan is always built: the
# Vulkan SDK (or headers with VK_EXT_descriptor_heap, 1.4.357 or later) is needed to build, on Windows too.
# These switches are tesworld's choices for copies it fetches. A host that built its own (SARP, through BasicRenderer)
# made its own choices, and forcing these into the cache would undo them at its next configure.
if(NOT TARGET BasicRHI::BasicRHI)
	if(WIN32)
		set(BASICRHI_ENABLE_D3D12 ON CACHE BOOL "" FORCE)
	else()
		set(BASICRHI_ENABLE_D3D12 OFF CACHE BOOL "" FORCE)
	endif()
	set(BASICRHI_ENABLE_VULKAN ON CACHE BOOL "" FORCE)
	set(BASICRHI_ENABLE_STREAMLINE OFF CACHE BOOL "" FORCE)
	set(BASICRHI_ENABLE_PIX OFF CACHE BOOL "" FORCE)
	set(BASICRHI_ENABLE_IMGUI OFF CACHE BOOL "" FORCE)
	set(BASICRHI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(BASICRHI_SPDLOG_TARGET "spdlog::spdlog_header_only" CACHE STRING "" FORCE)
	set(BASICRHI_ENABLE_TRACY_GPU_PROFILING OFF CACHE BOOL "" FORCE)
	set(BASICRHI_ENABLE_TRACY_CPU_PROFILING OFF CACHE BOOL "" FORCE)
endif()
if(NOT TARGET OpenRenderGraph::OpenRenderGraph)
	set(OPENRENDERGRAPH_BUILD_TESTS OFF CACHE BOOL "" FORCE)
	set(OPENRENDERGRAPH_ENABLE_DEBUG_UI OFF CACHE BOOL "" FORCE)
	set(OPENRENDERGRAPH_ENABLE_SUBMODULE_FALLBACK OFF CACHE BOOL "" FORCE)
endif()

if(NOT TARGET volk::volk AND NOT TARGET BasicRHI::BasicRHI)
	# Volk's vk* globals in its own namespace, as BasicRHI wants in a mixed D3D12/Vulkan executable.
	set(VOLK_NAMESPACE ON CACHE BOOL "" FORCE)
	FetchContent_Declare(volk
		GIT_REPOSITORY https://github.com/zeux/volk.git
		GIT_TAG d41d1af15680c369b182fb913a6fb133045b0077
		EXCLUDE_FROM_ALL
		SYSTEM)
	FetchContent_MakeAvailable(volk)
endif()
if(NOT TARGET BasicRHI::BasicRHI)
	FetchContent_Declare(BasicRHI
		GIT_REPOSITORY https://github.com/panthuncia/BasicRHI.git
		GIT_TAG beb5fcdacfa8384f476af5bec2fbc45bfdf6dba8
		EXCLUDE_FROM_ALL
		SYSTEM)
	FetchContent_MakeAvailable(BasicRHI)
	if(TARGET BasicRHI AND NOT TARGET BasicRHI::BasicRHI)
		add_library(BasicRHI::BasicRHI ALIAS BasicRHI)
	endif()
endif()
if(NOT TARGET OpenRenderGraph::OpenRenderGraph)
	FetchContent_Declare(OpenRenderGraph
		GIT_REPOSITORY https://github.com/panthuncia/OpenRenderGraph.git
		GIT_TAG b8cd0847feff8cd45dcd3a71710664afe3f73d27
		EXCLUDE_FROM_ALL
		SYSTEM)
	FetchContent_MakeAvailable(OpenRenderGraph)
endif()

# Build-time shader compiler: vcpkg's directx-dxc (it ships dxil.dll beside dxc, so DXIL comes out signed). Its prebuilt
# Linux binary needs glibc 2.38; on an older machine pass another with -DTESWORLD_DXC_EXECUTABLE=<path>. It must support
# -fvk-bind-resource-heap.
find_program(TESWORLD_DXC_EXECUTABLE NAMES dxc PATH_SUFFIXES tools/directx-dxc NO_CMAKE_ENVIRONMENT_PATH NO_SYSTEM_ENVIRONMENT_PATH
	DOC "DXC that compiles tesworld's GPU shaders at build time")
if(NOT TESWORLD_DXC_EXECUTABLE)
	message(FATAL_ERROR "dxc not found; install vcpkg's directx-dxc (the 'gpu' feature) or set TESWORLD_DXC_EXECUTABLE")
endif()
execute_process(COMMAND "${TESWORLD_DXC_EXECUTABLE}" --version
	RESULT_VARIABLE _tesworld_dxc_result OUTPUT_VARIABLE _tesworld_dxc_version ERROR_VARIABLE _tesworld_dxc_error
	OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT _tesworld_dxc_result EQUAL 0)
	message(FATAL_ERROR "${TESWORLD_DXC_EXECUTABLE} does not run on this machine; set TESWORLD_DXC_EXECUTABLE to another dxc:\n${_tesworld_dxc_error}")
endif()
message(STATUS "tesworld: shader compiler ${TESWORLD_DXC_EXECUTABLE} (${_tesworld_dxc_version})")
# BASICRHI_VULKAN_DXC_FLAGS and basicrhi_compile_spirv: the SPIR-V ABI BasicRHI's Vulkan backend expects.
FetchContent_GetProperties(BasicRHI SOURCE_DIR _tesworld_basicrhi_dir)
if(_tesworld_basicrhi_dir)
	include("${_tesworld_basicrhi_dir}/cmake/BasicRHIShaderFlags.cmake")
endif()
if(NOT COMMAND basicrhi_compile_spirv)
	message(FATAL_ERROR "tesworld: BasicRHIShaderFlags.cmake wasn't found; a host providing BasicRHI must include it")
endif()

foreach(_tesworld_dependency BasicRHI OpenRenderGraph volk BasicTelemetryCore BasicTelemetryTracy)
	if(TARGET ${_tesworld_dependency})
		set_target_properties(${_tesworld_dependency} PROPERTIES SYSTEM TRUE)
	endif()
endforeach()
