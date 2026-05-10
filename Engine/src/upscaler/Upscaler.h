#pragma once

#include <filesystem>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <optional>

#include <vulkan/vulkan.h>
#include <glm/glm.hpp>

#include "Device.h"
#include "VulkanTexture.h"

namespace RT::Vulkan
{

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declarations
// ─────────────────────────────────────────────────────────────────────────────
class VulkanUniform;

// ─────────────────────────────────────────────────────────────────────────────
//  NCNN-style layer descriptor parsed from a .param file
// ─────────────────────────────────────────────────────────────────────────────
struct LayerParam
{
    std::string   type;          // e.g. "Convolution", "ReLU", "PixelShuffle"
    std::string   name;          // unique layer name
    int32_t       inputCount  = 0;
    int32_t       outputCount = 0;
    std::vector<std::string> inputs;
    std::vector<std::string> outputs;

    // Typed attribute bag  (key -> value as string; cast on use)
    std::unordered_map<int32_t, std::string> attrs;

    // Convenience helpers
    int32_t     attrInt  (int32_t key, int32_t     def = 0)   const;
    float       attrFloat(int32_t key, float       def = 0.f) const;
    std::string attrStr  (int32_t key, std::string def = {})  const;
};

// ─────────────────────────────────────────────────────────────────────────────
//  A single tensor blob (weight / bias / intermediate activation)
// ─────────────────────────────────────────────────────────────────────────────
struct Blob
{
    std::string      name;
    std::vector<int32_t> shape;   // [N, C, H, W] or subset
    VkBuffer         buffer  = VK_NULL_HANDLE;
    VkDeviceMemory   memory  = VK_NULL_HANDLE;
    VkDeviceSize     size    = 0;           // bytes
    bool             onDevice = false;

    // Non-copyable, movable
    Blob() = default;
    ~Blob();
    Blob(const Blob&) = delete;
    Blob& operator=(const Blob&) = delete;
    Blob(Blob&&) noexcept;
    Blob& operator=(Blob&&) noexcept;

private:
    void destroy();
};

// ─────────────────────────────────────────────────────────────────────────────
//  Upscaling algorithm / model preset
// ─────────────────────────────────────────────────────────────────────────────
enum class UpscaleMode : uint8_t
{
    // Generic NCNN compute shader path (model defined by .param/.bin)
    Generic  = 0,

    // Built-in bilinear / nearest upscale (no model needed)
    Bilinear = 1,
    Nearest  = 2,
};

// ─────────────────────────────────────────────────────────────────────────────
//  Creation parameters
// ─────────────────────────────────────────────────────────────────────────────
struct UpscalerSpec
{
    std::filesystem::path paramPath;   // NCNN .param file (empty for built-ins)
    std::filesystem::path binPath;     // NCNN .bin   file (empty for built-ins)
    std::filesystem::path shaderDir;   // directory that holds .comp.glsl shaders

    UpscaleMode  mode        = UpscaleMode::Generic;
    uint32_t     scaleFactor = 2u;    // 1×, 2×, 3×, 4×
    bool         fp16        = false; // half-precision inference (future)

    // Tile-based inference to handle large images on limited VRAM
    bool         tiling      = true;
    glm::uvec2   tileSize    = { 512u, 512u };
    uint32_t     tilePad     = 10u;   // overlap padding to hide tile seams
};

// ─────────────────────────────────────────────────────────────────────────────
//  Per-dispatch push constants (matches the shader layout)
// ─────────────────────────────────────────────────────────────────────────────
struct UpscalePushConstants
{
    glm::uvec2 srcSize    = {};
    glm::uvec2 dstSize    = {};
    glm::uvec2 tileOffset = {};     // top-left of current tile in src pixels
    glm::uvec2 tileSize   = {};
    float      scaleFactor = 2.f;
    uint32_t   channels    = 4u;
    uint32_t   _pad0 = 0u, _pad1 = 0u;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Main class
// ─────────────────────────────────────────────────────────────────────────────
class Upscaler
{
public:
    // ── Lifecycle ─────────────────────────────────────────────────────────
    explicit Upscaler(const UpscalerSpec& spec);
    ~Upscaler();

