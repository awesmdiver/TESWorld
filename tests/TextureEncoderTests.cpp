// The tests check with assert, so it stays on in every configuration.
#undef NDEBUG

#include "tesworld/TextureEncoder.h"
#if defined(TESWORLD_TEST_GPU)
#include "tesworld/TextureEncoderGpu.h"
#endif

#include <oneapi/tbb/parallel_for.h>

#include <cassert>
#include <cmath>
#include <cstdlib>
#include <future>
#include <iostream>
#include <mutex>
#include <vector>

using namespace tesworld;
using namespace tesworld::texture;

namespace
{
	// An atlas page's worth: smooth colour, noise, and cutouts an alpha test keeps or drops. Not a power of two, and its
	// last row and column of blocks partial.
	MipChain AtlasLike(std::uint32_t a_width, std::uint32_t a_height, std::uint32_t a_seed, bool a_alpha)
	{
		Image image(a_width, a_height);
		std::uint32_t state = a_seed;
		for (std::uint32_t y = 0; y < a_height; ++y) {
			for (std::uint32_t x = 0; x < a_width; ++x) {
				state = state * 1664525u + 1013904223u;
				const float noise = static_cast<float>(state >> 24) / 255.0f - 0.5f;
				const float u = static_cast<float>(x) / a_width, v = static_cast<float>(y) / a_height;
				const bool cut = a_alpha && std::sin(static_cast<float>(x) * 0.11f) * std::cos(static_cast<float>(y) * 0.07f) > 0.4f;
				image.SetTexel(x, y, { 0.3f + 0.5f * u + 0.08f * noise, 0.6f - 0.3f * v + 0.05f * noise, 0.4f + 0.2f * std::sin(u * 9.0f), cut ? 0.0f : 1.0f });
			}
		}
		return BuildMips(std::move(image), a_alpha ? std::optional<float>(0.5f) : std::nullopt);
	}

	EncodeResult EncodeNow(TextureEncoder& a_encoder, const MipChain& a_chain, Format a_format)
	{
		EncodeResult result;
		a_encoder.Encode({ a_chain, a_format }, [&](EncodeResult a_result) { result = std::move(a_result); });
		a_encoder.Wait();
		return result;
	}

	// Of the top level, decoded, against the source: every channel.
	double Psnr(const MipChain& a_source, const std::vector<std::uint8_t>& a_dds)
	{
		const auto decoded = DecodeDds(a_dds);
		assert(decoded && decoded->levels.size() == a_source.levels.size());
		const auto& a = a_source.levels.front().rgba;
		const auto& b = decoded->levels.front().rgba;
		double squared = 0.0;
		for (std::size_t i = 0; i < a.size(); ++i) {
			const double delta = double(a[i]) - double(b[i]);
			squared += delta * delta;
		}
		return 10.0 * std::log10(255.0 * 255.0 / (squared / static_cast<double>(a.size())));
	}

	void CpuEncoderIsEncodeDds()
	{
		auto encoder = CreateCpuTextureEncoder();
		assert(encoder->Backend() == "CPU");
		std::vector<MipChain> chains;
		for (std::uint32_t i = 0; i < 6; ++i) chains.push_back(AtlasLike(200 + 30 * i, 120 + 7 * i, i, i % 2 == 0));
		std::mutex mutex;
		std::vector<EncodeResult> results(chains.size());
		for (std::size_t i = 0; i < chains.size(); ++i) {
			encoder->Encode({ chains[i], i % 3 == 0 ? Format::BC1 : Format::BC7 }, [&, i](EncodeResult a_result) {
				std::scoped_lock lock(mutex);
				results[i] = std::move(a_result);
			});
		}
		encoder->Wait();
		for (std::size_t i = 0; i < chains.size(); ++i) {
			assert(results[i].Ok());
			assert(results[i].dds == *EncodeDds(chains[i], i % 3 == 0 ? Format::BC1 : Format::BC7));
			// BC1 blackens what its alpha cuts out: its quality isn't the point.
			if (i % 3 != 0) assert(Psnr(chains[i], results[i].dds) > 30.0);
		}
		assert(!EncodeNow(*encoder, MipChain{}, Format::BC7).Ok());
	}

#if defined(TESWORLD_TEST_GPU)
	void SetFailAfter(const char* a_frames)
	{
#ifdef _WIN32
		_putenv_s("TESWORLD_GPU_FAIL_AFTER", a_frames);
#else
		setenv("TESWORLD_GPU_FAIL_AFTER", a_frames, 1);
#endif
	}

