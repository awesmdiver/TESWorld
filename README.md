# tesworld

Reads Bethesda game worlds: plugins and load order, terrain, placed references, the game's archives, meshes and
textures. Shared by [SARP](https://github.com/panthuncia/SARP), which renders Skyrim's world, and
[Oculory](https://github.com/panthuncia/Oculory)'s LOD generator. C++23, Windows and Linux.

| Target | What it holds | Links |
| --- | --- | --- |
| `tesworld::core` | `Plugins.h`: load order, the plugin parser, the static world snapshot, SARP's terrain build. `Terrain.h`: a worldspace's height lattice. `Scene.h`: LOD settings and blocks. `GrassCache.h`: No Grass In Objects' grass cache. `Seasons.h`: Seasons of Skyrim's settings, swap files and generated winter swaps. `Platform.h`: mapped files. `Math.h`. | TBB, zlib |
| `tesworld::vfs` | `Vfs.h`: BSA archives read into memory, loose files over archives in the game's order. | core, zlib, lz4 |
| `tesworld::texture` | `Texture.h`: bounded read-only DDS header/payload observations (`InspectDds`, 2D/single cubemaps, 16384 per side, 256 MiB), DDS to and from RGBA8 mip chains (block formats encoded in bands across the cores), sampling as a GPU samples. `TextureEncoder.h`: chains encoded to DDS out of the caller's way, on the CPU or the GPU. `Stochastic.h`: SARP's histogram-preserving stochastic textures. `TerrainMaterial.h`: the landscape's painted material evaluated on the CPU as BasicRenderer's terrain shader evaluates it, and its layers' textures found as SARP finds them. `TerrainBake.h`: terrain LOD's diffuse and normal maps baked from it, by a baker on the CPU or the GPU. `CommunityShadersPBR.h`: Community Shaders' PBR data. | core, vfs, DirectXTex, nlohmann-json, TBB |
| `tesworld::atlas` | `Atlas.h`: xatlas (at SARP's pin) charting meshes and packing rectangles into pages, and a rasterizer for baking into them. Meshes are packed together, or each by itself on every core and then placed as rectangles (`Packing::PerMesh`, many times quicker for many meshes). | core, xatlas (one thread an atlas), TBB |
| `tesworld::jobs` | `Jobs.h`: work as a graph of queues that workers pull from, after SARP's static streaming: typed items with a handler per node, dependencies (`After`) and dataflow edges (`Feeds`), claims in proportion to each node's priority with a cap per node, a blocking lane for I/O, holds for work elsewhere (a GPU frame's callback), cancellation, the first exception rethrown, and nodes that can never complete named. | TBB |
| `tesworld::pipeline` | `Pipeline.h`: work items through serial and parallel stages, with a cap on items in flight, a byte budget, cancellation and progress; a byte-bounded asset cache that loads each key once. | core, TBB |
| `tesworld::render` | `Render.h`: models drawn into images through an orthographic camera (`CpuRenderer`: z-buffered, alpha-tested, both faces, lit by vertex normals or normal maps, supersampled), and `ViewRenderer`, views rendered out of the caller's way on the CPU or the GPU. | core, texture, TBB |
| `tesworld::nif` | `Nif.h`: a model read as triangle lists in model space; raw header-sized block, reference, controller and texture observations before load-time preparation (`InspectStructure`); a bounded SE texture-set rewriter preserving all other block/header/footer bytes (`RewriteTextureSets`); a Skyrim SE NIF written from a scene graph. Raw inspection requires modern block sizes and at most 100,000 blocks. Optional (`TESWORLD_WITH_NIF`). | core, [nifly](https://github.com/ousnius/nifly) (GPL-3.0) |
| `tesworld::gpu` | `Gpu.h`: a headless GPU (D3D12 or Vulkan) run by an OpenRenderGraph host on a thread of its own, as FasterNGIO's rejection runs; unsupported adapters refused up front. `TerrainBakeGpu.h`: the terrain bake on it (the material, mips and BC7 in compute kernels, `src/gpu/shaders`, compiled at build time and embedded). `TextureEncoderGpu.h`: BC7 encoding on it; `Completion::ContextThread` finishes requests on the context's thread rather than as TBB tasks, for callers that block TBB workers on them. `GpuCompute.h`: a caller's own compute kernels on it, recorded through a `Recorder` with transient uploads and scratch buffers (`Upload`, `Scratch`, freed when the submission completes) and readbacks. `RenderGpu.h`: views rendered on it, many to a frame, with `CpuRenderer`'s arithmetic in the shaders (with `tesworld::render`). All are clients of the context built from the same parts (`src/gpu/ClientQueue.h` for jobs, frames and the CPU fallback; `src/gpu/ImageFrames.h` for images through a frame). Optional (`TESWORLD_WITH_GPU`, vcpkg feature `gpu`; the Vulkan SDK to build). | core, texture, [BasicRHI](https://github.com/panthuncia/BasicRHI), [OpenRenderGraph](https://github.com/panthuncia/OpenRenderGraph) |
| `tesworld::profiling` | `Profiling.h`: `TW_ZONE` and friends, BasicTelemetry scopes passed through to Tracy with `TESWORLD_WITH_PROFILING` (vcpkg feature `profiling`), nothing without. Every target links it. | [BasicTelemetry](https://github.com/panthuncia/BasicTelemetry), Tracy (profiling only) |

The plugin parser is SARP's (`src/GameData`), moved here unchanged apart from:

- its namespace, `tesworld` (SARP can keep `SARP::GameData` as an alias);
- the header cache, now a `HeaderCache` its caller owns instead of a global;
- the Win32 mapping behind `Platform.h`, with a POSIX one beside it;
- the records LOD needs: the worldspace's bounds, parent, water, HD LOD textures and large references (merged across
  masters, as the game does), object bounds, STAT materials (MATO), static collections, and the complete optional XCLC
  hide-land word on cells (including partial-grid overrides); consumers interpret its terrain behavior;
- `BuildTerrainWorld` can build the mesh, the painted layers or both, reads a snapshot without copying it, and builds the
  layers per region in parallel (Tamriel: 49 s to 0.4 s).

The terrain material and stochastic textures are BasicRenderer's (`terrainCommon.hlsli`,
`TextureProcessingManager.cpp`), ported to the CPU for offline bakes; their function names follow the originals.

## Tools

Built with the library (`TESWORLD_BUILD_TOOLS`), for looking at game data and generated output:

- `tesworld-extract <data> <out> <game path>...`: files as the game sees them (loose over BSA).
- `tesworld-nifdump <nif>...`: a NIF's scene graph, vertex formats, shaders, segments.
- `tesworld-dds2png <dds> <png> [level]`.
- `tesworld-topdown <png> <size> <data>... -- <mesh>...`: meshes rendered from above with their textures.

## Building

```sh
cmake --preset windows   # or linux; needs VCPKG_ROOT
cmake --build --preset windows
ctest --preset windows
```

nifly and xatlas come from upstream at pinned commits through FetchContent, as do BasicTelemetry, BasicRHI, OpenRenderGraph
and volk (at FasterNGIO's pins) for the GPU and profiling builds; the rest from vcpkg. `windows-gpu` / `linux-gpu`
build the GPU module and its tests.

## Licence

GPL-3.0-or-later (`LICENSE`), as nifly, which `tesworld::nif` links, and FasterNGIO, whose GPU host `tesworld::gpu` is
ported from.

As an additional permission (GPL-3.0 section 7), the copyright holder also licenses tesworld's targets other than
`tesworld::nif` under the MIT licence for use in [SARP](https://github.com/panthuncia/SARP), so SARP can stay MIT. This
permission doesn't cover nifly or `tesworld::nif`, and SARP checks at configure time that it doesn't build them.


### Reader correctness diagnostics

Static-world CELL readers retain EDID and field presence on ordinary and compressed records. Compressed CELLs are
streamed through the complete zlib payload, validating the checksum, advertised size and subrecord termination;
DATA/XCLC/XCLW are not an early completion condition. Ignored override observations do not suppress prior active
records; active deleted records do. `tesworld_plugin_reader_tests` exercises these distinctions and malformed input.

`WorldTerrain` resolves land and water parent-use chains independently, stopping at a missing parent or a cycle.
`WaterWorld()` exposes the effective water source alongside `LandWorld()`. Master RNAM entries use stable
first-contributor duplicate selection for reproducibility. This is a provisional fallback: conflicting RNAM coordinates
and distinct CELLs competing for one coordinate require independent engine precedence evidence. Oculory's Mutagen
differential validator reports these conflicts as unresolved coverage and retains candidate provenance.

CELL LAND is a singleton slot: later explicit children replace it even with a new identity; an omitted child preserves
it, deleting its current identity clears it without reviving displaced records, and deleting a displaced identity
does not clear its replacement.
### Grass consumers

`PluginParser::ParseMode::StaticGrassIndex` uses the same plugin reader with Skyrim's LAND DATA
bit gating (height bit 0, vertex color bit 1, in subrecord order). The default static mode retains
all decoded source arrays for LOD and differential validation. `enginePartialForms` in snapshot
build options preserves Skyrim LoadPartial behavior: existing CELL fields are retained, WRLD
updates only map bounds, and a partial form without an earlier definition is skipped. The default
continues to expose field-presence merging for existing LOD consumers. FasterNGIO projects the
resolved snapshot into grass inputs and applies engine water rounding; it retains its triangle
sampler and vanilla RNG sequence. Both modes share ignored-record and singleton CELL LAND fixes.

`StaticWorldSnapshotBuildOptions.includeDisabledPlacements` retains initially disabled winning placements for
consumers that classify dynamic references. It defaults to false and applies to serial and parallel bucketing;
override resolution still happens before visibility filtering. Retained placements keep their original flags.


### Seasonal configuration filenames

`seasons::ReadSeasonsSettings` expects `DataFiles.list` to preserve original filename spelling. Seasons' swap-file
extension, season suffix and secondary `MainFormSwap` exclusion are case-sensitive; original filenames also determine
swap precedence. Asset/form resolution does not change its matching rules. Loose-file versus archive visibility and
the winning overlay spelling are the consumer's responsibility. `tesworld_seasons_tests` covers filename predicates
and ordering independently of the file provider.