    Upscaler(const Upscaler&) = delete;
    Upscaler(Upscaler&&)      = delete;
    Upscaler& operator=(const Upscaler&) = delete;
    Upscaler& operator=(Upscaler&&)      = delete;

    // ── Hot-reload / model swap (safe to call between frames) ────────────
    void reloadModel(const std::filesystem::path& paramPath,
                     const std::filesystem::path& binPath);

    // ── Primary dispatch ──────────────────────────────────────────────────
    //  Records all barrier + dispatch commands into the current frame
    //  command buffer (Context::frameCmd must be recording).
    //
    //  src  – RGBA8 or RGBA32F input texture  (shader-read layout expected)
    //  dst  – pre-allocated output texture    (storage layout expected)
    //         must be spec.scaleFactor × src dimensions
    void upscale(const VulkanTexture& src, VulkanTexture& dst);

    // ── Queries ───────────────────────────────────────────────────────────
    bool          isReady()      const { return ready; }
    uint32_t      scaleFactor()  const { return spec.scaleFactor; }
    UpscaleMode   mode()         const { return spec.mode; }
    const std::vector<LayerParam>& layers() const { return parsedLayers; }

private:
    // ── Model loading ─────────────────────────────────────────────────────
    void loadModel(const std::filesystem::path& paramPath,
                   const std::filesystem::path& binPath);
    std::vector<LayerParam> parseParamFile(const std::filesystem::path& path) const;
    void                    loadWeights(const std::filesystem::path& binPath);

    // ── Vulkan resource management ────────────────────────────────────────
    void createDescriptorSetLayout();
    void createDescriptorPool();
    void allocateDescriptorSets();
    void createPipelineLayout();
    void createComputePipeline(const std::filesystem::path& shaderPath);
    void createWeightBuffers();
    void createStagingBuffer(VkDeviceSize size, VkBuffer& buf, VkDeviceMemory& mem) const;
    void uploadBlobToDevice(Blob& blob, const void* hostData) const;

    void updateDescriptorSets(const VulkanTexture& src, const VulkanTexture& dst);

    // ── Tiling helpers ────────────────────────────────────────────────────
    void dispatchTile(const VulkanTexture& src,
                      const VulkanTexture& dst,
                      const glm::uvec2     tileOffset,
                      const glm::uvec2     tileSize) const;

    // ── Cleanup ───────────────────────────────────────────────────────────
    void destroyPipeline();
    void destroyDescriptors();
    void destroyWeightBuffers();

private:
    UpscalerSpec spec;
    bool         ready = false;

    // ── Parsed model ──────────────────────────────────────────────────────
    std::vector<LayerParam>   parsedLayers;
    std::vector<Blob>         weightBlobs;    // one blob per weight tensor

    // ── Vulkan objects ────────────────────────────────────────────────────
    VkPipeline            pipeline            = VK_NULL_HANDLE;
    VkPipelineLayout      pipelineLayout      = VK_NULL_HANDLE;
    VkShaderModule        computeShaderModule = VK_NULL_HANDLE;

    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    VkDescriptorPool      descriptorPool      = VK_NULL_HANDLE;
    VkDescriptorSet       descriptorSet       = VK_NULL_HANDLE;

    // ── Scratch / intermediate buffers ───────────────────────────────────
    //  Used for tiled inference; re-created on size change
    struct ScratchBuffers
    {
        VkBuffer       buffer = VK_NULL_HANDLE;
        VkDeviceMemory memory = VK_NULL_HANDLE;
        VkDeviceSize   size   = 0;
    } scratch;

    // ── Descriptor binding indices (must match shader) ───────────────────
    static constexpr uint32_t kBindingSrcImage   = 0u;
    static constexpr uint32_t kBindingDstImage   = 1u;
    static constexpr uint32_t kBindingWeights    = 2u;  // SSBO for packed weights
    static constexpr uint32_t kBindingScratch    = 3u;  // SSBO for intermediates

    // ── Compute group size (must match shader local_size_x/y) ────────────
    static constexpr glm::uvec2 kWorkgroupSize = { 8u, 8u };
};

} // namespace RT::Vulkan
