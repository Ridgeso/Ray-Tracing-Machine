#define _CRT_SECURE_NO_WARNINGS

/*
	Implementation file. Contains all dependencies implementation where is used single header approche.
	
	! Important: Include order may be important.
*/
#define TINYGLTF_IMPLEMENTATION
#include "tiny_gltf.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

/*
	ImGui backend is temporar and will be moved to CMake.
*/
//#include <backends/imgui_impl_opengl3.cpp>
#include <backends/imgui_impl_vulkan.cpp>
#include <backends/imgui_impl_glfw.cpp>
