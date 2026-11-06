#pragma once

#include "tesworld/Gpu.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <future>
#include <memory>
#include <span>
#include <string>
#include <vector>

// A caller's own compute kernels on tesworld's GPU context (Gpu.h): kernels compiled by the caller, the buffers,
// textures and samplers they read, and submissions that dispatch them and read results back. BasicRHI stays out of
// sight, as in Gpu.h.
//
// Kernels see what tesworld's own do: push constants at b0 (space0), and every resource through
// ResourceDescriptorHeap / SamplerDescriptorHeap at the indices this API hands out. A D3D12 kernel is DXIL for shader
// model 6.6; a Vulkan one is SPIR-V compiled with BasicRHI's shader ABI (BasicRHIShaderFlags.cmake), as
// tesworld_gpu_shader does.
//
// Every call may be made from any thread; each runs on the context's render thread, one at a time. Creating,
// uploading and releasing wait for the render thread; Submit returns once the work is submitted.
namespace tesworld::gpu
{
	// Formats a texture may be uploaded in. Block-compressed ones are uploaded as their blocks and decoded by the
	// device when sampled, as a renderer's would be.
	enum class TextureFormat
	{
		RGBA8,
		RGBA8_sRGB,
		BGRA8,
		BGRA8_sRGB,
		R8,
		BC1,
		BC1_sRGB,
		BC2,
		BC2_sRGB,
		BC3,
		BC3_sRGB,
		BC4,
		BC5,
		BC7,
		BC7_sRGB
	};

	// Bytes per row of a_width texels (per row of 4x4 blocks, for a block-compressed format), tightly packed.
	[[nodiscard]] std::uint32_t TextureRowBytes(TextureFormat a_format, std::uint32_t a_width);
	// Rows of a_height texels (rows of blocks, for a block-compressed format).
	[[nodiscard]] std::uint32_t TextureRows(TextureFormat a_format, std::uint32_t a_height);

	struct TextureMip
	{
		std::uint32_t width{ 0 };
		std::uint32_t height{ 0 };
		// Rows, each a_rowPitch bytes apart; 0 means TextureRowBytes (tightly packed).
		std::span<const std::byte> bytes;
		std::uint32_t rowPitch{ 0 };
	};

	struct TextureDesc
	{
		TextureFormat format{ TextureFormat::RGBA8 };
		// The largest first. Each is half the previous (rounded down, at least 1) in both dimensions.
		std::span<const TextureMip> mips;
		const char* name{ "tesworld texture" };
	};

	enum class Filter
	{
		Point,
		Linear
	};

	enum class Address
	{
		Wrap,
		Mirror,
		Clamp
	};

	struct SamplerDesc
	{
		Filter filter{ Filter::Linear };
		Filter mipFilter{ Filter::Linear };
		Address addressU{ Address::Wrap };
		Address addressV{ Address::Wrap };
	};

	struct KernelDesc
	{
		// One for the API in use is needed (Context::ApiInUse); the other may be empty.
		std::span<const std::byte> dxil;
		std::span<const std::byte> spirv;
		std::string entry;
		// 32-bit words of push constants, at most 64.
		std::uint32_t constantWords{ 0 };
	};

	// Handles are small ids owned by a Compute; 0 is none. Indices are what a kernel indexes the descriptor heaps with.
	struct Kernel
	{
		std::uint32_t id{ 0 };
	};

	struct Buffer
	{
		std::uint32_t id{ 0 };
		std::uint64_t bytes{ 0 };
		// ByteAddressBuffer / RWByteAddressBuffer views: 0xFFFFFFFF where not asked for.
		std::uint32_t srv{ 0xFFFFFFFFu };
		std::uint32_t uav{ 0xFFFFFFFFu };
	};

	struct Texture
	{
		std::uint32_t id{ 0 };
		std::uint32_t width{ 0 };
		std::uint32_t height{ 0 };
		std::uint32_t mips{ 0 };
		// Texture2D<float4>, all mips.
		std::uint32_t srv{ 0xFFFFFFFFu };
	};

	struct Sampler
	{
		std::uint32_t id{ 0 };
		// SamplerDescriptorHeap index.
		std::uint32_t index{ 0xFFFFFFFFu };
	};

	inline constexpr std::uint32_t kNoIndex = 0xFFFFFFFFu;

	// What a submission records, on the render thread. Dispatches run in the order recorded; Barrier waits for every
	// write before it to be visible to everything after it.
	class Recorder
	{
	public:
		virtual ~Recorder() = default;
		virtual void Dispatch(Kernel a_kernel, std::span<const std::uint32_t> a_constants, std::uint32_t a_x, std::uint32_t a_y = 1,
			std::uint32_t a_z = 1) = 0;
		virtual void Barrier() = 0;
		// Copies a_bytes of a_buffer from a_offset into the next of the submission's results, after the dispatches before it.
		virtual void Readback(Buffer a_buffer, std::uint64_t a_offset, std::uint64_t a_bytes) = 0;
		// Buffers that live only as long as this submission, released once the GPU has run it, with no wait for the GPU.
		// Upload's bytes are copied now and kernels read them through its srv; Scratch is a device buffer kernels write
		// through its uav (its contents start undefined) and Readback can copy from.
		virtual Buffer Upload(std::span<const std::byte> a_bytes) = 0;
		virtual Buffer Scratch(std::uint64_t a_bytes) = 0;
	};

	// The bytes of a submission's readbacks, in the order they were recorded.
	using Readbacks = std::vector<std::vector<std::byte>>;

	class Compute
	{
	public:
		explicit Compute(Context& a_context);
		~Compute();
		Compute(const Compute&) = delete;
		Compute& operator=(const Compute&) = delete;

		[[nodiscard]] Context& GetContext() { return _context; }

		// Throws UnsupportedError when there's no bytecode for the API in use.
		[[nodiscard]] Kernel CreateKernel(const KernelDesc& a_desc);
		// A device buffer kernels read (srv) and, with a_writable, write (uav). Contents start undefined.
		[[nodiscard]] Buffer CreateBuffer(std::uint64_t a_bytes, bool a_writable, const char* a_name);
		// A device buffer kernels read, holding a_bytes.
		[[nodiscard]] Buffer Upload(std::span<const std::byte> a_bytes, const char* a_name);
		[[nodiscard]] Texture UploadTexture(const TextureDesc& a_desc);
		[[nodiscard]] Sampler CreateSampler(const SamplerDesc& a_desc);

		void Release(Kernel a_kernel);
		void Release(Buffer a_buffer);
		void Release(Texture a_texture);
		// Samplers live as long as the Compute: the sampler heap is small and a caller needs few.

		// Records through a_record on the render thread and submits. The future holds the readbacks once the GPU has run
		// it, or the render thread's failure.
		[[nodiscard]] std::future<Readbacks> Submit(std::function<void(Recorder&)> a_record);
		// Waits for everything submitted.
		void Finish();

		// Opaque: Compute.cpp's.
		struct State;

	private:
		Context& _context;
		std::unique_ptr<State> _state;
	};
}
