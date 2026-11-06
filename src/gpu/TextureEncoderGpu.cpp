#include "tesworld/TextureEncoderGpu.h"

#include "ClientQueue.h"
#include "ContextImpl.h"
#include "ImageFrames.h"

#include "tesworld/Profiling.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>
#include <vector>

namespace tesworld::texture
{
	namespace
	{
		using gpu::detail::ContextImpl;
		using gpu::detail::ImageBuffers;
		using gpu::detail::ImageLayout;
		using gpu::detail::ImagePrograms;
		using gpu::detail::PlacedImage;

		// A frame takes chains until they reach this many texels (a 4096x4096 chain is 22M); one larger goes alone.
		constexpr std::uint64_t kFrameTexels = 32ull << 20;
		constexpr std::size_t kFrameSets = 3;
		// What the frames may take of the adapter's own memory.
		constexpr double kMemoryShare = 0.7;

		struct Job
		{
			EncodeRequest request;
			TextureEncoder::Callback done;
		};

		struct Placed
		{
			Job job;
			PlacedImage image;
		};

		struct FrameSet
		{
			ImageBuffers buffers;
			std::vector<Placed> placed;
			bool busy{ false };
		};

		class GpuTextureEncoder final : public TextureEncoder, public gpu::detail::Client
		{
		public:
			GpuTextureEncoder(gpu::Context& a_context, Completion a_completion) :
				_context(a_context),
				_gpu(a_context.Impl()),
				_cpu(CreateCpuTextureEncoder()),
				_completion(a_completion),
				_queue(_gpu, kFrameSets, [this](Job a_job) { EncodeOnCpu(std::move(a_job)); }, a_completion == Completion::ContextThread)
			{
				// Each frame set's scratch, blocks and readback on the device (the upload is host memory).
				const auto frameBytes = static_cast<double>(kFrameTexels) * (4 + 1 + 1) * 4 / 3;
				const auto memory = static_cast<double>(_context.DedicatedMemory());
				if (memory != 0 && frameBytes * kFrameSets > memory * kMemoryShare) {
					throw gpu::UnsupportedError(_context.Describe() + " has too little memory to encode textures");
				}
				_gpu.Run([&] { _images.emplace(_gpu); });
				_gpu.AddClient(*this);
			}

			~GpuTextureEncoder() noexcept override
			{
				Wait();
				_gpu.RemoveClient(*this);
				if (!_queue.Failed()) {
					try {
						_gpu.Run([&] {
							_gpu.CompleteAllInFlight();
							for (const auto& frame : _queue.Frames()) _images->Release(frame->buffers);
						});
					} catch (...) {
						// The context failed meanwhile: it releases everything itself.
					}
				}
			}

			void Encode(EncodeRequest a_request, Callback a_done) override
			{
				if (a_request.format != Format::BC7 || a_request.chain.Empty()) {
					if (_completion == Completion::ContextThread) {
						EncodeResult result;
						if (auto dds = EncodeDds(a_request.chain, a_request.format, &result.error)) result.dds = std::move(*dds);
						a_done(std::move(result));
						return;
					}
					_cpu->Encode(std::move(a_request), std::move(a_done));
					return;
				}
				_queue.Push({ std::move(a_request), std::move(a_done) });
			}

			void Wait() override
			{
				TW_ZONE("GPU texture encode wait");
				_queue.Wait();
				_cpu->Wait();
			}

			[[nodiscard]] std::string Backend() const override { return "GPU, " + _context.Describe(); }

			[[nodiscard]] std::string Problem() const override
			{
				const auto failure = _queue.Failure();
				return failure.empty() ? std::string() : "the GPU failed (" + failure + "); the rest was encoded on the CPU";
			}

			// ---- Render thread ----

			bool Pump() override
			{
				auto& waiting = _queue.Waiting();
				if (waiting.empty()) return false;
				auto* frame = _queue.AcquireFrame();
				if (!frame) return false;
				TakeFrame(*frame, waiting);
				Record(*frame);
				return true;
			}

			void Fail(const std::string& a_reason) override { _queue.Fail(a_reason); }

		private:
			// Takes chains in order into the frame, places them and copies their levels into its upload buffer.
			void TakeFrame(FrameSet& a_frame, std::deque<Job>& a_waiting)
			{
				TW_ZONE("GPU texture frame take");
				ImageLayout layout;
				while (!a_waiting.empty()) {
					const auto& chain = a_waiting.front().request.chain;
					std::uint64_t texels = 0;
					for (const auto& level : chain.levels) texels += static_cast<std::uint64_t>(level.width) * level.height;
					if (!a_frame.placed.empty() && layout.Texels() + texels > kFrameTexels) break;
					Placed placed{ .job = std::move(a_waiting.front()) };
					a_waiting.pop_front();
					const auto& taken = placed.job.request;
					placed.image = layout.Place(taken.chain.Width(), taken.chain.Height(), static_cast<std::uint32_t>(taken.chain.levels.size()), taken.format, true);
					a_frame.placed.push_back(std::move(placed));
				}
				_images->Ensure(a_frame.buffers, layout, "tesworld texture encode");
				for (const auto& placed : a_frame.placed) ImagePrograms::Stage(a_frame.buffers, placed.image, placed.job.request.chain);
			}

			void Record(FrameSet& a_frame)
			{
				TW_ZONE("GPU texture frame");
				a_frame.busy = true;
				_gpu.ExecuteFrame(
					[&](rhi::CommandList& a_commands) {
						std::vector<const PlacedImage*> images;
						for (const auto& placed : a_frame.placed) images.push_back(&placed.image);
						_images->RecordUploads(a_commands, a_frame.buffers, images);
						_images->RecordBlocks(a_commands, a_frame.buffers, images);
						_images->RecordReadback(a_commands, a_frame.buffers, images);
					},
					[this, &a_frame] { Complete(a_frame); });
			}

			// Render thread: the frame's bytes copied out (the readback is reused), each request finished in a task.
			void Complete(FrameSet& a_frame)
			{
				TW_ZONE("GPU texture frame complete");
				for (auto& placed : a_frame.placed) {
					auto bytes = ImagePrograms::Read(a_frame.buffers, placed.image);
					// The chain was only kept for the CPU, had the GPU failed.
					_queue.Run([this, done = std::move(placed.job.done), image = std::move(placed.image), bytes = std::move(bytes)] {
						done(gpu::detail::Wrap(image, bytes));
						_queue.Finished();
					});
				}
				a_frame.placed.clear();
				a_frame.busy = false;
			}

			void EncodeOnCpu(Job a_job)
			{
				auto done = std::move(a_job.done);
				if (_completion == Completion::ContextThread) {
					// The caller encodes it itself: the CPU encoder's own tasks are what it can't wait on.
					done(EncodeResult{ .error = "the GPU failed (" + _queue.Failure() + ")" });
					_queue.Finished();
					return;
				}
				_cpu->Encode(std::move(a_job.request), [this, done = std::move(done)](EncodeResult a_result) {
					done(std::move(a_result));
					_queue.Finished();
				});
			}

			gpu::Context& _context;
			ContextImpl& _gpu;
			std::unique_ptr<TextureEncoder> _cpu;
			Completion _completion;

			// Render thread only.
			std::optional<ImagePrograms> _images;

			// Last, so it's gone first: its tasks finish jobs with the rest.
			gpu::detail::ClientQueue<Job, FrameSet> _queue;
		};
	}

	std::unique_ptr<TextureEncoder> CreateGpuTextureEncoder(gpu::Context& a_context, Completion a_completion)
	{
		return std::make_unique<GpuTextureEncoder>(a_context, a_completion);
	}
}