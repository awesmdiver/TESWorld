#include "ContextImpl.h"

#include "Layout.h"

#include "tesworld/Profiling.h"

#if TESWORLD_ORG_GRAPH_HOST
#include <OpenRenderGraph/GraphHost.h>
#include <OpenRenderGraph/Runtime.h>
#else
#include <OpenRenderGraph/PersistentGraphHost.h>
#endif
#include <Render/Runtime/RuntimeDevice.h>
#include <Render/Runtime/ThreadPoolTaskService.h>
#include <RenderPasses/Base/TypedRenderGraphPass.h>
#include <Resources/Buffers/Buffer.h>
#include <rhi_feature_info.h>
#include <rhi_helpers.h>
#include <spdlog/sinks/stdout_sinks.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace tesworld::gpu
{
	namespace detail
	{
		namespace
		{
			// A raw view per buffer: the terrain bake holds a few per material and frame set.
			constexpr std::uint32_t kDescriptorCapacity = 1u << 14;
			// Render and depth targets alive at once.
			constexpr std::uint32_t kTargetCapacity = 32;
			// Samplers callers create (Compute::CreateSampler); they're never freed.
			constexpr std::uint32_t kSamplerCapacity = 16;

			void Check(rhi::Result a_result, const char* a_what)
			{
				if (!rhi::IsOk(a_result)) {
					throw std::runtime_error(std::string("GPU: ") + a_what + " failed (rhi::Result " + std::to_string(static_cast<std::uint32_t>(a_result)) + ")");
				}
			}

			std::string Environment(const char* a_name)
			{
#if defined(_WIN32)
				char* value = nullptr;
				std::size_t length = 0;
				if (_dupenv_s(&value, &length, a_name) != 0 || !value) return {};
				std::string result(value);
				std::free(value);
				return result;
#else
				const char* value = std::getenv(a_name);
				return value ? std::string(value) : std::string();
#endif
			}

			std::string Lower(std::string a_text)
			{
				std::ranges::transform(a_text, a_text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
				return a_text;
			}

			// Vulkan lists software rasterizers as devices (D3D12 skips WARP itself): their compute is far slower than the CPU
			// path's.
			bool IsSoftwareAdapter(const AdapterFeatureInfo& a_adapter)
			{
				const auto name = Lower(a_adapter.name);
				return a_adapter.vendorId == 0x10005 || name.find("llvmpipe") != std::string::npos || name.find("lavapipe") != std::string::npos ||
				       name.find("swiftshader") != std::string::npos || name.find("microsoft basic render") != std::string::npos;
			}
		}

		struct PassContext
		{
			rhi::DescriptorHeapHandle heap{};
			rhi::DescriptorHeapHandle samplerHeap{};
			const std::function<void(rhi::CommandList&)>* record{ nullptr };
		};

		namespace
		{
			struct ComputeFrame
			{
				PassContext context;
			};

			struct ComputeBindings
			{
			};

			// Records whatever the current frame's callback records. All synchronisation inside the frame is the callback's;
			// the graph only orders whole frames.
			class ComputePass final : public org::TypedRenderGraphPass<ComputePass, ComputeFrame, ComputeBindings>
			{
			public:
				ComputePass(std::shared_ptr<PassContext> a_context, std::shared_ptr<org::Buffer> a_heartbeat) :
					_context(std::move(a_context)), _heartbeat(std::move(a_heartbeat)) {}

				ComputeBindings Declare(org::PassBuilder& a_builder)
				{
					a_builder.PreferQueue(org::QueueKind::Graphics);
					a_builder.UnorderedAccess(_heartbeat);
					return {};
				}

				ComputeFrame Prepare(const ComputeBindings&, const org::PassPrepareContext&) const { return ComputeFrame{ .context = *_context }; }

				static void Record(const ComputeBindings&, const ComputeFrame& a_frame, org::PassRecordContext& a_recording)
				{
					if (!a_frame.context.record) {
						return;
					}
					auto& commands = a_recording.Commands();
					// Earlier frames' copies and dispatches may still touch what this frame does.
					ContextImpl::Barrier(commands, rhi::ResourceSyncState::All, rhi::ResourceAccessType::Common, rhi::ResourceSyncState::All,
						rhi::ResourceAccessType::Common);
					commands.SetDescriptorHeaps(a_frame.context.heap, a_frame.context.samplerHeap);
					(*a_frame.context.record)(commands);
				}

			private:
				std::shared_ptr<PassContext> _context;
				std::shared_ptr<org::Buffer> _heartbeat;
			};

			class ComputeExtension final : public org::RenderGraph::IRenderGraphExtension
			{
			public:
				ComputeExtension(std::shared_ptr<PassContext> a_context, std::shared_ptr<org::Buffer> a_heartbeat) :
					_context(std::move(a_context)), _heartbeat(std::move(a_heartbeat)) {}

				void PrepareForBuild(org::RenderGraph& a_graph) override { a_graph.RegisterResource(org::ResourceIdentifier("tesworld.heartbeat"), _heartbeat); }

				void GatherStructuralPasses(org::RenderGraph&, std::vector<org::RenderGraph::ExternalPassDesc>& a_out) override
				{
					a_out.push_back(org::RenderGraph::ExternalPassDesc::Compute("tesworld.compute",
						std::static_pointer_cast<org::RenderPass>(std::make_shared<ComputePass>(_context, _heartbeat)))
							.PreferQueue(org::QueueKind::Graphics));
				}

			private:
				std::shared_ptr<PassContext> _context;
				std::shared_ptr<org::Buffer> _heartbeat;
			};
		}

		ContextImpl::ContextImpl(ContextDesc a_desc) :
			_desc(a_desc)
		{
			if (_desc.quietLogging) {
				auto logger = std::make_shared<spdlog::logger>("tesworld-gpu", std::make_shared<spdlog::sinks::stderr_sink_mt>());
				logger->set_level(spdlog::level::warn);
				spdlog::set_default_logger(std::move(logger));
			}
			if (const auto failAfter = Environment("TESWORLD_GPU_FAIL_AFTER"); !failAfter.empty()) {
				_failAfter = std::strtoull(failAfter.c_str(), nullptr, 10);
			}
			// Startup is the only time a caller waits on the render thread for nothing it asked.
			std::promise<void> ready;
			auto started = ready.get_future();
			_thread = std::thread([this, &ready] {
				TW_THREAD_NAME("tesworld GPU");
				try {
					Initialize();
					ready.set_value();
				} catch (...) {
					Shutdown();
					ready.set_exception(std::current_exception());
					return;
				}
				Loop();
				Shutdown();
			});
			try {
				started.get();
			} catch (...) {
				_thread.join();
				throw;
			}
		}

		ContextImpl::~ContextImpl()
		{
			_stopping.store(true, std::memory_order_release);
			Wake();
			if (_thread.joinable()) {
				_thread.join();
			}
		}

		void ContextImpl::Wake()
		{
			_signal.fetch_add(1, std::memory_order_release);
			_signal.notify_one();
		}

		void ContextImpl::Run(const std::function<void()>& a_task)
		{
			auto done = std::make_shared<std::promise<void>>();
			auto finished = done->get_future();
			_inbox.Push(Task{ a_task, done });
			Wake();
			finished.get();
		}

		void ContextImpl::AddClient(Client& a_client)
		{
			Run([&] { _clients.push_back(&a_client); });
		}

		void ContextImpl::RemoveClient(Client& a_client)
		{
			// Once the thread has failed, Run refuses work: the client is dropped by the caller's destruction anyway.
			if (_failed.load(std::memory_order_acquire)) {
				return;
			}
			Run([&] { std::erase(_clients, &a_client); });
		}

		void ContextImpl::Initialize()
		{
			std::vector<Api> apis;
			auto forced = Lower(Environment("TESWORLD_GPU_API"));
			if (forced == "d3d12") {
				apis = { Api::D3D12 };
			} else if (forced == "vulkan") {
				apis = { Api::Vulkan };
			} else if (_desc.api) {
				apis = { *_desc.api };
			} else {
#if defined(_WIN32)
				apis = { Api::D3D12, Api::Vulkan };
#else
				apis = { Api::Vulkan };
#endif
			}
			std::string reasons;
			for (const auto api : apis) {
				try {
					CreateDevice(api);
					break;
				} catch (const UnsupportedError& e) {
					reasons += (reasons.empty() ? "" : "; ") + std::string(e.what());
					_device.Reset();
				}
			}
			if (!_device) {
				throw UnsupportedError(reasons.empty() ? std::string("no GPU API to try") : reasons);
			}

			rhi::DescriptorHeapDesc heapDesc{ rhi::DescriptorHeapType::CbvSrvUav, kDescriptorCapacity, true, "tesworld descriptors" };
			Check(_device->CreateDescriptorHeap(heapDesc, _heap), "descriptor heap");
			// Bindless root signatures index the sampler heap too, so one must be bound.
			rhi::DescriptorHeapDesc samplerDesc{ rhi::DescriptorHeapType::Sampler, kSamplerCapacity, true, "tesworld samplers" };
			Check(_device->CreateDescriptorHeap(samplerDesc, _samplerHeap), "sampler heap");
			rhi::DescriptorHeapDesc rtvDesc{ rhi::DescriptorHeapType::RTV, kTargetCapacity, false, "tesworld render targets" };
			Check(_device->CreateDescriptorHeap(rtvDesc, _rtvHeap), "render target heap");
			rhi::DescriptorHeapDesc dsvDesc{ rhi::DescriptorHeapType::DSV, kTargetCapacity, false, "tesworld depth targets" };
			Check(_device->CreateDescriptorHeap(dsvDesc, _dsvHeap), "depth target heap");
			for (std::uint32_t i = kTargetCapacity; i-- > 0;) {
				_freeRtvs.push_back(i);
				_freeDsvs.push_back(i);
			}

#if TESWORLD_ORG_GRAPH_HOST
			_runtime = org::Runtime::Create(org::Runtime::Desc{
				.device = _device.Get(),
				.backend = _backend,
				.slots = 2,
				.tasks = std::make_shared<org::runtime::ThreadPoolTaskService>(2),
			});
			_host = std::make_unique<org::GraphHost>(org::GraphHost::Desc{
				.runtime = _runtime,
				.framesInFlight = 2,
				.epochOrder = {},
			});
#else
			_host = std::make_unique<org::PersistentGraphHost>(org::PersistentGraphHost::Desc{
				.device = _device.Get(),
				.backend = _backend,
				.tasks = std::make_shared<org::runtime::ThreadPoolTaskService>(2),
				.framesInFlight = 2,
				.epochOrder = {},
			});
#endif
			_heartbeat = org::Buffer::CreateSharedUnmaterialized(rhi::HeapType::DeviceLocal, 256, true);
			org::BufferBase::DescriptorRequirements requirements{};
			requirements.createUAV = true;
			requirements.uavDesc = { .dimension = rhi::UavDim::Buffer,
				.formatOverride = rhi::Format::R32_Typeless,
				.buffer = { .kind = rhi::BufferViewKind::Raw, .firstElement = 0, .numElements = 64 } };
			_heartbeat->SetDescriptorRequirements(requirements);
			_heartbeat->SetName("tesworld.heartbeat");
			_passContext = std::make_shared<PassContext>();
			_passContext->heap = _heap->GetHandle();
			_passContext->samplerHeap = _samplerHeap->GetHandle();
			auto context = _passContext;
			auto heartbeat = _heartbeat;
			_host->AddExtension("tesworld", [context, heartbeat] { return std::make_unique<ComputeExtension>(context, heartbeat); });
			_host->SetCompletedFrameCallback([this](std::uint64_t a_frame, const org::runtime::IStatisticsService&) { OnFrameCompleted(a_frame); });
		}

		// Everything the kernels need, checked up front so an unsuitable adapter falls back to the CPU before any work is
		// posted rather than failing part way.
		void ContextImpl::CreateDevice(Api a_api)
		{
			_backend = a_api == Api::D3D12 ? rhi::Backend::D3D12 : rhi::Backend::Vulkan;
			rhi::DeviceCreateInfo create{};
			create.backend = _backend;
			create.framesInFlight = 2;
			create.enableDebug = _desc.debugLayer;
			rhi::Result created = rhi::Result::Unsupported;
#if defined(_WIN32)
			if (a_api == Api::D3D12) {
				created = rhi::CreateD3D12Device(create, _device);
			}
#endif
			if (a_api == Api::Vulkan) {
				created = rhi::CreateVulkanDevice(create, _device);
			}
			if (!rhi::IsOk(created) || !_device) {
				throw UnsupportedError(std::string("no ") + ApiName(a_api) + " device could be created");
			}

			AdapterFeatureInfo adapter{};
			ShaderFeatureInfo shader{};
			adapter.header.pNext = &shader.header;
			Check(_device->QueryFeatureInfo(&adapter.header), "feature query");
			const bool vulkan = a_api == Api::Vulkan;
			std::vector<std::string> missing;
			if (IsSoftwareAdapter(adapter)) {
				missing.emplace_back("hardware (it's a software rasterizer)");
			}
			if (shader.maxShaderModel < ShaderModel::SM_6_6) {
				missing.emplace_back("shader model 6.6");
			}
			if (!shader.unifiedResourceHeaps || !shader.unboundedDescriptorTables) {
				missing.emplace_back(vulkan ? "bindless descriptor heaps (VK_EXT_descriptor_heap)" : "bindless descriptor heaps (resource binding tier 3)");
			}
			if (!missing.empty()) {
				std::string list;
				for (const auto& item : missing) {
					list += (list.empty() ? "" : ", ") + item;
				}
				_device.Reset();
				throw UnsupportedError(std::string(adapter.name) + " (" + ApiName(a_api) + ") lacks " + list);
			}
			_api = a_api;
			_adapter = adapter.name;
			_dedicatedMemory = adapter.dedicatedVideoMemory;
		}

		void ContextImpl::Shutdown()
		{
			if (_device) {
				_device->WaitIdle();
			}
			_inFlight.clear();
			// ORG resources (the heartbeat buffer) must be gone before the host's destructor shuts the runtime device down.
			if (_host) {
				_host->RemoveExtension("tesworld");
				_host->DestroyGraph();
			}
			_heartbeat.reset();
			_passContext.reset();
			_heap.Reset();
			_samplerHeap.Reset();
			_rtvHeap.Reset();
			_dsvHeap.Reset();
			_host.reset();
#if TESWORLD_ORG_GRAPH_HOST
			_runtime.reset();
#endif
			_device.Reset();
		}

		rhi::Device ContextImpl::Device()
		{
			return _device.Get();
		}

		ComputeProgram ContextImpl::CreateProgram(std::string_view a_shader, std::string_view a_entry, std::uint32_t a_constantWords)
		{
			const auto name = std::string(a_shader) + (_api == Api::D3D12 ? ".dxil" : ".spv");
			const auto bytes = EmbeddedShader(name);
			if (bytes.empty()) {
				throw UnsupportedError("this build has no " + name);
			}
			return CreateProgram(bytes, a_entry, a_constantWords);
		}

		ComputeProgram ContextImpl::CreateProgram(std::span<const std::byte> a_bytes, std::string_view a_entry, std::uint32_t a_constantWords)
		{
			if (a_bytes.empty()) {
				throw UnsupportedError(std::string("no ") + ApiName(_api) + " bytecode for " + std::string(a_entry));
			}
			ComputeProgram program;
			program.constantWords = a_constantWords;
			rhi::PushConstantRangeDesc constants{};
			constants.visibility = rhi::ShaderStage::Compute;
			constants.num32BitValues = a_constantWords;
			constants.set = 0;
			constants.binding = 0;
			Check(_device->CreatePipelineLayout(rhi::PipelineLayoutDesc{ .pushConstants = { &constants, 1 }, .flags = rhi::PipelineLayoutFlags::PF_None },
					  program.layout),
				"pipeline layout");
			rhi::SubobjLayout layout{ program.layout->GetHandle() };
			rhi::SubobjShader shader{ rhi::ShaderStage::Compute, { a_bytes.data(), static_cast<std::uint32_t>(a_bytes.size()) }, std::string(a_entry) };
			const rhi::PipelineStreamItem items[] = { rhi::Make(layout), rhi::Make(shader) };
			Check(_device->CreatePipeline(items, 2, program.pipeline), "compute pipeline");
			return program;
		}

		GraphicsProgram ContextImpl::CreateGraphicsProgram(const GraphicsProgramDesc& a_desc)
		{
			const auto extension = std::string(_api == Api::D3D12 ? ".dxil" : ".spv");
			const auto vertexName = std::string(a_desc.shader) + ".VS" + extension, pixelName = std::string(a_desc.shader) + ".PS" + extension;
			const auto vertexBytes = EmbeddedShader(vertexName), pixelBytes = EmbeddedShader(pixelName);
			if (vertexBytes.empty() || pixelBytes.empty()) {
				throw UnsupportedError("this build has no " + (vertexBytes.empty() ? vertexName : pixelName));
			}
			GraphicsProgram program;
			program.constantWords = a_desc.constantWords;
			rhi::PushConstantRangeDesc constants{};
			constants.visibility = rhi::ShaderStage::AllGraphics;
			constants.num32BitValues = a_desc.constantWords;
			constants.set = 0;
			constants.binding = 0;
			Check(_device->CreatePipelineLayout(rhi::PipelineLayoutDesc{ .pushConstants = { &constants, 1 }, .flags = rhi::PipelineLayoutFlags::PF_None },
					  program.layout),
				"graphics pipeline layout");
			rhi::SubobjLayout layout{ program.layout->GetHandle() };
			rhi::SubobjShader vertex{ rhi::ShaderStage::Vertex, { vertexBytes.data(), static_cast<std::uint32_t>(vertexBytes.size()) }, std::string(a_desc.vertexEntry) };
			rhi::SubobjShader pixel{ rhi::ShaderStage::Pixel, { pixelBytes.data(), static_cast<std::uint32_t>(pixelBytes.size()) }, std::string(a_desc.pixelEntry) };
			rhi::SubobjRaster raster{};
			raster.rs.cull = rhi::CullMode::None;
			rhi::SubobjDepth depth{};
			depth.ds.depthEnable = a_desc.depth != rhi::Format::Unknown;
			depth.ds.depthWrite = depth.ds.depthEnable;
			depth.ds.depthFunc = rhi::CompareOp::Less;
			rhi::SubobjRTVs targets{};
			targets.rt.count = static_cast<std::uint32_t>(a_desc.colors.size());
			for (std::size_t i = 0; i < a_desc.colors.size(); ++i) targets.rt.formats[i] = a_desc.colors[i];
			rhi::SubobjBlend blend{};
			blend.bs.numAttachments = targets.rt.count;
			rhi::SubobjDSV depthFormat{ a_desc.depth };
			rhi::SubobjPrimitiveTopology topology{ rhi::PrimitiveTopology::TriangleList };
			const rhi::PipelineStreamItem items[] = { rhi::Make(layout), rhi::Make(vertex), rhi::Make(pixel), rhi::Make(raster), rhi::Make(depth),
				rhi::Make(blend), rhi::Make(targets), rhi::Make(depthFormat), rhi::Make(topology) };
			Check(_device->CreatePipeline(items, static_cast<std::uint32_t>(std::size(items)), program.pipeline), "graphics pipeline");
			return program;
		}

		DeviceTexture ContextImpl::CreateTarget(std::uint32_t a_width, std::uint32_t a_height, rhi::Format a_format, bool a_depth, const char* a_name)
		{
			DeviceTexture texture{ .width = a_width, .height = a_height, .format = a_format };
			rhi::ClearValue clear{};
			clear.format = a_format;
			if (a_depth) {
				clear.type = rhi::ClearValueType::DepthStencil;
				clear.depthStencil.depth = 1.0f;
			} else {
				clear.rgba[0] = clear.rgba[1] = clear.rgba[2] = clear.rgba[3] = 0.0f;
			}
			Check(_device->CreateCommittedResource(rhi::helpers::ResourceDesc::Tex2D(a_format, rhi::HeapType::DeviceLocal, a_width, a_height, 1, 1, 1,
						  rhi::ResourceLayout::Undefined, &clear, a_depth ? rhi::RF_AllowDepthStencil : rhi::RF_AllowRenderTarget, a_name),
					  texture.resource),
				a_name);
			if (a_depth) {
				if (_freeDsvs.empty()) throw std::runtime_error("GPU: depth target views exhausted");
				texture.dsv = _freeDsvs.back();
				_freeDsvs.pop_back();
				rhi::DsvDesc dsv{};
				dsv.dimension = rhi::DsvDim::Texture2D;
				dsv.formatOverride = a_format;
				Check(_device->CreateDepthStencilView(DsvSlot(texture.dsv), texture.resource->GetHandle(), dsv), "depth target view");
				return texture;
			}
			if (_freeRtvs.empty()) throw std::runtime_error("GPU: render target views exhausted");
			texture.rtv = _freeRtvs.back();
			_freeRtvs.pop_back();
			Check(_device->CreateRenderTargetView(RtvSlot(texture.rtv), texture.resource->GetHandle(), rhi::RtvDesc{}), "render target view");
			texture.srv = AllocateDescriptor();
			rhi::SrvDesc srv{};
			srv.dimension = rhi::SrvDim::Texture2D;
			srv.formatOverride = a_format;
			srv.tex2D.mostDetailedMip = 0;
			srv.tex2D.mipLevels = 1;
			Check(_device->CreateShaderResourceView({ _heap->GetHandle(), texture.srv }, texture.resource->GetHandle(), srv), "render target SRV");
			return texture;
		}

		void ContextImpl::Release(DeviceTexture& a_texture)
		{
			if (a_texture.srv != layout::kNone) _freeDescriptors.push_back(a_texture.srv);
			if (a_texture.rtv != layout::kNone) _freeRtvs.push_back(a_texture.rtv);
			if (a_texture.dsv != layout::kNone) _freeDsvs.push_back(a_texture.dsv);
			a_texture = {};
		}

		rhi::DescriptorSlot ContextImpl::RtvSlot(std::uint32_t a_index) const { return { _rtvHeap->GetHandle(), a_index }; }

		rhi::DescriptorSlot ContextImpl::DsvSlot(std::uint32_t a_index) const { return { _dsvHeap->GetHandle(), a_index }; }

		MappedBuffer ContextImpl::CreateMapped(std::uint64_t a_bytes, rhi::HeapType a_heap, const char* a_name)
		{
			MappedBuffer buffer;
			buffer.bytes = (std::max<std::uint64_t>)(a_bytes, 256);
			Check(_device->CreateCommittedResource(rhi::helpers::ResourceDesc::Buffer(buffer.bytes, a_heap, rhi::RF_None, a_name), buffer.resource), a_name);
			void* mapped = nullptr;
			buffer.resource->Map(&mapped, 0, a_heap == rhi::HeapType::Readback ? buffer.bytes : 0);
			buffer.mapped = static_cast<std::byte*>(mapped);
			return buffer;
		}

		std::uint32_t ContextImpl::AllocateDescriptor()
		{
			if (!_freeDescriptors.empty()) {
				const auto slot = _freeDescriptors.back();
				_freeDescriptors.pop_back();
				return slot;
			}
			if (_nextDescriptor >= kDescriptorCapacity) {
				throw std::runtime_error("GPU: descriptor heap exhausted");
			}
			return _nextDescriptor++;
		}

		DeviceBuffer ContextImpl::CreateDeviceBuffer(std::uint64_t a_bytes, bool a_srv, bool a_uav, const char* a_name)
		{
			DeviceBuffer buffer;
			buffer.bytes = (a_bytes + 255) / 256 * 256;
			Check(_device->CreateCommittedResource(
					  rhi::helpers::ResourceDesc::Buffer(buffer.bytes, rhi::HeapType::DeviceLocal, a_uav ? rhi::RF_AllowUnorderedAccess : rhi::RF_None, a_name),
					  buffer.resource),
				a_name);
			if (a_srv) {
				buffer.srv = AllocateDescriptor();
				rhi::SrvDesc srv{};
				srv.dimension = rhi::SrvDim::Buffer;
				srv.formatOverride = rhi::Format::R32_Typeless;
				srv.buffer.kind = rhi::BufferViewKind::Raw;
				srv.buffer.numElements = static_cast<std::uint32_t>(buffer.bytes / 4);
				Check(_device->CreateShaderResourceView({ _heap->GetHandle(), buffer.srv }, buffer.resource->GetHandle(), srv), "raw SRV");
			}
			if (a_uav) {
				buffer.uav = AllocateDescriptor();
				rhi::UavDesc uav{};
				uav.dimension = rhi::UavDim::Buffer;
				uav.formatOverride = rhi::Format::R32_Typeless;
				uav.buffer.kind = rhi::BufferViewKind::Raw;
				uav.buffer.numElements = static_cast<std::uint32_t>(buffer.bytes / 4);
				Check(_device->CreateUnorderedAccessView({ _heap->GetHandle(), buffer.uav }, buffer.resource->GetHandle(), uav), "raw UAV");
			}
			return buffer;
		}

		void ContextImpl::Release(DeviceBuffer& a_buffer)
		{
			if (a_buffer.srv != layout::kNone) _freeDescriptors.push_back(a_buffer.srv);
			if (a_buffer.uav != layout::kNone) _freeDescriptors.push_back(a_buffer.uav);
			a_buffer = {};
		}

		DeviceBuffer ContextImpl::Upload(std::span<const std::byte> a_bytes, const char* a_name)
		{
			auto buffer = CreateDeviceBuffer(a_bytes.size(), true, false, a_name);
			auto staging = std::make_shared<MappedBuffer>(CreateMapped(a_bytes.size(), rhi::HeapType::Upload, a_name));
			std::memcpy(staging->mapped, a_bytes.data(), a_bytes.size());
			const auto destination = buffer.resource->GetHandle();
			const auto source = staging->resource->GetHandle();
			const auto bytes = a_bytes.size();
			ExecuteFrame([&](rhi::CommandList& a_commands) { a_commands.CopyBufferRegion(destination, 0, source, 0, bytes); }, [staging] {});
			CompleteAllInFlight();
			return buffer;
		}

		SampledTexture ContextImpl::UploadTexture(rhi::Format a_format, std::span<const TextureLevel> a_levels, const char* a_name)
		{
			if (a_levels.empty() || a_levels.size() > 16) {
				throw std::invalid_argument("GPU: a texture needs 1 to 16 mips");
			}
			const auto mips = static_cast<std::uint32_t>(a_levels.size());
			SampledTexture texture{ .width = a_levels[0].width, .height = a_levels[0].height, .mips = mips };
			Check(_device->CreateCommittedResource(rhi::helpers::ResourceDesc::Tex2D(a_format, rhi::HeapType::DeviceLocal, texture.width, texture.height,
						  static_cast<std::uint16_t>(mips), 1, 1, rhi::ResourceLayout::Undefined, nullptr, rhi::RF_None, a_name),
					  texture.resource),
				a_name);
			const auto handle = texture.resource->GetHandle();
			std::vector<rhi::CopyableFootprint> footprints(mips);
			const auto placed = _device->GetCopyableFootprints(rhi::FootprintRangeDesc{ .texture = handle, .firstMip = 0, .mipCount = mips }, footprints.data(), mips);
			if (placed.count != mips) {
				throw std::runtime_error(std::string("GPU: no copy footprints for ") + a_name);
			}
			auto staging = std::make_shared<MappedBuffer>(CreateMapped(placed.totalBytes, rhi::HeapType::Upload, a_name));
			for (std::uint32_t mip = 0; mip < mips; ++mip) {
				const auto& level = a_levels[mip];
				const auto& footprint = footprints[mip];
				if (level.width != footprint.width || level.height != footprint.height) {
					throw std::invalid_argument(std::string("GPU: ") + a_name + " mip " + std::to_string(mip) + " isn't half the one before");
				}
				if (level.rows == 0 || level.rowBytes > footprint.rowPitch ||
					level.bytes.size() < static_cast<std::uint64_t>(level.rows - 1) * level.rowPitch + level.rowBytes) {
					throw std::invalid_argument(std::string("GPU: ") + a_name + " mip " + std::to_string(mip) + " is smaller than its rows");
				}
				for (std::uint32_t row = 0; row < level.rows; ++row) {
					std::memcpy(staging->mapped + footprint.offset + static_cast<std::uint64_t>(row) * footprint.rowPitch,
						level.bytes.data() + static_cast<std::uint64_t>(row) * level.rowPitch, level.rowBytes);
				}
			}
			const auto source = staging->resource->GetHandle();
			ExecuteFrame(
				[&](rhi::CommandList& a_commands) {
					const rhi::TextureSubresourceRange all{ .baseMip = 0, .mipCount = mips };
					rhi::TextureBarrier toCopy{ .texture = handle,
						.range = all,
						.beforeSync = rhi::ResourceSyncState::None,
						.afterSync = rhi::ResourceSyncState::Copy,
						.beforeAccess = rhi::ResourceAccessType::None,
						.afterAccess = rhi::ResourceAccessType::CopyDest,
						.beforeLayout = rhi::ResourceLayout::Undefined,
						.afterLayout = rhi::ResourceLayout::CopyDest,
						.discard = true };
					a_commands.Barriers(rhi::BarrierBatch{ .textures = { &toCopy, 1 } });
					for (std::uint32_t mip = 0; mip < mips; ++mip) {
						rhi::BufferTextureCopyFootprint copy{};
						copy.texture = handle;
						copy.buffer = source;
						copy.mip = mip;
						copy.arraySlice = 0;
						copy.footprint = footprints[mip];
						a_commands.CopyBufferToTexture(copy);
					}
					rhi::TextureBarrier toRead{ .texture = handle,
						.range = all,
						.beforeSync = rhi::ResourceSyncState::Copy,
						.afterSync = rhi::ResourceSyncState::All,
						.beforeAccess = rhi::ResourceAccessType::CopyDest,
						.afterAccess = rhi::ResourceAccessType::ShaderResource,
						.beforeLayout = rhi::ResourceLayout::CopyDest,
						.afterLayout = rhi::ResourceLayout::ShaderResource };
					a_commands.Barriers(rhi::BarrierBatch{ .textures = { &toRead, 1 } });
				},
				[staging] {});
			CompleteAllInFlight();
			texture.srv = AllocateDescriptor();
			rhi::SrvDesc srv{};
			srv.dimension = rhi::SrvDim::Texture2D;
			srv.formatOverride = a_format;
			srv.tex2D.mostDetailedMip = 0;
			srv.tex2D.mipLevels = mips;
			Check(_device->CreateShaderResourceView({ _heap->GetHandle(), texture.srv }, handle, srv), "texture SRV");
			return texture;
		}

		void ContextImpl::Release(SampledTexture& a_texture)
		{
			if (a_texture.srv != layout::kNone) _freeDescriptors.push_back(a_texture.srv);
			a_texture = {};
		}

		std::uint32_t ContextImpl::CreateSampler(const rhi::SamplerDesc& a_desc)
		{
			if (_nextSampler >= kSamplerCapacity) {
				throw std::runtime_error("GPU: sampler heap exhausted");
			}
			const auto index = _nextSampler++;
			Check(_device->CreateSampler({ _samplerHeap->GetHandle(), index }, a_desc), "sampler");
			return index;
		}

		void ContextImpl::Barrier(rhi::CommandList& a_commands, rhi::ResourceSyncState a_beforeSync, rhi::ResourceAccessType a_beforeAccess,
			rhi::ResourceSyncState a_afterSync, rhi::ResourceAccessType a_afterAccess)
		{
			rhi::GlobalBarrier barrier{};
			barrier.beforeSync = a_beforeSync;
			barrier.beforeAccess = a_beforeAccess;
			barrier.afterSync = a_afterSync;
			barrier.afterAccess = a_afterAccess;
			rhi::BarrierBatch batch{};
			batch.globals = { &barrier, 1 };
			a_commands.Barriers(batch);
		}

		void ContextImpl::Dispatch(rhi::CommandList& a_commands, const ComputeProgram& a_program, const void* a_constants, std::uint32_t a_x,
			std::uint32_t a_y, std::uint32_t a_z)
		{
			a_commands.BindLayout(a_program.layout->GetHandle());
			a_commands.BindPipeline(a_program.pipeline->GetHandle());
			a_commands.PushConstants(rhi::ShaderStage::Compute, 0, 0, 0, a_program.constantWords, static_cast<const std::uint32_t*>(a_constants));
			a_commands.Dispatch(a_x, a_y, a_z);
		}

		void ContextImpl::Transition(rhi::CommandList& a_commands, const DeviceTexture& a_texture, rhi::ResourceSyncState a_beforeSync,
			rhi::ResourceAccessType a_beforeAccess, rhi::ResourceLayout a_beforeLayout, rhi::ResourceSyncState a_afterSync, rhi::ResourceAccessType a_afterAccess,
			rhi::ResourceLayout a_afterLayout)
		{
			rhi::TextureBarrier barrier{
				.texture = a_texture.resource->GetHandle(),
				.range = {},
				.beforeSync = a_beforeSync,
				.afterSync = a_afterSync,
				.beforeAccess = a_beforeAccess,
				.afterAccess = a_afterAccess,
				.beforeLayout = a_beforeLayout,
				.afterLayout = a_afterLayout,
				.discard = a_beforeLayout == rhi::ResourceLayout::Undefined,
			};
			a_commands.Barriers(rhi::BarrierBatch{ .textures = { &barrier, 1 } });
		}

		void ContextImpl::ExecuteFrame(std::function<void(rhi::CommandList&)> a_record, std::function<void()> a_completed)
		{
			TW_ZONE("GPU frame");
			if (_failAfter != 0 && _frames >= _failAfter) {
				throw std::runtime_error("GPU: failing as TESWORLD_GPU_FAIL_AFTER asks");
			}
			_passContext->record = &a_record;
			_host->ExecuteFrame();
			_passContext->record = nullptr;
			_inFlight.emplace(_host->LastHostFrame(), std::move(a_completed));
			++_frames;
		}

		void ContextImpl::OnFrameCompleted(std::uint64_t a_frame)
		{
			const auto it = _inFlight.find(a_frame);
			if (it == _inFlight.end()) {
				return;
			}
			auto completed = std::move(it->second);
			_inFlight.erase(it);
			if (completed) completed();
		}

		void ContextImpl::CompleteAllInFlight()
		{
			if (_inFlight.empty()) {
				return;
			}
			TW_ZONE("GPU wait");
			_device->WaitIdle();
			auto inFlight = std::move(_inFlight);
			_inFlight.clear();
			for (auto& [frame, completed] : inFlight) {
				if (completed) completed();
			}
		}

		void ContextImpl::Loop()
		{
			const auto runTasks = [&] {
				while (auto task = _inbox.TryPop()) {
					try {
						if (_failed.load(std::memory_order_acquire)) {
							throw std::runtime_error("GPU: " + _failure);
						}
						task->work();
						task->done->set_value();
					} catch (...) {
						task->done->set_exception(std::current_exception());
					}
				}
			};
			try {
				while (true) {
					const auto seen = _signal.load(std::memory_order_acquire);
					runTasks();
					bool worked = false;
					for (auto* client : std::vector<Client*>(_clients)) {
						worked = client->Pump() || worked;
					}
					if (worked) {
						continue;
					}
					if (!_inFlight.empty()) {
						// Nothing new to run: finish what is in flight instead of ticking empty frames.
						CompleteAllInFlight();
						continue;
					}
					if (_stopping.load(std::memory_order_acquire)) {
						return;
					}
					_signal.wait(seen, std::memory_order_acquire);
				}
			} catch (const std::exception& e) {
				_failure = e.what();
			} catch (...) {
				_failure = "the render thread failed";
			}
			_failed.store(true, std::memory_order_release);
			// Every client finishes what it owes (on the CPU), so no caller waits forever.
			_inFlight.clear();
			for (auto* client : _clients) {
				client->Fail(_failure);
			}
			_clients.clear();
			while (!_stopping.load(std::memory_order_acquire)) {
				const auto seen = _signal.load(std::memory_order_acquire);
				runTasks();
				if (_stopping.load(std::memory_order_acquire)) {
					break;
				}
				_signal.wait(seen, std::memory_order_acquire);
			}
			runTasks();
		}
	}

	const char* ApiName(Api a_api)
	{
		return a_api == Api::D3D12 ? "D3D12" : "Vulkan";
	}

	Context::Context(ContextDesc a_desc) :
		_impl(std::make_unique<detail::ContextImpl>(a_desc))
	{}

	Context::~Context() = default;

	Api Context::ApiInUse() const
	{
		return _impl->ApiInUse();
	}

	const std::string& Context::Adapter() const
	{
		return _impl->Adapter();
	}

	std::uint64_t Context::DedicatedMemory() const
	{
		return _impl->DedicatedMemory();
	}

	std::string Context::Describe() const
	{
		return _impl->Adapter() + " (" + ApiName(_impl->ApiInUse()) + ")";
	}

	bool Context::SelfTest()
	{
		constexpr std::uint32_t kCount = 4096;
		constexpr std::uint32_t kValue = 0x1234;
		bool ok = false;
		auto& impl = *_impl;
		impl.Run([&] {
			auto program = impl.CreateProgram("Fill", "FillCS", sizeof(layout::FillConstants) / 4);
			auto buffer = impl.CreateDeviceBuffer(kCount * 4, false, true, "tesworld self test");
			auto readback = impl.CreateMapped(kCount * 4, rhi::HeapType::Readback, "tesworld self test readback");
			const layout::FillConstants constants{ .buffer = buffer.uav, .count = kCount, .value = kValue, .pad = 0 };
			impl.ExecuteFrame(
				[&](rhi::CommandList& a_commands) {
					detail::ContextImpl::Dispatch(a_commands, program, &constants, (kCount + 63) / 64, 1, 1);
					detail::ContextImpl::Barrier(a_commands, rhi::ResourceSyncState::ComputeShading, rhi::ResourceAccessType::UnorderedAccess,
						rhi::ResourceSyncState::Copy, rhi::ResourceAccessType::CopySource);
					a_commands.CopyBufferRegion(readback.resource->GetHandle(), 0, buffer.resource->GetHandle(), 0, kCount * 4);
				},
				[&] {
					const auto* words = reinterpret_cast<const std::uint32_t*>(readback.mapped);
					ok = true;
					for (std::uint32_t i = 0; i < kCount; ++i) {
						ok = ok && words[i] == kValue + i;
					}
				});
			impl.CompleteAllInFlight();
			impl.Release(buffer);
		});
		return ok;
	}
}
