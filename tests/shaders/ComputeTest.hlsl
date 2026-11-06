// GpuComputeTests: samples a texture at the given UVs, as a caller's kernel through GpuCompute.h.
struct SampleConstants
{
	uint textureIndex;
	uint samplerIndex;
	uint samples; // float2 per sample
	uint output; // float4 per sample
	uint count;
	float mip;
	uint pad0;
	uint pad1;
};

cbuffer Push : register(b0)
{
	SampleConstants g;
};

[numthreads(64, 1, 1)]
void SampleCS(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= g.count) {
		return;
	}
	Texture2D<float4> image = ResourceDescriptorHeap[g.textureIndex];
	SamplerState filter = SamplerDescriptorHeap[g.samplerIndex];
	ByteAddressBuffer samples = ResourceDescriptorHeap[g.samples];
	RWByteAddressBuffer output = ResourceDescriptorHeap[g.output];
	const float2 uv = asfloat(samples.Load2(id.x * 8u));
	output.Store4(id.x * 16u, asuint(image.SampleLevel(filter, uv, g.mip)));
}
