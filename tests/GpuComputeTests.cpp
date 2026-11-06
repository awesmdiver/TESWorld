#undef NDEBUG

#include "tesworld/GpuCompute.h"

#include <DirectXTex.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

using namespace tesworld;

namespace
{
	struct Rgba
	{
		float r, g, b, a;
	};

	// A mip as the device should see it: what DirectXTex decodes the uploaded bytes to.
	struct Level
	{
		std::uint32_t width{ 0 }, height{ 0 };
		std::vector<std::byte> uploaded;
		std::uint32_t rowPitch{ 0 };
		std::vector<std::uint8_t> decoded; // RGBA8
	};

	std::vector<std::byte> ReadFile(const std::filesystem::path& a_path)
	{
		std::ifstream file(a_path, std::ios::binary);
		assert(file && "build the test's shaders first");
		const std::vector<char> bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		std::vector<std::byte> result(bytes.size());
		std::memcpy(result.data(), bytes.data(), bytes.size());
		return result;
	}

	// Smooth colour with sharp alpha edges, different per mip, so filtering and block decode both show.
	std::vector<std::uint8_t> Pattern(std::uint32_t a_width, std::uint32_t a_height, std::uint32_t a_seed)
	{
		std::vector<std::uint8_t> texels(static_cast<std::size_t>(a_width) * a_height * 4);
		std::uint32_t state = a_seed;
		for (std::uint32_t y = 0; y < a_height; ++y) {
			for (std::uint32_t x = 0; x < a_width; ++x) {
				state = state * 1664525u + 1013904223u;
				auto* texel = &texels[(static_cast<std::size_t>(y) * a_width + x) * 4];
				texel[0] = static_cast<std::uint8_t>(x * 255 / (a_width - 1));
				texel[1] = static_cast<std::uint8_t>(y * 255 / (a_height - 1));
				texel[2] = static_cast<std::uint8_t>(state >> 24);
				texel[3] = ((x / 3 + y / 2 + a_seed) % 3) == 0 ? 0 : 255;
			}
		}
		return texels;
	}

	Level MakeLevel(gpu::TextureFormat a_format, std::uint32_t a_width, std::uint32_t a_height, std::uint32_t a_seed)
	{
		Level level{ .width = a_width, .height = a_height };
		const auto texels = Pattern(a_width, a_height, a_seed);
		DirectX::Image source{ a_width, a_height, DXGI_FORMAT_R8G8B8A8_UNORM, a_width * 4, static_cast<std::size_t>(a_width) * a_height * 4,
			const_cast<std::uint8_t*>(texels.data()) };
		if (a_format == gpu::TextureFormat::RGBA8) {
			level.uploaded.resize(texels.size());
			std::memcpy(level.uploaded.data(), texels.data(), texels.size());
			level.rowPitch = a_width * 4;
			level.decoded = texels;
			return level;
		}
		const DXGI_FORMAT format = a_format == gpu::TextureFormat::BC1 ? DXGI_FORMAT_BC1_UNORM :
		                           a_format == gpu::TextureFormat::BC3 ? DXGI_FORMAT_BC3_UNORM :
		                                                                 DXGI_FORMAT_BC7_UNORM;
		DirectX::ScratchImage compressed;
		[[maybe_unused]] HRESULT hr = DirectX::Compress(source, format, DirectX::TEX_COMPRESS_DEFAULT, 0.5f, compressed);
		assert(SUCCEEDED(hr));
		const auto* image = compressed.GetImage(0, 0, 0);
		level.uploaded.resize(image->slicePitch);
		std::memcpy(level.uploaded.data(), image->pixels, image->slicePitch);
		level.rowPitch = static_cast<std::uint32_t>(image->rowPitch);
		assert(level.rowPitch == gpu::TextureRowBytes(a_format, a_width));
		DirectX::ScratchImage decompressed;
		hr = DirectX::Decompress(*image, DXGI_FORMAT_R8G8B8A8_UNORM, decompressed);
		assert(SUCCEEDED(hr));
		const auto* decoded = decompressed.GetImage(0, 0, 0);
		level.decoded.resize(static_cast<std::size_t>(a_width) * a_height * 4);
		for (std::uint32_t y = 0; y < a_height; ++y) {
			std::memcpy(&level.decoded[static_cast<std::size_t>(y) * a_width * 4], decoded->pixels + y * decoded->rowPitch, a_width * 4);
		}
		return level;
	}

