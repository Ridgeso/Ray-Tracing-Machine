// ─────────────────────────────────────────────────────────────────────────────
//  Upscaler – Usage guide & integration example
//  (not compiled; paste into your render system)
// ─────────────────────────────────────────────────────────────────────────────

// 1. Construction  ─────────────────────────────────────────────────────────────
//
//    UpscalerSpec spec;
//    spec.paramPath   = "assets/models/realesrgan-x2plus.param";
//    spec.binPath     = "assets/models/realesrgan-x2plus.bin";
//    spec.shaderDir   = "assets/shaders/";
//    spec.mode        = UpscaleMode::Generic;   // model-driven
//    spec.scaleFactor = 2u;
//    spec.tiling      = true;
//    spec.tileSize    = { 512u, 512u };
//    spec.tilePad     = 10u;
//
//    auto upscaler = std::make_unique<Upscaler>(spec);

// 2. Texture setup  ────────────────────────────────────────────────────────────
//
//    // Source (e.g. rendered scene at half resolution)
//    auto src = std::make_unique<VulkanTexture>(glm::uvec2{960u, 540u}, Texture::Format::RGBA8);
//
//    // Destination (full resolution)
//    auto dst = std::make_unique<VulkanTexture>(glm::uvec2{1920u, 1080u}, Texture::Format::RGBA32F);

// 3. Per-frame dispatch  (inside beginFrame / endFrame)  ───────────────────────
//
//    // Ensure src is in shader-read layout before calling upscale()
//    src->transition(Texture::Access::Read, Texture::Layout::ShaderRead);
//
//    upscaler->upscale(*src, *dst);
//
//    // dst is automatically transitioned back to ShaderRead after upscale()
//    // – ready to be sampled in a subsequent fullscreen pass or ImGui display.

// 4. Hot model reload (e.g. user picks a different model at runtime)  ──────────
//
//    upscaler->reloadModel(
//        "assets/models/realesrgan-x4plus.param",
//        "assets/models/realesrgan-x4plus.bin");

// 5. Built-in modes (no model required)  ──────────────────────────────────────
//
//    UpscalerSpec bilinearSpec;
//    bilinearSpec.shaderDir   = "assets/shaders/";
//    bilinearSpec.mode        = UpscaleMode::Bilinear;
//    bilinearSpec.scaleFactor = 2u;
//    bilinearSpec.tiling      = false;     // bilinear doesn't need tiling
//
//    auto cheapUpscaler = std::make_unique<Upscaler>(bilinearSpec);

// ─────────────────────────────────────────────────────────────────────────────
//  Descriptor binding layout (matches Upscaler.h kBinding* constants)
//  ┌──────────┬───────────────────────────────────┬────────────────────────┐
//  │ binding  │ type                              │ purpose                │
//  ├──────────┼───────────────────────────────────┼────────────────────────┤
//  │    0     │ COMBINED_IMAGE_SAMPLER            │ src image              │
//  │    1     │ STORAGE_IMAGE                     │ dst image (write)      │
//  │    2     │ STORAGE_BUFFER (readonly in shdr) │ packed model weights   │
//  │    3     │ STORAGE_BUFFER                    │ intermediate scratch   │
//  └──────────┴───────────────────────────────────┴────────────────────────┘

// ─────────────────────────────────────────────────────────────────────────────
//  Push constant layout  (std430, 32 bytes)
//  struct UpscalePushConstants {
//      uvec2 srcSize;       // source image dimensions
//      uvec2 dstSize;       // destination image dimensions
//      uvec2 tileOffset;    // top-left of current tile in src pixels
//      uvec2 tileSize;      // current tile dimensions in src pixels
//      float scaleFactor;   // upscale ratio (2.0, 3.0, 4.0 …)
//      uint  channels;      // always 4 (RGBA)
//      uint  _pad0, _pad1;
//  };

// ─────────────────────────────────────────────────────────────────────────────
//  Expected NCNN .param format  (magic line 7767517)
//
//    7767517
//    23 22
//    Input            data          0 1 input
//    Convolution      conv1         1 1 input feat1 0=32 1=3 4=1 5=1 6=2304
//    ReLU             relu1         1 1 feat1 feat1r
//    Convolution      conv2         1 1 feat1r feat2 0=32 1=3 4=1 5=1 6=36864
//    ...
//    PixelShuffle     shuffle       1 1 last out 0=2
//
//  attr key reference (NCNN standard):
//    0  = num_output / out_channels
//    1  = kernel_w
//    2  = dilation_w
//    3  = stride_w
//    4  = pad_w
//    5  = bias_term
//    6  = weight_data_size
//    7  = bias_data_size   (defaults to num_output if absent)