	void GpuEncoderMatchesTheCpusQuality()
	{
		try {
			gpu::Context context;
			std::cout << "GPU: " << context.Describe() << '\n';
			auto gpu = CreateGpuTextureEncoder(context);
			auto cpu = CreateCpuTextureEncoder();

			// BC7 as good as DirectXTex's quick mode, with and without alpha, and the same twice.
			for (const bool alpha : { true, false }) {
				const auto chain = AtlasLike(1030, 517, alpha ? 11u : 12u, alpha);
				const auto onGpu = EncodeNow(*gpu, chain, Format::BC7);
				const auto onCpu = EncodeNow(*cpu, chain, Format::BC7);
				assert(onGpu.Ok() && onCpu.Ok());
				assert(onGpu.dds.size() == onCpu.dds.size());
				const double gpuPsnr = Psnr(chain, onGpu.dds), cpuPsnr = Psnr(chain, onCpu.dds);
				std::cout << "BC7 PSNR" << (alpha ? " (alpha)" : "") << ": GPU " << gpuPsnr << " dB, CPU " << cpuPsnr << " dB\n";
				assert(gpuPsnr >= cpuPsnr - 0.5);
				assert(EncodeNow(*gpu, chain, Format::BC7).dds == onGpu.dds);
			}

			// Other formats are the CPU's.
			const auto small = AtlasLike(96, 64, 13, true);
			assert(EncodeNow(*gpu, small, Format::BC3).dds == *EncodeDds(small, Format::BC3));

			// Many at once, more than a frame holds: each the same as alone.
			std::vector<MipChain> chains;
			for (std::uint32_t i = 0; i < 5; ++i) chains.push_back(AtlasLike(4096, 2048 + 4 * i, 20 + i, i % 2 == 0));
			std::mutex mutex;
			std::vector<EncodeResult> results(chains.size());
			for (std::size_t i = 0; i < chains.size(); ++i) {
				gpu->Encode({ chains[i], Format::BC7 }, [&, i](EncodeResult a_result) {
					std::scoped_lock lock(mutex);
					results[i] = std::move(a_result);
				});
			}
			gpu->Wait();
			for (std::size_t i = 0; i < chains.size(); ++i) {
				assert(results[i].Ok() && results[i].dds == EncodeNow(*gpu, chains[i], Format::BC7).dds);
			}
			assert(gpu->Problem().empty());
		} catch (const gpu::UnsupportedError& e) {
			std::cout << "no GPU, its tests skipped: " << e.what() << '\n';
		}
	}

	// A GPU that fails part way: what it owed is encoded on the CPU, and said.
	void AFailingGpuHandsItsWorkToTheCpu()
	{
		SetFailAfter("1");
		try {
			gpu::Context context;
			auto gpu = CreateGpuTextureEncoder(context);
			std::vector<MipChain> chains;
			for (std::uint32_t i = 0; i < 6; ++i) chains.push_back(AtlasLike(2048, 2048, 30 + i, false));
			std::mutex mutex;
			std::vector<EncodeResult> results(chains.size());
			for (std::size_t i = 0; i < chains.size(); ++i) {
				gpu->Encode({ chains[i], Format::BC7 }, [&, i](EncodeResult a_result) {
					std::scoped_lock lock(mutex);
					results[i] = std::move(a_result);
				});
			}
			gpu->Wait();
			for (std::size_t i = 0; i < chains.size(); ++i) {
				assert(results[i].Ok());
				assert(Psnr(chains[i], results[i].dds) > 35.0);
			}
			std::cout << "after failing: " << gpu->Problem() << '\n';
			assert(!gpu->Problem().empty());
		} catch (const gpu::UnsupportedError& e) {
			std::cout << "no GPU, its tests skipped: " << e.what() << '\n';
		}
		SetFailAfter("");
	}

