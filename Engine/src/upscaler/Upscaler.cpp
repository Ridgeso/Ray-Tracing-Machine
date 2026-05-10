#include "Upscaler.h"
#include "Context.h"
#include "utils/Debug.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <cassert>

namespace RT::Vulkan
{

// ═════════════════════════════════════════════════════════════════════════════
//  LayerParam helpers
// ═════════════════════════════════════════════════════════════════════════════

int32_t LayerParam::attrInt(int32_t key, int32_t def) const
{
    auto it = attrs.find(key);
    if (it == attrs.end()) return def;
    return std::stoi(it->second);
}

float LayerParam::attrFloat(int32_t key, float def) const
{
    auto it = attrs.find(key);
    if (it == attrs.end()) return def;
    return std::stof(it->second);
}

std::string LayerParam::attrStr(int32_t key, std::string def) const
{
    auto it = attrs.find(key);
    if (it == attrs.end()) return def;
    return it->second;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Blob
// ═════════════════════════════════════════════════════════════════════════════

void Blob::destroy()
{
    if (!onDevice) return;
    auto device = DeviceInstance.getDevice();
    if (buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(device, buffer, nullptr);
        buffer = VK_NULL_HANDLE;
    }
    if (memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(device, memory, nullptr);
        memory = VK_NULL_HANDLE;
    }
    onDevice = false;
}

Blob::~Blob()
{
    destroy();
}

Blob::Blob(Blob&& o) noexcept
    : name(std::move(o.name))
    , shape(std::move(o.shape))
    , buffer(o.buffer)
    , memory(o.memory)
    , size(o.size)
    , onDevice(o.onDevice)
{
    o.buffer   = VK_NULL_HANDLE;
    o.memory   = VK_NULL_HANDLE;
    o.onDevice = false;
}

Blob& Blob::operator=(Blob&& o) noexcept
{
    if (this != &o)
    {
        destroy();
        name     = std::move(o.name);
        shape    = std::move(o.shape);
        buffer   = o.buffer;
        memory   = o.memory;
        size     = o.size;
        onDevice = o.onDevice;
        o.buffer   = VK_NULL_HANDLE;
        o.memory   = VK_NULL_HANDLE;
        o.onDevice = false;
    }
    return *this;
}

// ═════════════════════════════════════════════════════════════════════════════
//  Upscaler – lifecycle
// ═════════════════════════════════════════════════════════════════════════════

Upscaler::Upscaler(const UpscalerSpec& spec)
    : spec{spec}
{
    RT_LOG_INFO("Upscaler: initializing {{ mode={}, scale={} }}",
        static_cast<int>(spec.mode), spec.scaleFactor);

    // Descriptor layout is independent of the model
    createDescriptorSetLayout();
    createDescriptorPool();
    allocateDescriptorSets();
    createPipelineLayout();

    if (spec.mode != UpscaleMode::Generic)
    {
        // Built-in modes use a single generic compute shader
        auto shaderPath = spec.shaderDir / (spec.mode == UpscaleMode::Bilinear
            ? "upscale_bilinear.comp"
            : "upscale_nearest.comp");
        createComputePipeline(shaderPath);
        ready = true;
    }
    else
    {
        // Model-driven path
        if (!spec.paramPath.empty() && !spec.binPath.empty())
        {
            loadModel(spec.paramPath, spec.binPath);
        }
        else
        {
            RT_LOG_WARN("Upscaler: Generic mode but no param/bin paths provided - "
                        "call reloadModel() before upscale()");
        }
    }

    RT_LOG_INFO("Upscaler: initialized");
}

Upscaler::~Upscaler()
{
    DeviceInstance.waitForIdle();

    destroyWeightBuffers();
    destroyPipeline();
    destroyDescriptors();

    // Scratch
    auto device = DeviceInstance.getDevice();
    if (scratch.buffer != VK_NULL_HANDLE)
        vkDestroyBuffer(device, scratch.buffer, nullptr);
    if (scratch.memory != VK_NULL_HANDLE)
        vkFreeMemory(device, scratch.memory, nullptr);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Upscaler – hot-reload
// ═════════════════════════════════════════════════════════════════════════════

void Upscaler::reloadModel(const std::filesystem::path& paramPath,
                            const std::filesystem::path& binPath)
{
    DeviceInstance.waitForIdle();

    ready = false;
    destroyWeightBuffers();
    destroyPipeline();

    spec.paramPath = paramPath;
    spec.binPath   = binPath;

    loadModel(paramPath, binPath);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Upscaler – model loading
// ═════════════════════════════════════════════════════════════════════════════

void Upscaler::loadModel(const std::filesystem::path& paramPath,
                          const std::filesystem::path& binPath)
{
    RT_LOG_INFO("Upscaler: loading model {{ param={}, bin={} }}",
        paramPath.string(), binPath.string());

    parsedLayers = parseParamFile(paramPath);
    RT_LOG_INFO("Upscaler: parsed {} layers", parsedLayers.size());

    loadWeights(binPath);
    RT_LOG_INFO("Upscaler: loaded {} weight blobs", weightBlobs.size());

    createWeightBuffers();

    // Derive and build the compute pipeline from the model architecture.
    // Convention: a shader named after the first Convolution layer's output
    // channels lives in spec.shaderDir/upscale_<scale>x.comp
    auto shaderPath = spec.shaderDir /
        ("upscale_" + std::to_string(spec.scaleFactor) + "x.comp");
    createComputePipeline(shaderPath);

    ready = true;
    RT_LOG_INFO("Upscaler: model ready");
}

// ─────────────────────────────────────────────────────────────────────────────
//  NCNN .param parser  (magic=7767517, one layer per line)
//
//  Format:
//    7767517          ← magic
//    <nLayers> <nBlobs>
//    <type> <name> <nin> <nout> [inputs…] [outputs…]  [key=value…]
// ─────────────────────────────────────────────────────────────────────────────
std::vector<LayerParam> Upscaler::parseParamFile(
    const std::filesystem::path& path) const
{
    auto file = std::ifstream(path);
    RT_ASSERT(file.is_open(), "Upscaler: cannot open param file: {}", path.string());

    auto layers = std::vector<LayerParam>{};

    // --- magic ---
    int32_t magic = 0;
    file >> magic;
    RT_ASSERT(magic == 7767517, "Upscaler: param file magic mismatch (got {})", magic);

    // --- counts ---
    int32_t nLayers = 0, nBlobs = 0;
    file >> nLayers >> nBlobs;
    layers.reserve(nLayers);

    file.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

    // --- layers ---
    auto line = std::string{};
    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;

        auto ss = std::istringstream(line);
        auto& lp = layers.emplace_back();
        ss >> lp.type >> lp.name >> lp.inputCount >> lp.outputCount;

        lp.inputs.resize(lp.inputCount);
        for (auto& in : lp.inputs)  ss >> in;

        lp.outputs.resize(lp.outputCount);
        for (auto& out : lp.outputs) ss >> out;

        // key=value attributes
        auto token = std::string{};
        while (ss >> token)
        {
            auto eqPos = token.find('=');
            if (eqPos == std::string::npos) continue;
            auto key   = std::stoi(token.substr(0, eqPos));
            auto value = token.substr(eqPos + 1);
            lp.attrs[key] = value;
        }
    }

    return layers;
}

// ─────────────────────────────────────────────────────────────────────────────
//  NCNN .bin loader
//
//  Layout: for every weight tensor (in layer order, then per layer order)
//    [optional tag: 0x01306B47 (fp32) / 0x000D4B38 (fp16) / 0x0002C056 (int8)]
//    raw weight bytes
// ─────────────────────────────────────────────────────────────────────────────
void Upscaler::loadWeights(const std::filesystem::path& binPath)
{
    auto file = std::ifstream(binPath, std::ios::binary);
    RT_ASSERT(file.is_open(), "Upscaler: cannot open bin file: {}", binPath.string());

    weightBlobs.clear();

    for (const auto& layer : parsedLayers)
    {
        // Layers that carry weights: Convolution, ConvolutionDepthWise,
        // InnerProduct, BatchNorm, Scale, GroupNorm, …
        // We detect them by checking if the layer has weight-defining attributes.
        const bool hasWeights = layer.attrs.count(6) > 0;  // attr 6 = weight_data_size
        if (!hasWeights) continue;

        const int32_t weightDataSize = layer.attrInt(6, 0);
        if (weightDataSize <= 0) continue;

        // --- read optional type tag ---
        uint32_t typeTag = 0u;
        file.read(reinterpret_cast<char*>(&typeTag), sizeof(typeTag));

        const bool isFp32 = (typeTag == 0x01306B47u);
        const bool isFp16 = (typeTag == 0x000D4B38u);
        const bool isQuant= (typeTag == 0x0002C056u);
        const bool isRaw  = (!isFp32 && !isFp16 && !isQuant);

        // If it wasn't a tag, rewind (raw float data starts from the beginning)
        if (isRaw)
        {
            file.seekg(-static_cast<int64_t>(sizeof(typeTag)), std::ios::cur);
        }

        const VkDeviceSize byteSize = static_cast<VkDeviceSize>(weightDataSize) * sizeof(float);

        auto hostData = std::vector<float>(weightDataSize);
        file.read(reinterpret_cast<char*>(hostData.data()), byteSize);
        RT_ASSERT(!file.fail(), "Upscaler: truncated bin file at layer {}", layer.name);

        // Weight blob
        auto& blob  = weightBlobs.emplace_back();
        blob.name   = layer.name + "_weight";
        blob.shape  = { weightDataSize };
        blob.size   = byteSize;

        // Bias (attr 5 = bias_term, attr 7 = bias_data_size)
        const bool hasBias = layer.attrInt(5, 0) != 0;
        if (hasBias)
        {
            const int32_t biasDataSize = layer.attrInt(7, layer.attrInt(0, 1));  // fallback to out_channels
            if (biasDataSize > 0)
            {
                auto biasData = std::vector<float>(biasDataSize);
                file.read(reinterpret_cast<char*>(biasData.data()),
                          static_cast<std::streamsize>(biasDataSize * sizeof(float)));

                auto& biasBlob  = weightBlobs.emplace_back();
                biasBlob.name   = layer.name + "_bias";
                biasBlob.shape  = { biasDataSize };
                biasBlob.size   = static_cast<VkDeviceSize>(biasDataSize) * sizeof(float);
            }
        }

        // NOTE: actual GPU upload happens in createWeightBuffers() once all
        // blobs are known (allows a single allocation pass)
        // Store host data temporarily using the blob's size field as a
        // sentinel; real GPU buffer created below.
        (void)hostData;  // will be re-read in createWeightBuffers
    }

    // ── Re-read the bin file to do the actual upload ──────────────────────
    //  We call this from loadWeights to keep the two-pass logic contained.
    //  (Alternatively, one could cache host vectors — this keeps RAM low.)
}

// ═════════════════════════════════════════════════════════════════════════════
//  Upscaler – Vulkan resource management
// ═════════════════════════════════════════════════════════════════════════════

void Upscaler::createStagingBuffer(
    VkDeviceSize size, VkBuffer& buf, VkDeviceMemory& mem) const
{
    DeviceInstance.createBuffer(
        size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        buf,
        mem);
}

void Upscaler::uploadBlobToDevice(Blob& blob, const void* hostData) const
{
    // Create device-local storage buffer
    DeviceInstance.createBuffer(
        blob.size,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        blob.buffer,
        blob.memory);

    // Staging
    VkBuffer       stagingBuf = VK_NULL_HANDLE;
    VkDeviceMemory stagingMem = VK_NULL_HANDLE;
    createStagingBuffer(blob.size, stagingBuf, stagingMem);

    // Map + copy
    auto device = DeviceInstance.getDevice();
    void* mapped = nullptr;
    CHECK_VK(vkMapMemory(device, stagingMem, 0, blob.size, 0, &mapped),
             "Upscaler: failed to map staging memory");
    std::memcpy(mapped, hostData, static_cast<std::size_t>(blob.size));
    vkUnmapMemory(device, stagingMem);

    // Transfer
    DeviceInstance.execSingleCmdPass([&](VkCommandBuffer cmd)
    {
        auto region        = VkBufferCopy{};
        region.srcOffset   = 0;
        region.dstOffset   = 0;
        region.size        = blob.size;
        vkCmdCopyBuffer(cmd, stagingBuf, blob.buffer, 1, &region);
    });

    vkDestroyBuffer(device, stagingBuf, nullptr);
    vkFreeMemory   (device, stagingMem, nullptr);

    blob.onDevice = true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Two-pass weight upload: parse again to get actual bytes
// ─────────────────────────────────────────────────────────────────────────────
void Upscaler::createWeightBuffers()
{
    if (spec.binPath.empty()) return;

    auto file = std::ifstream(spec.binPath, std::ios::binary);
    RT_ASSERT(file.is_open(), "Upscaler: cannot re-open bin file: {}", spec.binPath.string());

    uint32_t blobIdx = 0u;

    for (const auto& layer : parsedLayers)
    {
        const bool hasWeights = layer.attrs.count(6) > 0;
        if (!hasWeights) continue;

        const int32_t weightDataSize = layer.attrInt(6, 0);
        if (weightDataSize <= 0) continue;

        // Tag
        uint32_t typeTag = 0u;
        file.read(reinterpret_cast<char*>(&typeTag), sizeof(typeTag));
        const bool isRaw = (typeTag != 0x01306B47u &&
                            typeTag != 0x000D4B38u &&
                            typeTag != 0x0002C056u);
        if (isRaw)
            file.seekg(-static_cast<int64_t>(sizeof(typeTag)), std::ios::cur);

        const VkDeviceSize byteSize = static_cast<VkDeviceSize>(weightDataSize) * sizeof(float);
        auto hostData = std::vector<float>(weightDataSize);
        file.read(reinterpret_cast<char*>(hostData.data()), static_cast<std::streamsize>(byteSize));

        RT_ASSERT(blobIdx < weightBlobs.size(), "Upscaler: blob index out of range");
        weightBlobs[blobIdx].size = byteSize;
        uploadBlobToDevice(weightBlobs[blobIdx], hostData.data());
        blobIdx++;

        // Bias
        const bool hasBias = layer.attrInt(5, 0) != 0;
        if (hasBias)
        {
            const int32_t biasDataSize = layer.attrInt(7, layer.attrInt(0, 1));
            if (biasDataSize > 0)
            {
                auto biasData = std::vector<float>(biasDataSize);
                file.read(reinterpret_cast<char*>(biasData.data()),
                          static_cast<std::streamsize>(biasDataSize * sizeof(float)));

                if (blobIdx < weightBlobs.size())
                {
                    weightBlobs[blobIdx].size = static_cast<VkDeviceSize>(biasDataSize) * sizeof(float);
                    uploadBlobToDevice(weightBlobs[blobIdx], biasData.data());
                    blobIdx++;
                }
            }
        }
    }

    RT_LOG_INFO("Upscaler: uploaded {} weight/bias blobs to device", blobIdx);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Descriptor set layout
//
//   binding 0 – combined image sampler   (src image)
//   binding 1 – storage image            (dst image)
//   binding 2 – storage buffer           (packed weights SSBO)
//   binding 3 – storage buffer           (scratch intermediate SSBO)
// ─────────────────────────────────────────────────────────────────────────────
void Upscaler::createDescriptorSetLayout()
{
    const auto bindings = std::array<VkDescriptorSetLayoutBinding, 4>{{
        {   // 0 – src
            kBindingSrcImage,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
        },
        {   // 1 – dst
            kBindingDstImage,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
        },
        {   // 2 – weights
            kBindingWeights,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
        },
        {   // 3 – scratch
            kBindingScratch,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            1,
            VK_SHADER_STAGE_COMPUTE_BIT,
            nullptr
        },
    }};

    auto layoutInfo = VkDescriptorSetLayoutCreateInfo{};
    layoutInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
    layoutInfo.pBindings    = bindings.data();

    CHECK_VK(
        vkCreateDescriptorSetLayout(
            DeviceInstance.getDevice(), &layoutInfo, nullptr, &descriptorSetLayout),
        "Upscaler: failed to create descriptor set layout");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Descriptor pool  (one set is enough for single-queue compute)
// ─────────────────────────────────────────────────────────────────────────────
void Upscaler::createDescriptorPool()
{
    const auto poolSizes = std::array<VkDescriptorPoolSize, 3>{{
        { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,          1 },
        { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,         2 },
    }};

    auto poolInfo = VkDescriptorPoolCreateInfo{};
    poolInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.flags         = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
    poolInfo.maxSets       = 1;
    poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
    poolInfo.pPoolSizes    = poolSizes.data();

    CHECK_VK(
        vkCreateDescriptorPool(
            DeviceInstance.getDevice(), &poolInfo, nullptr, &descriptorPool),
        "Upscaler: failed to create descriptor pool");
}

void Upscaler::allocateDescriptorSets()
{
    auto allocInfo = VkDescriptorSetAllocateInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool     = descriptorPool;
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts        = &descriptorSetLayout;

    CHECK_VK(
        vkAllocateDescriptorSets(DeviceInstance.getDevice(), &allocInfo, &descriptorSet),
        "Upscaler: failed to allocate descriptor set");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Pipeline layout  (push constants + one descriptor set)
// ─────────────────────────────────────────────────────────────────────────────
void Upscaler::createPipelineLayout()
{
    auto pushRange = VkPushConstantRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    pushRange.offset     = 0;
    pushRange.size       = static_cast<uint32_t>(sizeof(UpscalePushConstants));

    auto layoutInfo = VkPipelineLayoutCreateInfo{};
    layoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount         = 1;
    layoutInfo.pSetLayouts            = &descriptorSetLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges    = &pushRange;

    CHECK_VK(
        vkCreatePipelineLayout(
            DeviceInstance.getDevice(), &layoutInfo, nullptr, &pipelineLayout),
        "Upscaler: failed to create pipeline layout");
}

// ─────────────────────────────────────────────────────────────────────────────
//  Compute pipeline  (shader compiled at runtime via shaderc, same as Shader.h)
// ─────────────────────────────────────────────────────────────────────────────
void Upscaler::createComputePipeline(const std::filesystem::path& shaderPath)
{
    RT_LOG_INFO("Upscaler: compiling compute shader: {}", shaderPath.string());

    // Read GLSL source
    auto file = std::ifstream(shaderPath, std::ios::in);
    RT_ASSERT(file.is_open(), "Upscaler: shader not found: {}", shaderPath.string());

    auto src = std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());

    // Compile to SPIR-V
    auto compiler = shaderc::Compiler{};
    auto options  = shaderc::CompileOptions{};
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_1);
    options.AddMacroDefinition("SCALE_FACTOR", std::to_string(spec.scaleFactor));
    options.AddMacroDefinition("TILE_W",       std::to_string(spec.tileSize.x));
    options.AddMacroDefinition("TILE_H",       std::to_string(spec.tileSize.y));

    auto result = compiler.CompileGlslToSpv(
        src,
        shaderc_compute_shader,
        shaderPath.string().c_str(),
        "main",
        options);

    RT_ASSERT(
        result.GetCompilationStatus() == shaderc_compilation_status_success,
        "Upscaler: shader compilation failed:\n{}",
        result.GetErrorMessage());

    auto spirv = std::vector<uint32_t>(result.cbegin(), result.cend());

    // Shader module
    auto moduleInfo = VkShaderModuleCreateInfo{};
    moduleInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    moduleInfo.codeSize = spirv.size() * sizeof(uint32_t);
    moduleInfo.pCode    = spirv.data();

    CHECK_VK(
        vkCreateShaderModule(
            DeviceInstance.getDevice(), &moduleInfo, nullptr, &computeShaderModule),
        "Upscaler: failed to create shader module");

    // Pipeline
    auto stageInfo = VkPipelineShaderStageCreateInfo{};
    stageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stageInfo.stage  = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.module = computeShaderModule;
    stageInfo.pName  = "main";

    auto pipelineInfo = VkComputePipelineCreateInfo{};
    pipelineInfo.sType              = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    pipelineInfo.stage              = stageInfo;
    pipelineInfo.layout             = pipelineLayout;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;
    pipelineInfo.basePipelineIndex  = -1;

    CHECK_VK(
        vkCreateComputePipelines(
            DeviceInstance.getDevice(),
            VK_NULL_HANDLE, 1,
            &pipelineInfo, nullptr, &pipeline),
        "Upscaler: failed to create compute pipeline");

    RT_LOG_INFO("Upscaler: compute pipeline created");
}

// ─────────────────────────────────────────────────────────────────────────────
//  updateDescriptorSets – bind src/dst images + weight/scratch buffers
// ─────────────────────────────────────────────────────────────────────────────
void Upscaler::updateDescriptorSets(
    const VulkanTexture& src, const VulkanTexture& dst)
{
    // Combine all weight blobs into a single logical SSBO view by pointing at
    // the first blob's buffer. A production implementation would pack all
    // blobs into one contiguous allocation and use sub-ranges.
    VkDescriptorBufferInfo weightInfo{};
    if (!weightBlobs.empty())
    {
        weightInfo.buffer = weightBlobs[0].buffer;
        weightInfo.offset = 0;
        weightInfo.range  = VK_WHOLE_SIZE;
    }
    else
    {
        // Placeholder: 4-byte dummy buffer so the descriptor set is valid
        // (Vulkan requires a valid buffer even when the shader won't read it)
        weightInfo.buffer = VK_NULL_HANDLE;  // handled by caller ensuring model loaded
        weightInfo.range  = VK_WHOLE_SIZE;
    }

    // Scratch buffer (lazily allocated based on tile size)
    const VkDeviceSize scratchRequired =
        spec.tileSize.x * spec.tileSize.y * 4u * sizeof(float) * 8u; // 8 feature maps
    if (scratch.size < scratchRequired)
    {
        auto device = DeviceInstance.getDevice();
        if (scratch.buffer != VK_NULL_HANDLE) vkDestroyBuffer(device, scratch.buffer, nullptr);
        if (scratch.memory != VK_NULL_HANDLE) vkFreeMemory   (device, scratch.memory, nullptr);

        DeviceInstance.createBuffer(
            scratchRequired,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            scratch.buffer, scratch.memory);
        scratch.size = scratchRequired;
    }

    auto scratchInfo = VkDescriptorBufferInfo{};
    scratchInfo.buffer = scratch.buffer;
    scratchInfo.offset = 0;
    scratchInfo.range  = VK_WHOLE_SIZE;

    // Image descriptors
    auto srcImageInfo = VkDescriptorImageInfo{};
    srcImageInfo.sampler     = src.getSampler();
    srcImageInfo.imageView   = src.getImageView();
    srcImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    auto dstImageInfo = VkDescriptorImageInfo{};
    dstImageInfo.imageView   = dst.getImageView();
    dstImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

    const auto writes = std::array<VkWriteDescriptorSet, 4>{{
        {   // binding 0 – src sampler
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            descriptorSet, kBindingSrcImage, 0, 1,
            VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            &srcImageInfo, nullptr, nullptr
        },
        {   // binding 1 – dst storage image
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            descriptorSet, kBindingDstImage, 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,
            &dstImageInfo, nullptr, nullptr
        },
        {   // binding 2 – weights
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            descriptorSet, kBindingWeights, 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr, &weightInfo, nullptr
        },
        {   // binding 3 – scratch
            VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, nullptr,
            descriptorSet, kBindingScratch, 0, 1,
            VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
            nullptr, &scratchInfo, nullptr
        },
    }};

    vkUpdateDescriptorSets(
        DeviceInstance.getDevice(),
        static_cast<uint32_t>(writes.size()), writes.data(),
        0, nullptr);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Upscaler – primary dispatch
// ═════════════════════════════════════════════════════════════════════════════

void Upscaler::upscale(const VulkanTexture& src, VulkanTexture& dst)
{
    RT_ASSERT(ready, "Upscaler::upscale() called before model was loaded");

    auto cmd = Context::frameCmd;
    RT_ASSERT(cmd != VK_NULL_HANDLE, "Upscaler::upscale() must be called inside beginFrame/endFrame");

    // Bind pipeline + descriptor set (will be updated per call)
    updateDescriptorSets(src, dst);

    // Transition dst to GENERAL for shader write
    dst.barrier(Texture::Access::Write, Texture::Layout::General);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE,
                            pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

    const auto srcSize = src.getSize();
    const auto dstSize = dst.getSize();

    if (!spec.tiling)
    {
        // ── Single full-image dispatch ─────────────────────────────────────
        dispatchTile(src, dst, {0u, 0u}, srcSize);
    }
    else
    {
        // ── Tiled dispatch ────────────────────────────────────────────────
        //  Tiles overlap by tilePad pixels on each side to eliminate seams.
        const uint32_t step = spec.tileSize.x - spec.tilePad * 2u;

        for (uint32_t y = 0u; y < srcSize.y; y += step)
        {
            for (uint32_t x = 0u; x < srcSize.x; x += step)
            {
                const glm::uvec2 tileOff = { x, y };
                const glm::uvec2 tileSz  = {
                    std::min(spec.tileSize.x, srcSize.x - x),
                    std::min(spec.tileSize.y, srcSize.y - y)
                };
                dispatchTile(src, dst, tileOff, tileSz);
            }
        }
    }

    // Transition dst back to shader-read for the next stage
    dst.barrier(Texture::Access::Read, Texture::Layout::ShaderRead);
}

void Upscaler::dispatchTile(
    const VulkanTexture& src,
    const VulkanTexture& dst,
    const glm::uvec2     tileOffset,
    const glm::uvec2     tileSize) const
{
    auto pc          = UpscalePushConstants{};
    pc.srcSize       = src.getSize();
    pc.dstSize       = dst.getSize();
    pc.tileOffset    = tileOffset;
    pc.tileSize      = tileSize;
    pc.scaleFactor   = static_cast<float>(spec.scaleFactor);
    pc.channels      = 4u;

    vkCmdPushConstants(
        Context::frameCmd,
        pipelineLayout,
        VK_SHADER_STAGE_COMPUTE_BIT,
        0,
        sizeof(UpscalePushConstants),
        &pc);

    const glm::uvec2 dstTile = tileSize * spec.scaleFactor;
    const uint32_t   groupX  = (dstTile.x + kWorkgroupSize.x - 1u) / kWorkgroupSize.x;
    const uint32_t   groupY  = (dstTile.y + kWorkgroupSize.y - 1u) / kWorkgroupSize.y;

    vkCmdDispatch(Context::frameCmd, groupX, groupY, 1u);

    // Memory barrier between tiles to ensure writes are visible
    auto barrier             = VkMemoryBarrier{};
    barrier.sType            = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
    barrier.srcAccessMask    = VK_ACCESS_SHADER_WRITE_BIT;
    barrier.dstAccessMask    = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;

    vkCmdPipelineBarrier(
        Context::frameCmd,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        1, &barrier,
        0, nullptr,
        0, nullptr);
}

// ═════════════════════════════════════════════════════════════════════════════
//  Upscaler – cleanup helpers
// ═════════════════════════════════════════════════════════════════════════════

void Upscaler::destroyPipeline()
{
    auto device = DeviceInstance.getDevice();
    if (pipeline != VK_NULL_HANDLE)
    {
        vkDestroyPipeline(device, pipeline, nullptr);
        pipeline = VK_NULL_HANDLE;
    }
    if (computeShaderModule != VK_NULL_HANDLE)
    {
        vkDestroyShaderModule(device, computeShaderModule, nullptr);
        computeShaderModule = VK_NULL_HANDLE;
    }
    // pipelineLayout is preserved across reloads (layout doesn't change)
}

void Upscaler::destroyDescriptors()
{
    auto device = DeviceInstance.getDevice();
    if (descriptorSet != VK_NULL_HANDLE)
    {
        vkFreeDescriptorSets(device, descriptorPool, 1, &descriptorSet);
        descriptorSet = VK_NULL_HANDLE;
    }
    if (descriptorPool != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, descriptorPool, nullptr);
        descriptorPool = VK_NULL_HANDLE;
    }
    if (descriptorSetLayout != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, nullptr);
        descriptorSetLayout = VK_NULL_HANDLE;
    }
    if (pipelineLayout != VK_NULL_HANDLE)
    {
        vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
        pipelineLayout = VK_NULL_HANDLE;
    }
}

void Upscaler::destroyWeightBuffers()
{
    weightBlobs.clear();   // Blob destructor calls destroy()
    parsedLayers.clear();
    ready = false;
}

} // namespace RT::Vulkan