	Rgba Texel(const Level& a_level, int a_x, int a_y)
	{
		const auto* t = &a_level.decoded[(static_cast<std::size_t>(a_y) * a_level.width + static_cast<std::size_t>(a_x)) * 4];
		return { t[0] / 255.0f, t[1] / 255.0f, t[2] / 255.0f, t[3] / 255.0f };
	}

	int Address(int a_coordinate, int a_size, gpu::Address a_mode)
	{
		if (a_mode == gpu::Address::Clamp) return std::clamp(a_coordinate, 0, a_size - 1);
		return ((a_coordinate % a_size) + a_size) % a_size;
	}

	Rgba Reference(const Level& a_level, float a_u, float a_v, gpu::Filter a_filter, gpu::Address a_mode)
	{
		const float x = a_u * static_cast<float>(a_level.width), y = a_v * static_cast<float>(a_level.height);
		const int w = static_cast<int>(a_level.width), h = static_cast<int>(a_level.height);
		if (a_filter == gpu::Filter::Point) {
			return Texel(a_level, Address(static_cast<int>(std::floor(x)), w, a_mode), Address(static_cast<int>(std::floor(y)), h, a_mode));
		}
		const float fx = x - 0.5f, fy = y - 0.5f;
		const int x0 = static_cast<int>(std::floor(fx)), y0 = static_cast<int>(std::floor(fy));
		const float tx = fx - static_cast<float>(x0), ty = fy - static_cast<float>(y0);
		Rgba out{ 0, 0, 0, 0 };
		for (int j = 0; j < 2; ++j) {
			for (int i = 0; i < 2; ++i) {
				const float weight = (i ? tx : 1.0f - tx) * (j ? ty : 1.0f - ty);
				const auto t = Texel(a_level, Address(x0 + i, w, a_mode), Address(y0 + j, h, a_mode));
				out.r += weight * t.r;
				out.g += weight * t.g;
				out.b += weight * t.b;
				out.a += weight * t.a;
			}
		}
		return out;
	}

	struct SampleConstants
	{
		std::uint32_t texture, sampler, samples, output, count;
		float mip;
		std::uint32_t pad0, pad1;
	};

