#pragma once

#include "tesworld/Gpu.h"
#include "tesworld/TextureEncoder.h"

#include <memory>

// Texture encoding on the GPU (tesworld::gpu): BC7 chains uploaded in frames on the context's thread, encoded with the
// terrain bake's BC7 kernel (BasicRenderer's mode 6), read back and wrapped as DirectXTex would. Other formats, and
// everything owed once the GPU fails, are encoded on the CPU.
namespace tesworld::texture
{
	// Where requests finish.
	enum class Completion
	{
		// A TBB task in the arena the encoder was made in, as TextureEncoder says; once the GPU fails, the CPU encodes.
		TbbTask,
		// On the context's thread, the callback included, so it must be short; a request that isn't BC7 is encoded on the
		// calling thread. Once the GPU fails a request finishes with the error instead. For callers that block TBB threads
		// waiting on their callbacks: a TBB task finishing them might never get a thread.
		ContextThread
	};

	// Throws gpu::UnsupportedError when its frames won't fit the adapter's memory.
	[[nodiscard]] std::unique_ptr<TextureEncoder> CreateGpuTextureEncoder(gpu::Context& a_context, Completion a_completion = Completion::TbbTask);
}