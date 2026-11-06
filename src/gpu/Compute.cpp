#include "tesworld/GpuCompute.h"

#include "ContextImpl.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace tesworld::gpu
{
	namespace
	{
		using detail::ContextImpl;

		rhi::Format ToRhi(TextureFormat a_format)
		{
			switch (a_format) {
			case TextureFormat::RGBA8:
				return rhi::Format::R8G8B8A8_UNorm;
			case TextureFormat::RGBA8_sRGB:
				return rhi::Format::R8G8B8A8_UNorm_sRGB;
			case TextureFormat::BGRA8:
				return rhi::Format::B8G8R8A8_UNorm;
			case TextureFormat::BGRA8_sRGB:
				return rhi::Format::B8G8R8A8_UNorm_sRGB;
			case TextureFormat::R8:
				return rhi::Format::R8_UNorm;
			case TextureFormat::BC1:
				return rhi::Format::BC1_UNorm;
			case TextureFormat::BC1_sRGB:
				return rhi::Format::BC1_UNorm_sRGB;
			case TextureFormat::BC2:
				return rhi::Format::BC2_UNorm;
			case TextureFormat::BC2_sRGB:
				return rhi::Format::BC2_UNorm_sRGB;
			case TextureFormat::BC3:
				return rhi::Format::BC3_UNorm;
			case TextureFormat::BC3_sRGB:
				return rhi::Format::BC3_UNorm_sRGB;
			case TextureFormat::BC4:
				return rhi::Format::BC4_UNorm;
			case TextureFormat::BC5:
				return rhi::Format::BC5_UNorm;
			case TextureFormat::BC7:
				return rhi::Format::BC7_UNorm;
			case TextureFormat::BC7_sRGB:
				return rhi::Format::BC7_UNorm_sRGB;
			}
			throw std::invalid_argument("GPU: unknown texture format");
		}

		// Bytes per 4x4 block, or 0 for a format of single texels.
		std::uint32_t BlockBytes(TextureFormat a_format)
		{
			switch (a_format) {
			case TextureFormat::BC1:
			case TextureFormat::BC1_sRGB:
			case TextureFormat::BC4:
				return 8;
			case TextureFormat::BC2:
			case TextureFormat::BC2_sRGB:
			case TextureFormat::BC3:
			case TextureFormat::BC3_sRGB:
			case TextureFormat::BC5:
			case TextureFormat::BC7:
			case TextureFormat::BC7_sRGB:
				return 16;
			default:
				return 0;
			}
		}

		std::uint32_t TexelBytes(TextureFormat a_format)
		{
			return a_format == TextureFormat::R8 ? 1u : 4u;
		}

		rhi::AddressMode ToRhi(Address a_address)
		{
			switch (a_address) {
			case Address::Wrap:
				return rhi::AddressMode::Wrap;
			case Address::Mirror:
				return rhi::AddressMode::Mirror;
			case Address::Clamp:
				return rhi::AddressMode::Clamp;
			}
			return rhi::AddressMode::Wrap;
		}

		// A readback a submission recorded: its host copy, read once the frame is done.
		struct PendingReadback
		{
			std::shared_ptr<detail::MappedBuffer> buffer;
			std::uint64_t bytes{ 0 };
		};

		// What a submission made for itself (Recorder::Upload, Scratch), kept until the GPU has run it.
		struct Transients
		{
			std::unordered_map<std::uint32_t, detail::DeviceBuffer> buffers;
			std::vector<detail::MappedBuffer> staging;
		};
	}

	std::uint32_t TextureRowBytes(TextureFormat a_format, std::uint32_t a_width)
	{
		const auto block = BlockBytes(a_format);
		return block != 0 ? (std::max)(1u, (a_width + 3) / 4) * block : a_width * TexelBytes(a_format);
	}

	std::uint32_t TextureRows(TextureFormat a_format, std::uint32_t a_height)
	{
		return BlockBytes(a_format) != 0 ? (std::max)(1u, (a_height + 3) / 4) : a_height;
	}

	// Render thread only, but for the constructor.
	struct Compute::State
	{
		std::unordered_map<std::uint32_t, detail::ComputeProgram> kernels;
		std::unordered_map<std::uint32_t, detail::DeviceBuffer> buffers;
		std::unordered_map<std::uint32_t, detail::SampledTexture> textures;
		std::uint32_t next{ 1 };
	};

	namespace
	{
		class RecorderImpl final : public Recorder
		{
		public:
			RecorderImpl(Compute::State& a_state, rhi::CommandList& a_commands, ContextImpl& a_context, std::vector<PendingReadback>& a_readbacks,
				Transients& a_transients) :
				_state(a_state), _commands(a_commands), _context(a_context), _readbacks(a_readbacks), _transients(a_transients)
			{}

			void Dispatch(Kernel a_kernel, std::span<const std::uint32_t> a_constants, std::uint32_t a_x, std::uint32_t a_y, std::uint32_t a_z) override
			{
				const auto it = _state.kernels.find(a_kernel.id);
				if (it == _state.kernels.end()) {
					throw std::invalid_argument("GPU: dispatch of a kernel this Compute didn't create");
				}
				if (a_constants.size() > it->second.constantWords) {
					throw std::invalid_argument("GPU: more push constants than the kernel declared");
				}
				std::array<std::uint32_t, 64> words{};
				std::ranges::copy(a_constants, words.begin());
				ContextImpl::Dispatch(_commands, it->second, words.data(), a_x, a_y, a_z);
			}

			void Barrier() override
			{
				ContextImpl::Barrier(_commands, rhi::ResourceSyncState::All, rhi::ResourceAccessType::Common, rhi::ResourceSyncState::All,
					rhi::ResourceAccessType::Common);
			}

			void Readback(Buffer a_buffer, std::uint64_t a_offset, std::uint64_t a_bytes) override
			{
				const detail::DeviceBuffer* source = nullptr;
				if (const auto it = _state.buffers.find(a_buffer.id); it != _state.buffers.end()) source = &it->second;
				if (const auto it = _transients.buffers.find(a_buffer.id); it != _transients.buffers.end()) source = &it->second;
				if (!source || a_offset + a_bytes > source->bytes) {
					throw std::invalid_argument("GPU: readback outside a buffer this Compute created");
				}
				// Kernels' writes, before the copy reads them.
				ContextImpl::Barrier(_commands, rhi::ResourceSyncState::All, rhi::ResourceAccessType::UnorderedAccess, rhi::ResourceSyncState::Copy,
					rhi::ResourceAccessType::CopySource);
				auto host = std::make_shared<detail::MappedBuffer>(_context.CreateMapped(a_bytes, rhi::HeapType::Readback, "tesworld readback"));
				_commands.CopyBufferRegion(host->resource->GetHandle(), 0, source->resource->GetHandle(), a_offset, a_bytes);
				_readbacks.push_back(PendingReadback{ std::move(host), a_bytes });
			}

			Buffer Upload(std::span<const std::byte> a_bytes) override
			{
				if (a_bytes.empty()) {
					throw std::invalid_argument("GPU: an empty upload");
				}
				auto buffer = _context.CreateDeviceBuffer(a_bytes.size(), true, false, "tesworld transient upload");
				auto staging = _context.CreateMapped(a_bytes.size(), rhi::HeapType::Upload, "tesworld transient staging");
				std::memcpy(staging.mapped, a_bytes.data(), a_bytes.size());
				_commands.CopyBufferRegion(buffer.resource->GetHandle(), 0, staging.resource->GetHandle(), 0, a_bytes.size());
				// The copy, before any kernel after it reads the buffer.
				ContextImpl::Barrier(_commands, rhi::ResourceSyncState::Copy, rhi::ResourceAccessType::CopyDest, rhi::ResourceSyncState::All,
					rhi::ResourceAccessType::ShaderResource);
				_transients.staging.push_back(std::move(staging));
				return Keep(std::move(buffer));
			}

			Buffer Scratch(std::uint64_t a_bytes) override
			{
				if (a_bytes == 0) {
					throw std::invalid_argument("GPU: an empty scratch buffer");
				}
				return Keep(_context.CreateDeviceBuffer(a_bytes, true, true, "tesworld transient scratch"));
			}

		private:
			Compute::State& _state;
			rhi::CommandList& _commands;
			ContextImpl& _context;
			std::vector<PendingReadback>& _readbacks;
			Transients& _transients;

			Buffer Keep(detail::DeviceBuffer a_buffer)
			{
				const Buffer result{ .id = _state.next++, .bytes = a_buffer.bytes, .srv = a_buffer.srv, .uav = a_buffer.uav };
				_transients.buffers.emplace(result.id, std::move(a_buffer));
				return result;
			}
		};
	}

	Compute::Compute(Context& a_context) :
		_context(a_context), _state(std::make_unique<State>())
	{}

	Compute::~Compute()
	{
		try {
			auto& impl = _context.Impl();
			impl.Run([&] {
				impl.CompleteAllInFlight();
				for (auto& [id, buffer] : _state->buffers) impl.Release(buffer);
				for (auto& [id, texture] : _state->textures) impl.Release(texture);
				_state->kernels.clear();
				_state->buffers.clear();
				_state->textures.clear();
			});
		} catch (...) {
			// The render thread failed: what it held goes with the context.
		}
	}

	Kernel Compute::CreateKernel(const KernelDesc& a_desc)
	{
		if (a_desc.constantWords > 64) {
			throw std::invalid_argument("GPU: at most 64 words of push constants");
		}
		auto& impl = _context.Impl();
		Kernel kernel;
		impl.Run([&] {
			const auto bytecode = impl.ApiInUse() == Api::D3D12 ? a_desc.dxil : a_desc.spirv;
			auto program = impl.CreateProgram(bytecode, a_desc.entry, a_desc.constantWords);
			kernel.id = _state->next++;
			_state->kernels.emplace(kernel.id, std::move(program));
		});
		return kernel;
	}

	Buffer Compute::CreateBuffer(std::uint64_t a_bytes, bool a_writable, const char* a_name)
	{
		auto& impl = _context.Impl();
		Buffer result;
		impl.Run([&] {
			auto buffer = impl.CreateDeviceBuffer(a_bytes, true, a_writable, a_name);
			result = Buffer{ .id = _state->next++, .bytes = buffer.bytes, .srv = buffer.srv, .uav = buffer.uav };
			_state->buffers.emplace(result.id, std::move(buffer));
		});
		return result;
	}

	Buffer Compute::Upload(std::span<const std::byte> a_bytes, const char* a_name)
	{
		auto& impl = _context.Impl();
		Buffer result;
		impl.Run([&] {
			auto buffer = impl.Upload(a_bytes, a_name);
			result = Buffer{ .id = _state->next++, .bytes = buffer.bytes, .srv = buffer.srv, .uav = buffer.uav };
			_state->buffers.emplace(result.id, std::move(buffer));
		});
		return result;
	}

	Texture Compute::UploadTexture(const TextureDesc& a_desc)
	{
		std::vector<detail::TextureLevel> levels;
		levels.reserve(a_desc.mips.size());
		for (const auto& mip : a_desc.mips) {
			const auto rowBytes = TextureRowBytes(a_desc.format, mip.width);
			levels.push_back(detail::TextureLevel{ .width = mip.width,
				.height = mip.height,
				.bytes = mip.bytes,
				.rowPitch = mip.rowPitch != 0 ? mip.rowPitch : rowBytes,
				.rowBytes = rowBytes,
				.rows = TextureRows(a_desc.format, mip.height) });
		}
		const auto format = ToRhi(a_desc.format);
		auto& impl = _context.Impl();
		Texture result;
		impl.Run([&] {
			auto texture = impl.UploadTexture(format, levels, a_desc.name);
			result = Texture{ .id = _state->next++, .width = texture.width, .height = texture.height, .mips = texture.mips, .srv = texture.srv };
			_state->textures.emplace(result.id, std::move(texture));
		});
		return result;
	}

	Sampler Compute::CreateSampler(const SamplerDesc& a_desc)
	{
		rhi::SamplerDesc desc{};
		desc.minFilter = desc.magFilter = a_desc.filter == Filter::Linear ? rhi::Filter::Linear : rhi::Filter::Nearest;
		desc.mipFilter = a_desc.mipFilter == Filter::Linear ? rhi::MipFilter::Linear : rhi::MipFilter::Nearest;
		desc.addressU = ToRhi(a_desc.addressU);
		desc.addressV = ToRhi(a_desc.addressV);
		desc.addressW = rhi::AddressMode::Clamp;
		auto& impl = _context.Impl();
		Sampler result;
		impl.Run([&] { result = Sampler{ .id = _state->next++, .index = impl.CreateSampler(desc) }; });
		return result;
	}

	void Compute::Release(Kernel a_kernel)
	{
		auto& impl = _context.Impl();
		impl.Run([&] {
			impl.CompleteAllInFlight();
			_state->kernels.erase(a_kernel.id);
		});
	}

	void Compute::Release(Buffer a_buffer)
	{
		auto& impl = _context.Impl();
		impl.Run([&] {
			const auto it = _state->buffers.find(a_buffer.id);
			if (it == _state->buffers.end()) return;
			impl.CompleteAllInFlight();
			impl.Release(it->second);
			_state->buffers.erase(it);
		});
	}

	void Compute::Release(Texture a_texture)
	{
		auto& impl = _context.Impl();
		impl.Run([&] {
			const auto it = _state->textures.find(a_texture.id);
			if (it == _state->textures.end()) return;
			impl.CompleteAllInFlight();
			impl.Release(it->second);
			_state->textures.erase(it);
		});
	}

	std::future<Readbacks> Compute::Submit(std::function<void(Recorder&)> a_record)
	{
		auto promise = std::make_shared<std::promise<Readbacks>>();
		auto result = promise->get_future();
		auto& impl = _context.Impl();
		impl.Run([&] {
			auto readbacks = std::make_shared<std::vector<PendingReadback>>();
			auto transients = std::make_shared<Transients>();
			impl.ExecuteFrame(
				[&](rhi::CommandList& a_commands) {
					RecorderImpl recorder(*_state, a_commands, impl, *readbacks, *transients);
					a_record(recorder);
				},
				[promise, readbacks, transients, &impl] {
					// The GPU is done with them: their descriptors can be reused.
					for (auto& [id, buffer] : transients->buffers) impl.Release(buffer);
					transients->buffers.clear();
					transients->staging.clear();
					Readbacks out;
					out.reserve(readbacks->size());
					for (const auto& readback : *readbacks) {
						out.emplace_back(readback.buffer->mapped, readback.buffer->mapped + readback.bytes);
					}
					promise->set_value(std::move(out));
				});
		});
		return result;
	}

	void Compute::Finish()
	{
		auto& impl = _context.Impl();
		impl.Run([&] { impl.CompleteAllInFlight(); });
	}
}