	void KernelsSampleUploadedTextures()
	{
		try {
			// TESWORLD_TEST_GPU_DEBUG=1 turns the API's validation on, and its messages.
			char* debugValue = nullptr;
			std::size_t debugLength = 0;
			const bool debug = _dupenv_s(&debugValue, &debugLength, "TESWORLD_TEST_GPU_DEBUG") == 0 && debugValue != nullptr;
			std::free(debugValue);
			gpu::Context context(gpu::ContextDesc{ .debugLayer = debug, .quietLogging = !debug });
			std::cout << "GPU: " << context.Describe() << '\n';
			gpu::Compute compute(context);
			const auto dir = std::filesystem::path(TESWORLD_TEST_SHADER_DIR);
			const auto dxil = ReadFile(dir / "ComputeTest.dxil");
			const auto spirv = ReadFile(dir / "ComputeTest.spv");
			const auto kernel = compute.CreateKernel(gpu::KernelDesc{ .dxil = dxil, .spirv = spirv, .entry = "SampleCS", .constantWords = 8 });

			// Points inside, on texel centres, and outside [0, 1] where wrap and clamp differ.
			std::vector<std::array<float, 2>> uvs;
			std::uint32_t state = 7;
			for (int i = 0; i < 512; ++i) {
				state = state * 1664525u + 1013904223u;
				const float u = static_cast<float>(state >> 8) / 16777216.0f * 1.6f - 0.3f;
				state = state * 1664525u + 1013904223u;
				const float v = static_cast<float>(state >> 8) / 16777216.0f * 1.6f - 0.3f;
				uvs.push_back({ u, v });
			}
			for (int y = 0; y < 16; ++y) {
				for (int x = 0; x < 16; ++x) uvs.push_back({ (static_cast<float>(x) + 0.5f) / 16.0f, (static_cast<float>(y) + 0.5f) / 16.0f });
			}
			const auto count = static_cast<std::uint32_t>(uvs.size());
			const auto samples = compute.Upload(std::as_bytes(std::span(uvs)), "samples");
			const auto output = compute.CreateBuffer(static_cast<std::uint64_t>(count) * 16, true, "sampled");

			struct Mode
			{
				gpu::Filter filter;
				gpu::Address address;
			};
			const Mode modes[] = { { gpu::Filter::Point, gpu::Address::Wrap }, { gpu::Filter::Linear, gpu::Address::Wrap }, { gpu::Filter::Linear, gpu::Address::Clamp } };
			// RGBA8 and BC7 decode exactly. D3D lets BC1-3's interpolated endpoints be off by a few steps, which is why a bake
			// whose answers must match a renderer's samples on the device rather than decoding on the CPU. Hardware
			// filtering weights are fixed point.
			const auto tolerance = [](gpu::TextureFormat a_format, gpu::Filter a_filter) {
				const bool exact = a_format == gpu::TextureFormat::RGBA8 || a_format == gpu::TextureFormat::BC7;
				return ((exact ? 0.5f : 3.0f) + (a_filter == gpu::Filter::Linear ? 1.5f : 0.0f)) / 255.0f;
			};
			std::vector<gpu::Sampler> samplers;
			for (const auto& mode : modes) {
				samplers.push_back(compute.CreateSampler(gpu::SamplerDesc{ .filter = mode.filter, .mipFilter = gpu::Filter::Point, .addressU = mode.address, .addressV = mode.address }));
			}

			for (const auto format : { gpu::TextureFormat::RGBA8, gpu::TextureFormat::BC1, gpu::TextureFormat::BC3, gpu::TextureFormat::BC7 }) {
				const Level levels[] = { MakeLevel(format, 16, 16, 1), MakeLevel(format, 8, 8, 2) };
				std::vector<gpu::TextureMip> mips;
				for (const auto& level : levels) {
					mips.push_back(gpu::TextureMip{ .width = level.width, .height = level.height, .bytes = level.uploaded, .rowPitch = level.rowPitch });
				}
				const auto texture = compute.UploadTexture(gpu::TextureDesc{ .format = format, .mips = mips, .name = "test texture" });
				assert(texture.mips == 2 && texture.width == 16);
				for (std::size_t m = 0; m < std::size(modes); ++m) {
					for (std::uint32_t mip = 0; mip < 2; ++mip) {
						// Mip 1 reads and writes buffers of the submission's own (Recorder::Upload, Scratch).
						const bool transient = mip == 1;
						auto results = compute
						                   .Submit([&](gpu::Recorder& a_recorder) {
							                   const auto in = transient ? a_recorder.Upload(std::as_bytes(std::span(uvs))) : samples;
							                   const auto out = transient ? a_recorder.Scratch(static_cast<std::uint64_t>(count) * 16) : output;
							                   const SampleConstants constants{ texture.srv, samplers[m].index, in.srv, out.uav, count, static_cast<float>(mip), 0, 0 };
							                   const std::span<const std::uint32_t> words(reinterpret_cast<const std::uint32_t*>(&constants), sizeof(constants) / 4);
							                   a_recorder.Dispatch(kernel, words, (count + 63) / 64);
							                   a_recorder.Readback(out, 0, static_cast<std::uint64_t>(count) * 16);
						                   })
						                   .get();
						assert(results.size() == 1 && results[0].size() == static_cast<std::size_t>(count) * 16);
						const auto* sampled = reinterpret_cast<const Rgba*>(results[0].data());
						float worst = 0.0f;
						for (std::uint32_t i = 0; i < count; ++i) {
							const auto expected = Reference(levels[mip], uvs[i][0], uvs[i][1], modes[m].filter, modes[m].address);
							const auto& got = sampled[i];
							worst = (std::max)({ worst, std::abs(got.r - expected.r), std::abs(got.g - expected.g), std::abs(got.b - expected.b), std::abs(got.a - expected.a) });
						}
						std::cout << "format " << static_cast<int>(format) << " sampler " << m << " mip " << mip << ": worst difference " << worst * 255.0f << "/255\n";
						std::cout.flush();
						assert(worst <= tolerance(format, modes[m].filter));
					}
				}
				compute.Release(texture);
			}
			compute.Release(samples);
			compute.Release(output);
			compute.Release(kernel);
			std::cout << "GPU compute: textures sample as DirectXTex decodes them\n";
		} catch (const gpu::UnsupportedError& e) {
			std::cout << "GPU compute skipped: " << e.what() << '\n';
		}
	}
}

int main()
{
	KernelsSampleUploadedTextures();
	std::cout << "tesworld GPU compute tests passed\n";
	return 0;
}