	// Finished on the context's thread: callers that block every TBB thread waiting on their encodes still get them,
	// the same bytes as the default's, and a GPU that fails says so rather than handing the work to TBB tasks.
	void ContextThreadCompletionNeedsNoTbbThread()
	{
		try {
			gpu::Context context;
			auto inlineGpu = CreateGpuTextureEncoder(context, Completion::ContextThread);
			std::vector<MipChain> chains;
			for (std::uint32_t i = 0; i < 24; ++i) chains.push_back(AtlasLike(256 + 64 * (i % 4), 256, 40 + i, i % 2 == 0));
			std::vector<EncodeResult> results(chains.size());
			oneapi::tbb::parallel_for(std::size_t{ 0 }, chains.size(), [&](std::size_t i) {
				std::promise<EncodeResult> promise;
				auto future = promise.get_future();
				inlineGpu->Encode({ chains[i], Format::BC7 }, [&promise](EncodeResult a_result) { promise.set_value(std::move(a_result)); });
				results[i] = future.get();
			});
			inlineGpu->Wait();
			auto tbbGpu = CreateGpuTextureEncoder(context);
			for (std::size_t i = 0; i < chains.size(); ++i) {
				assert(results[i].Ok() && results[i].dds == EncodeNow(*tbbGpu, chains[i], Format::BC7).dds);
			}
			const auto small = AtlasLike(96, 64, 13, true);
			assert(EncodeNow(*inlineGpu, small, Format::BC3).dds == *EncodeDds(small, Format::BC3));
			std::cout << "context-thread completion: " << chains.size() << " encodes from blocked TBB threads\n";
		} catch (const gpu::UnsupportedError& e) {
			std::cout << "no GPU, its tests skipped: " << e.what() << '\n';
		}

		SetFailAfter("1");
		try {
			gpu::Context context;
			auto gpu = CreateGpuTextureEncoder(context, Completion::ContextThread);
			std::vector<MipChain> chains;
			for (std::uint32_t i = 0; i < 6; ++i) chains.push_back(AtlasLike(2048, 2048, 30 + i, false));
			std::mutex mutex;
			std::vector<EncodeResult> results(chains.size());
			for (std::size_t i = 0; i < chains.size(); ++i) {
				gpu->Encode({ chains[i], Format::BC7 }, [&, i](EncodeResult a_result) {
					std::scoped_lock lock(mutex);
					results[i] = std::move(a_result);
				});
			}
			gpu->Wait();
			std::size_t failed = 0;
			for (const auto& result : results) failed += result.Ok() ? 0u : 1u;
			std::cout << "context-thread completion after failing: " << failed << " of " << results.size() << " reported failed\n";
			assert(failed != 0 && !gpu->Problem().empty());
		} catch (const gpu::UnsupportedError& e) {
			std::cout << "no GPU, its tests skipped: " << e.what() << '\n';
		}
		SetFailAfter("");
	}
#endif
}

int main()
{
	CpuEncoderIsEncodeDds();
#if defined(TESWORLD_TEST_GPU)
	GpuEncoderMatchesTheCpusQuality();
	AFailingGpuHandsItsWorkToTheCpu();
	ContextThreadCompletionNeedsNoTbbThread();
#endif
	std::cout << "tesworld texture encoder tests passed\n";
	return 0;
}