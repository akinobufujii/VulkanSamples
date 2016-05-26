//==============================================================================
// インクルード
//==============================================================================
#include <tchar.h>
#include <Windows.h>
#include <assert.h>
#include <vector>
#define _USE_MATH_DEFINES
#include <cmath>

#define VK_USE_PLATFORM_WIN32_KHR	// Win32用の拡張機能を有効にする
#define VK_PROTOTYPES

#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "MeshImporter.h"

//==============================================================================
// 定数
//==============================================================================
static const uint32_t SCREEN_WIDTH = 1280;					// 画面幅
static const uint32_t SCREEN_HEIGHT = 720;					// 画面高さ
static LPCTSTR	CLASS_NAME = TEXT("04_DrawMeshUseTexture");	// ウィンドウネーム
static LPCSTR APPLICATION_NAME = "04_DrawMeshUseTexture";	// アプリケーション名
static const UINT SWAP_CHAIN_COUNT = 2;						// スワップチェーン数
static const UINT TIMEOUT_NANO_SEC = 100000000;				// コマンド実行のタイムアウト時間(ナノ秒)

//==============================================================================
// 各種定義
//==============================================================================
// Vulkanの物理デバイス(GPU)情報
struct VulkanGPU
{
	VkPhysicalDevice					device;					// GPUデバイス
	VkPhysicalDeviceProperties			deviceProperties;		// GPUデバイスのプロパティ
	VkPhysicalDeviceMemoryProperties	deviceMemoryProperties;	// GPUデバイスのメモリプロパティ
};

// Vulkanのテクスチャ情報
struct VulkanTexture
{
	VkImage			image;		// イメージ
	VkImageView		view;		// ビュー
	VkDeviceMemory  memory;		// メモリ
	VkSampler		sampler;	// サンプラ
};

// ステージングバッファ
struct StagingBuffer
{
	VkDeviceMemory	memory;
	VkBuffer		buffer;
};

// ステージングバッファまとめ
struct StagingBuffers
{
	StagingBuffer vertices;	// 頂点ステージングバッファ
	StagingBuffer indices;	// インデックスステージングバッファ
};

// 頂点バッファ
struct VertexBuffer
{
	VkBuffer										buf;
	VkDeviceMemory									mem;
	VkPipelineVertexInputStateCreateInfo			vi;
	std::vector<VkVertexInputBindingDescription>	bindingDescriptions;
	std::vector<VkVertexInputAttributeDescription>	attributeDescriptions;
};

// インデックスバッファ
struct IndexBuffer
{
	int				count;
	VkBuffer		buf;
	VkDeviceMemory	mem;
};

// ユニフォームバッファ
struct UniformBuffer
{
	VkBuffer				buffer;
	VkDeviceMemory			memory;
	VkDescriptorBufferInfo	descriptor;
};

// ユニフォームバッファ用マトリックス構造体
struct UBOMatrices
{
	glm::mat4 projectionMatrix;
	glm::mat4 viewMatrix;
	glm::mat4 worldMatrix;
};

//==============================================================================
// グローバル変数
//==============================================================================
VkInstance									g_VulkanInstance				= nullptr;	// Vulkanのインスタンス
VkSurfaceKHR								g_VulkanSurface					= nullptr;	// Vulkanのサーフェス情報
std::vector<VulkanGPU>						g_GPUs							= {};		// GPU情報
VulkanGPU									g_currentGPU;								// 現在選択しているGPU情報
VkDevice									g_VulkanDevice					= nullptr;	// Vulkanデバイス
VkQueue										g_VulkanQueue					= nullptr;	// グラフィックスキュー
VkSemaphore									g_VulkanSemahoreRenderComplete	= nullptr;	// コマンドバッファ実行用セマフォ
VkCommandPool								g_VulkanCommandPool				= nullptr;	// コマンドプールオブジェクト
VkFence										g_VulkanFence					= nullptr;	// フェンスオブジェクト
std::vector<VkCommandBuffer>				g_commandBuffers				= {};		// コマンドバッファ配列(スワップチェーン数に依存)
uint32_t									g_currentBufferIndex			= 0;		// 現在のバッファインデックス
VkSwapchainKHR								g_VulkanSwapChain				= nullptr;	// スワップチェーンオブジェクト
std::vector<VulkanTexture>					g_backBuffersTextures			= {};		// バックバッファ情報
VulkanTexture								g_depthBufferTexture;						// 深度テクスチャ
std::vector<VkFramebuffer>					g_frameBuffers					= {};		// フレームバッファ配列
VkRenderPass								g_renderPass					= nullptr;	// レンダーパス
VkPipelineCache								g_pipelineCache					= nullptr;	// パイプラインキャッシュ

VertexBuffer								g_vertexBuffer;			// 頂点バッファ
IndexBuffer									g_indexBuffer;			// インデックスバッファ
VulkanTexture								g_meshTexture;			// メッシュのテクスチャ
UniformBuffer								g_uniformBuffer;		// ユニフォームバッファ
UBOMatrices									g_uboMatrices;			// ユニフォームバッファ用マトリックス
VkDescriptorPool							g_descriptorPool;		// ディスクリプタプール
VkDescriptorSetLayout						g_descriptorSetLayout;	// ディスクリプタセットレイアウト
VkDescriptorSet								g_descriptorSet;		// ディスクリプタセット
VkPipelineLayout							g_pipelineLayout;		// パイプラインレイアウト
VkPipeline									g_pipeline;				// パイプライン

//==============================================================================
// Vulkanエラーチェック用関数
//==============================================================================
void checkVulkanError(VkResult result, LPTSTR message)
{
	assert(result == VK_SUCCESS && message);
}

//==============================================================================
// シェーダ読み込み関数
//==============================================================================
VkShaderModule loadShader(const char *fileName, VkDevice device, VkShaderStageFlagBits stage)
{
	auto readBinaryFile = [](const char *filename, size_t *psize) -> char*
	{
		long int size;
		size_t retval;
		void* pShaderCode;

		FILE* fp = nullptr;
		errno_t error = fopen_s(&fp, filename, "rb");
		if(fp == nullptr)
		{
			return nullptr;
		}

		fseek(fp, 0L, SEEK_END);
		size = ftell(fp);

		fseek(fp, 0L, SEEK_SET);

		pShaderCode = malloc(size);
		retval = fread(pShaderCode, size, 1, fp);
		assert(retval == 1);
		fclose(fp);

		*psize = size;

		return static_cast<char*>(pShaderCode);
	};

	size_t size = 0;
	const char *shaderCode = readBinaryFile(fileName, &size);
	assert(size > 0);

	VkShaderModule shaderModule;
	VkShaderModuleCreateInfo moduleCreateInfo;

	moduleCreateInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
	moduleCreateInfo.pNext = nullptr;

	moduleCreateInfo.codeSize = size;
	moduleCreateInfo.pCode = (uint32_t*)shaderCode;
	moduleCreateInfo.flags = 0;
	VkResult result = vkCreateShaderModule(device, &moduleCreateInfo, nullptr, &shaderModule);
	checkVulkanError(result, TEXT("シェーダモジュール作成失敗"));

	return shaderModule;
}

//==============================================================================
// イメージレイアウト設定関数
//==============================================================================
void setImageLayout(
	VkDevice				device,
	VkCommandBuffer			commandBuffer,
	VkImage					image,
	VkImageAspectFlags		aspectFlags,
	VkImageLayout			oldLayout,
	VkImageLayout			newLayout,
	VkImageSubresourceRange	subresourceRange)
{
	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = 0;
	imageMemoryBarrier.oldLayout = oldLayout;
	imageMemoryBarrier.newLayout = newLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = subresourceRange;

	if(newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
	}

	if(newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	}

	if(newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
	}

	if(newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
	{
		imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_INPUT_ATTACHMENT_READ_BIT;
	}

	vkCmdPipelineBarrier(
		commandBuffer,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&imageMemoryBarrier);
}

//==============================================================================
// メモリタイプ獲得
//==============================================================================
bool getMemoryType(const VulkanGPU& gpu, uint32_t typeBits, VkFlags properties, uint32_t * typeIndex)
{
	for(uint32_t i = 0; i < 32; i++)
	{
		if((typeBits & 1) == 1)
		{
			if((gpu.deviceMemoryProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				*typeIndex = i;
				return true;
			}
		}
		typeBits >>= 1;
	}
	return false;
}

//==============================================================================
// Vulkan初期化
//==============================================================================
bool initVulkan(HINSTANCE hinst, HWND wnd)
{
	VkResult result;

	//==================================================
	// Vulkanのインスタンス作成
	//==================================================
	VkApplicationInfo applicationInfo = {};
	applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	applicationInfo.pApplicationName = APPLICATION_NAME;
	applicationInfo.pEngineName = APPLICATION_NAME;
	applicationInfo.apiVersion = VK_API_VERSION;

	std::vector<LPCSTR> enabledExtensionsByInstance = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME };

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;

	if(enabledExtensionsByInstance.empty() == false)
	{
		instanceCreateInfo.enabledExtensionCount = enabledExtensionsByInstance.size();
		instanceCreateInfo.ppEnabledExtensionNames = enabledExtensionsByInstance.data();
	}

	result = vkCreateInstance(&instanceCreateInfo, nullptr, &g_VulkanInstance);
	checkVulkanError(result, TEXT("Vulkanインスタンス作成失敗"));

	//==================================================
	// 物理デバイス(GPUデバイス)
	//==================================================
	// 物理デバイス数を獲得
	uint32_t gpuCount = 0;
	vkEnumeratePhysicalDevices(g_VulkanInstance, &gpuCount, nullptr);
	assert(gpuCount > 0 && TEXT("物理デバイス数の獲得失敗"));

	// 物理デバイス数を列挙
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	result = vkEnumeratePhysicalDevices(g_VulkanInstance, &gpuCount, physicalDevices.data());
	checkVulkanError(result, TEXT("物理デバイスの列挙に失敗しました"));

	// すべてのGPU情報を格納
	g_GPUs.resize(gpuCount);
	for(uint32_t i = 0; i < gpuCount; ++i)
	{
		g_GPUs[i].device = physicalDevices[i];

		// 物理デバイスのプロパティ獲得
		vkGetPhysicalDeviceProperties(g_GPUs[i].device, &g_GPUs[i].deviceProperties);

		// 物理デバイスのメモリプロパティ獲得
		vkGetPhysicalDeviceMemoryProperties(g_GPUs[i].device, &g_GPUs[i].deviceMemoryProperties);
	}

	// ※このサンプルでは最初に列挙されたGPUデバイスを使用する
	g_currentGPU = g_GPUs[0];

	// グラフィックス操作をサポートするキューを検索
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(g_currentGPU.device, &queueCount, nullptr);
	assert(queueCount >= 1 && TEXT("物理デバイスキューの検索失敗"));

	std::vector<VkQueueFamilyProperties> queueProps;
	queueProps.resize(queueCount);
	vkGetPhysicalDeviceQueueFamilyProperties(g_currentGPU.device, &queueCount, queueProps.data());

	uint32_t graphicsQueueIndex = 0;
	for(graphicsQueueIndex = 0; graphicsQueueIndex < queueCount; ++graphicsQueueIndex)
	{
		if(queueProps[graphicsQueueIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT)
		{
			break;
		}
	}
	assert(graphicsQueueIndex < queueCount && TEXT("グラフィックスをサポートするキューを見つけられませんでした"));

	//==================================================
	// Vulkanデバイス作成
	//==================================================
	float queuePrioritie = 0.0f;
	VkDeviceQueueCreateInfo queueCreateInfo = {};
	queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
	queueCreateInfo.queueFamilyIndex = graphicsQueueIndex;
	queueCreateInfo.queueCount = 1;
	queueCreateInfo.pQueuePriorities = &queuePrioritie;

	std::vector<LPCSTR> enabledExtensionsByDevice = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.pEnabledFeatures = nullptr;

	if(enabledExtensionsByDevice.empty() == false)
	{
		deviceCreateInfo.enabledExtensionCount = enabledExtensionsByDevice.size();
		deviceCreateInfo.ppEnabledExtensionNames = enabledExtensionsByDevice.data();
	}

	result = vkCreateDevice(g_currentGPU.device, &deviceCreateInfo, nullptr, &g_VulkanDevice);
	checkVulkanError(result, TEXT("Vulkanデバイス作成失敗"));

	// グラフィックスキュー獲得
	vkGetDeviceQueue(g_VulkanDevice, graphicsQueueIndex, 0, &g_VulkanQueue);

	//==================================================
	// フェンスオブジェクト作成
	//==================================================
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = 0;
	result = vkCreateFence(g_VulkanDevice, &fenceCreateInfo, nullptr, &g_VulkanFence);
	checkVulkanError(result, TEXT("フェンスオブジェクト作成失敗"));

	//==================================================
	// 同期（セマフォ）オブジェクト作成
	//==================================================
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	// コマンドバッファ実行用セマフォ作成
	result = vkCreateSemaphore(g_VulkanDevice, &semaphoreCreateInfo, nullptr, &g_VulkanSemahoreRenderComplete);
	checkVulkanError(result, TEXT("コマンドバッファ実行用セマフォ作成失敗"));

	//==================================================
	// コマンドプール作製
	//==================================================
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = 0;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	result = vkCreateCommandPool(g_VulkanDevice, &cmdPoolInfo, nullptr, &g_VulkanCommandPool);
	checkVulkanError(result, TEXT("コマンドプール作成失敗"));

	//==================================================
	// コマンドバッファ作成
	//==================================================
	// メモリを確保.
	g_commandBuffers.resize(SWAP_CHAIN_COUNT);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = g_VulkanCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = SWAP_CHAIN_COUNT;

	result = vkAllocateCommandBuffers(g_VulkanDevice, &commandBufferAllocateInfo, g_commandBuffers.data());
	checkVulkanError(result, TEXT("コマンドバッファ作成失敗"));

	VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {};
	commandBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	commandBufferInheritanceInfo.pNext = nullptr;
	commandBufferInheritanceInfo.renderPass = VK_NULL_HANDLE;
	commandBufferInheritanceInfo.subpass = 0;
	commandBufferInheritanceInfo.framebuffer = VK_NULL_HANDLE;
	commandBufferInheritanceInfo.occlusionQueryEnable = VK_FALSE;
	commandBufferInheritanceInfo.queryFlags = 0;
	commandBufferInheritanceInfo.pipelineStatistics = 0;

	VkCommandBufferBeginInfo commandBufferBeginInfo = {};
	commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	commandBufferBeginInfo.pNext = nullptr;
	commandBufferBeginInfo.flags = 0;
	commandBufferBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

	//==================================================
	// OS(今回はWin32)用のサーフェスを作成する
	//==================================================
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = hinst;
	surfaceCreateInfo.hwnd = wnd;
	result = vkCreateWin32SurfaceKHR(g_VulkanInstance, &surfaceCreateInfo, nullptr, &g_VulkanSurface);
	checkVulkanError(result, TEXT("サーフェス作成失敗"));

	//==================================================
	// スワップチェーンを作成する
	//==================================================
	VkFormat        imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	VkColorSpaceKHR imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

	uint32_t surfaceFormatCount = 0;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(g_currentGPU.device, g_VulkanSurface, &surfaceFormatCount, nullptr);
	checkVulkanError(result, TEXT("サポートしているカラーフォーマット数の獲得失敗"));

	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	surfaceFormats.resize(surfaceFormatCount);
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(g_currentGPU.device, g_VulkanSurface, &surfaceFormatCount, surfaceFormats.data());
	checkVulkanError(result, TEXT("サポートしているカラーフォーマットの獲得失敗"));

	// 一致するカラーフォーマットを検索する
	bool isFind = false;
	for(const auto& surfaceFormat : surfaceFormats)
	{
		if(imageFormat == surfaceFormat.format &&
			imageColorSpace == surfaceFormat.colorSpace)
		{
			isFind = true;
			break;
		}
	}

	if(isFind == false)
	{
		imageFormat = surfaceFormats[0].format;
		imageColorSpace = surfaceFormats[0].colorSpace;
	}

	// サーフェスの機能を獲得する
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkSurfaceTransformFlagBitsKHR surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		g_currentGPU.device,
		g_VulkanSurface,
		&surfaceCapabilities);
	checkVulkanError(result, TEXT("サーフェスの機能の獲得失敗"));

	if((surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0)
	{
		surfaceTransform = surfaceCapabilities.currentTransform;
	}

	// プレゼント機能を獲得する
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t presentModeCount;
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(
		g_currentGPU.device,
		g_VulkanSurface,
		&presentModeCount,
		nullptr);
	checkVulkanError(result, TEXT("プレゼント機能数の獲得失敗"));

	std::vector<VkPresentModeKHR> presentModes;
	presentModes.resize(presentModeCount);
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(
		g_currentGPU.device,
		g_VulkanSurface,
		&presentModeCount,
		presentModes.data());
	checkVulkanError(result, TEXT("プレゼント機能の獲得失敗"));

	for(const auto& presentModeInfo : presentModes)
	{
		if(presentModeInfo == VK_PRESENT_MODE_MAILBOX_KHR)
		{
			presentMode = VK_PRESENT_MODE_MAILBOX_KHR;
			break;
		}
		if(presentModeInfo == VK_PRESENT_MODE_IMMEDIATE_KHR)
		{
			presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
		}
	}

	presentModes.clear();

	uint32_t desiredSwapChainImageCount = surfaceCapabilities.minImageCount + 1;
	if(surfaceCapabilities.maxImageCount > 0 && desiredSwapChainImageCount > surfaceCapabilities.maxImageCount)
	{
		desiredSwapChainImageCount = surfaceCapabilities.maxImageCount;
	}

	// スワップチェーン作成
	VkSwapchainCreateInfoKHR swapchainCreateInfo = {};
	swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
	swapchainCreateInfo.pNext = nullptr;
	swapchainCreateInfo.flags = 0;
	swapchainCreateInfo.surface = g_VulkanSurface;
	swapchainCreateInfo.minImageCount = desiredSwapChainImageCount;
	swapchainCreateInfo.imageFormat = imageFormat;
	swapchainCreateInfo.imageColorSpace = imageColorSpace;
	swapchainCreateInfo.imageExtent = { SCREEN_WIDTH, SCREEN_HEIGHT };
	swapchainCreateInfo.imageArrayLayers = 1;
	swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
	swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
	swapchainCreateInfo.queueFamilyIndexCount = 0;
	swapchainCreateInfo.pQueueFamilyIndices = nullptr;
	swapchainCreateInfo.preTransform = surfaceTransform;
	swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
	swapchainCreateInfo.presentMode = presentMode;
	swapchainCreateInfo.clipped = VK_TRUE;
	swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE;

	result = vkCreateSwapchainKHR(g_VulkanDevice, &swapchainCreateInfo, nullptr, &g_VulkanSwapChain);
	checkVulkanError(result, TEXT("サーフェス作成失敗"));

	//==================================================
	// イメージの作成
	//==================================================
	uint32_t swapChainCount = 0;
	result = vkGetSwapchainImagesKHR(g_VulkanDevice, g_VulkanSwapChain, &swapChainCount, nullptr);
	checkVulkanError(result, TEXT("スワップチェーンイメージ数の獲得失敗"));

	g_backBuffersTextures.resize(swapChainCount);

	std::vector<VkImage> images;
	images.resize(swapChainCount);
	result = vkGetSwapchainImagesKHR(g_VulkanDevice, g_VulkanSwapChain, &swapChainCount, images.data());
	checkVulkanError(result, TEXT("スワップチェーンイメージの獲得失敗"));

	for(uint32_t i = 0; i < swapChainCount; ++i)
	{
		g_backBuffersTextures[i].image = images[i];
	}

	images.clear();

	//==================================================
	// イメージビューの生成
	//==================================================
	for(auto& backBuffer : g_backBuffersTextures)
	{
		VkImageViewCreateInfo imageViewCreateInfo = {};
		imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
		imageViewCreateInfo.pNext = nullptr;
		imageViewCreateInfo.flags = 0;
		imageViewCreateInfo.image = backBuffer.image;
		imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
		imageViewCreateInfo.format = imageFormat;
		imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
		imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
		imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
		imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
		imageViewCreateInfo.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };

		result = vkCreateImageView(g_VulkanDevice, &imageViewCreateInfo, nullptr, &backBuffer.view);
		checkVulkanError(result, TEXT("イメージビューの作成失敗"));

		setImageLayout(
			g_VulkanDevice,
			g_commandBuffers[g_currentBufferIndex],
			backBuffer.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
			{ VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
	}

	//==================================================
	// 深度ステンシルバッファの生成
	//==================================================
	VkFormat depthFormat = VK_FORMAT_D24_UNORM_S8_UINT;

	VkImageTiling imageTiling;
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(g_currentGPU.device, depthFormat, &formatProperties);

	if(formatProperties.linearTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		imageTiling = VK_IMAGE_TILING_LINEAR;
	}
	else if(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
	{
		imageTiling = VK_IMAGE_TILING_OPTIMAL;
	}
	else
	{
		checkVulkanError(VK_RESULT_MAX_ENUM, TEXT("サポートされていないフォーマットです"));
		return false;
	}

	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.flags = 0;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = depthFormat;
	imageCreateInfo.extent.width = SCREEN_WIDTH;
	imageCreateInfo.extent.height = SCREEN_HEIGHT;
	imageCreateInfo.extent.depth = 1;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = imageTiling;
	imageCreateInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.queueFamilyIndexCount = 0;
	imageCreateInfo.pQueueFamilyIndices = nullptr;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

	result = vkCreateImage(g_VulkanDevice, &imageCreateInfo, nullptr, &g_depthBufferTexture.image);
	checkVulkanError(result, TEXT("深度テクスチャ用イメージビュー作成失敗"));

	// メモリ要件を獲得
	VkMemoryRequirements memoryRequirements;
	vkGetImageMemoryRequirements(g_VulkanDevice, g_depthBufferTexture.image, &memoryRequirements);

	VkFlags requirementsMask = 0;
	uint32_t typeBits = memoryRequirements.memoryTypeBits;
	uint32_t typeIndex = 0;

	for(const auto& memoryType : g_currentGPU.deviceMemoryProperties.memoryTypes)
	{
		if((typeBits & 0x1) == 1)
		{
			if((memoryType.propertyFlags & requirementsMask) == requirementsMask)
			{
				break;
			}
		}
		typeBits >>= 1;
		++typeIndex;
	}

	// メモリ確保
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = typeIndex;

	result = vkAllocateMemory(g_VulkanDevice, &allocInfo, nullptr, &g_depthBufferTexture.memory);
	checkVulkanError(result, TEXT("深度テクスチャ用メモリ確保失敗"));

	result = vkBindImageMemory(g_VulkanDevice, g_depthBufferTexture.image, g_depthBufferTexture.memory, 0);
	checkVulkanError(result, TEXT("深度テクスチャメモリにバインド失敗"));

	VkImageViewCreateInfo imageViewCreateInfo = {};
	imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	imageViewCreateInfo.pNext = nullptr;
	imageViewCreateInfo.image = g_depthBufferTexture.image;
	imageViewCreateInfo.format = depthFormat;
	imageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_R;
	imageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_G;
	imageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_B;
	imageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_A;
	imageViewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
	imageViewCreateInfo.subresourceRange.levelCount = 1;
	imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
	imageViewCreateInfo.subresourceRange.layerCount = 1;
	imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
	imageViewCreateInfo.flags = 0;

	result = vkCreateImageView(g_VulkanDevice, &imageViewCreateInfo, nullptr, &g_depthBufferTexture.view);
	checkVulkanError(result, TEXT("深度テクスチャイメージビュー作成失敗"));

	setImageLayout(
		g_VulkanDevice,
		g_commandBuffers[g_currentBufferIndex],
		g_depthBufferTexture.image,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		{ VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1 });

	//==================================================
	// フレームバッファの生成
	//==================================================
	VkImageView imageViews[2];	// 0=カラーバッファ、1=深度バッファ

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = nullptr;
	frameBufferCreateInfo.flags = 0;
	frameBufferCreateInfo.renderPass = VK_NULL_HANDLE;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = imageViews;
	frameBufferCreateInfo.width = SCREEN_WIDTH;
	frameBufferCreateInfo.height = SCREEN_HEIGHT;
	frameBufferCreateInfo.layers = 1;

	g_frameBuffers.resize(SWAP_CHAIN_COUNT);
	for(uint32_t i = 0; i < SWAP_CHAIN_COUNT; ++i)
	{
		imageViews[0] = g_backBuffersTextures[i].view;
		imageViews[1] = g_depthBufferTexture.view;
		auto result = vkCreateFramebuffer(g_VulkanDevice, &frameBufferCreateInfo, nullptr, &g_frameBuffers[i]);
		checkVulkanError(result, TEXT("フレームバッファ作成失敗"));
	}

	//==================================================
	// コマンドを実行
	//==================================================
	VkPipelineStageFlags pipeStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = &pipeStageFlags;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &g_commandBuffers[g_currentBufferIndex];
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	result = vkQueueSubmit(g_VulkanQueue, 1, &submitInfo, VK_NULL_HANDLE);
	checkVulkanError(result, TEXT("グラフィックキューのサブミットに失敗"));

	result = vkQueueWaitIdle(g_VulkanQueue);
	checkVulkanError(result, TEXT("グラフィックキューのアイドル待ちに失敗"));

	//==================================================
	// フレームを用意
	//==================================================
	result = vkAcquireNextImageKHR(
		g_VulkanDevice,
		g_VulkanSwapChain,
		UINT64_MAX,
		g_VulkanSemahoreRenderComplete,
		nullptr,
		&g_currentBufferIndex);
	checkVulkanError(result, TEXT("次の有効なイメージインデックスの獲得に失敗"));

	//==================================================
	// レンダーパスを作成
	//==================================================
	VkAttachmentDescription attachments[2];
	attachments[0].format = VK_FORMAT_B8G8R8A8_UNORM;
	attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[0].initialLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	attachments[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	attachments[1].format = depthFormat;
	attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
	attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
	attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
	attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
	attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
	attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
	attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkAttachmentReference colorReference = {};
	colorReference.attachment = 0;
	colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

	VkAttachmentReference depthReference = {};
	depthReference.attachment = 1;
	depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

	VkSubpassDescription subpass = {};
	subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
	subpass.flags = 0;
	subpass.inputAttachmentCount = 0;
	subpass.pInputAttachments = nullptr;
	subpass.colorAttachmentCount = 1;
	subpass.pColorAttachments = &colorReference;
	subpass.pResolveAttachments = nullptr;
	subpass.pDepthStencilAttachment = &depthReference;
	subpass.preserveAttachmentCount = 0;
	subpass.pPreserveAttachments = nullptr;

	VkRenderPassCreateInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
	renderPassInfo.pNext = nullptr;
	renderPassInfo.attachmentCount = 2;
	renderPassInfo.pAttachments = attachments;
	renderPassInfo.subpassCount = 1;
	renderPassInfo.pSubpasses = &subpass;
	renderPassInfo.dependencyCount = 0;
	renderPassInfo.pDependencies = nullptr;

	result = vkCreateRenderPass(g_VulkanDevice, &renderPassInfo, nullptr, &g_renderPass);
	checkVulkanError(result, TEXT("レンダーパスの作成に失敗"));

	//==================================================
	// パイプラインキャッシュを作成
	//==================================================
	VkPipelineCacheCreateInfo pipelineCacheCreateInfo = {};
	pipelineCacheCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
	result = vkCreatePipelineCache(g_VulkanDevice, &pipelineCacheCreateInfo, nullptr, &g_pipelineCache);
	checkVulkanError(result, TEXT("パイプラインキャッシュの作成に失敗"));

	return true;
}

//==============================================================================
// Vulkan破棄
//==============================================================================
void destroyVulkan()
{
	if(g_pipelineCache)
	{
		vkDestroyPipelineCache(g_VulkanDevice, g_pipelineCache, nullptr);
	}
	
	if(g_renderPass)
	{
		vkDestroyRenderPass(g_VulkanDevice, g_renderPass, nullptr);
	}

	for(auto& frameBuffer : g_frameBuffers)
	{
		vkDestroyFramebuffer(g_VulkanDevice, frameBuffer, nullptr);
	}

	if(g_depthBufferTexture.view)
	{
		vkDestroyImageView(g_VulkanDevice, g_depthBufferTexture.view, nullptr);
	}

	if(g_depthBufferTexture.image)
	{
		vkDestroyImage(g_VulkanDevice, g_depthBufferTexture.image, nullptr);
	}

	if(g_depthBufferTexture.memory)
	{
		vkFreeMemory(g_VulkanDevice, g_depthBufferTexture.memory, nullptr);
	}

	if(g_commandBuffers.empty() == false)
	{
		vkFreeCommandBuffers(g_VulkanDevice, g_VulkanCommandPool, SWAP_CHAIN_COUNT, g_commandBuffers.data());
	}

	if(g_VulkanCommandPool)
	{
		vkDestroyCommandPool(g_VulkanDevice, g_VulkanCommandPool, nullptr);
	}

	if(g_VulkanSemahoreRenderComplete)
	{
		vkDestroySemaphore(g_VulkanDevice, g_VulkanSemahoreRenderComplete, nullptr);
	}

	if(g_VulkanFence)
	{
		vkDestroyFence(g_VulkanDevice, g_VulkanFence, nullptr);
	}

	if(g_VulkanSwapChain)
	{
		vkDestroySwapchainKHR(g_VulkanDevice, g_VulkanSwapChain, nullptr);
	}

	if(g_VulkanSurface)
	{
		vkDestroySurfaceKHR(g_VulkanInstance, g_VulkanSurface, nullptr);
	}

	if(g_VulkanDevice)
	{
		vkDestroyDevice(g_VulkanDevice, nullptr);
	}

	if(g_VulkanInstance)
	{
		vkDestroyInstance(g_VulkanInstance, nullptr);
	}

	g_frameBuffers.clear();
	g_commandBuffers.clear();

	g_pipelineCache = nullptr;
	g_renderPass = nullptr;
	g_VulkanSurface = nullptr;
	g_VulkanSwapChain = nullptr;
	g_VulkanCommandPool = nullptr;
	g_VulkanSemahoreRenderComplete = nullptr;
	g_VulkanFence = nullptr;
	g_VulkanQueue = nullptr;
	g_VulkanDevice = nullptr;
	g_VulkanInstance = nullptr;
}

//==============================================================================
// リソース作成
//==============================================================================
bool createResource()
{
	VkResult result;
	void* pData;

	VkMemoryAllocateInfo memoryAlloc = {};
	memoryAlloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	VkMemoryRequirements memReqs;

	StagingBuffers stagingBuffers;

	//==================================================
	// コピー用コマンドバッファ作成
	//==================================================
	VkCommandBufferAllocateInfo cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufInfo.commandPool = g_VulkanCommandPool;
	cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufInfo.commandBufferCount = 1;

	VkCommandBuffer copyCommandBuffer;
	result = vkAllocateCommandBuffers(g_VulkanDevice, &cmdBufInfo, &copyCommandBuffer);
	checkVulkanError(result, TEXT("コピー用コマンドバッファ作成に失敗"));

	//==================================================
	// メッシュデータ読み込み
	//==================================================
	MeshImporter meshImporter;
	meshImporter.loadMesh("Bandouiruka0.3ds", 8, true);
	const auto& meshDatum = meshImporter.getMeshDatum();

	//==================================================
	// 頂点バッファセットアップ
	//==================================================
	std::vector<MeshImporter::MeshData::VertexFormat> vertexBuffer;

	for(const auto& meshData : meshDatum)
	{
		for(const auto& vertexData : meshData.vertices)
		{
			vertexBuffer.emplace_back(vertexData);
		}
	}

	int vertexBufferSize = vertexBuffer.size() * sizeof(MeshImporter::MeshData::VertexFormat);

	// 頂点バッファ用ステージングバッファ作成
	VkBufferCreateInfo vertexBufferInfo = {};
	vertexBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	vertexBufferInfo.size = vertexBufferSize;
	vertexBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	result = vkCreateBuffer(g_VulkanDevice, &vertexBufferInfo, nullptr, &stagingBuffers.vertices.buffer);
	checkVulkanError(result, TEXT("ステージング用頂点バッファ作成に失敗"));

	// 頂点バッファ用ステージングバッファメモリ作成
	vkGetBufferMemoryRequirements(g_VulkanDevice, stagingBuffers.vertices.buffer, &memReqs);
	memoryAlloc.allocationSize = memReqs.size;
	getMemoryType(g_currentGPU, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memoryAlloc.memoryTypeIndex);
	result = vkAllocateMemory(g_VulkanDevice, &memoryAlloc, nullptr, &stagingBuffers.vertices.memory);
	checkVulkanError(result, TEXT("ステージング用頂点バッファメモリ確保に失敗"));

	// メモリマッピング
	result = vkMapMemory(g_VulkanDevice, stagingBuffers.vertices.memory, 0, memoryAlloc.allocationSize, 0, &pData);
	checkVulkanError(result, TEXT("ステージング用頂点バッファメモリマッピングに失敗"));

	// 内容をコピー
	memcpy(pData, vertexBuffer.data(), vertexBufferSize);

	// メモリアンマップしてバッファをバインドする
	vkUnmapMemory(g_VulkanDevice, stagingBuffers.vertices.memory);
	result = vkBindBufferMemory(g_VulkanDevice, stagingBuffers.vertices.buffer, stagingBuffers.vertices.memory, 0);
	checkVulkanError(result, TEXT("バッファのバインドに失敗"));

	// デバイスにのみ見えるデスティネーションバッファを作成する
	// バッファは頂点バッファとして使用します
	vertexBufferInfo.usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	result = vkCreateBuffer(g_VulkanDevice, &vertexBufferInfo, nullptr, &g_vertexBuffer.buf);
	checkVulkanError(result, TEXT("頂点バッファ作成に失敗"));

	vkGetBufferMemoryRequirements(g_VulkanDevice, g_vertexBuffer.buf, &memReqs);
	memoryAlloc.allocationSize = memReqs.size;
	getMemoryType(g_currentGPU, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAlloc.memoryTypeIndex);
	result = vkAllocateMemory(g_VulkanDevice, &memoryAlloc, nullptr, &g_vertexBuffer.mem);
	checkVulkanError(result, TEXT("頂点バッファのメモリ確保に失敗"));

	result = vkBindBufferMemory(g_VulkanDevice, g_vertexBuffer.buf, g_vertexBuffer.mem, 0);
	checkVulkanError(result, TEXT("バッファのバインドに失敗"));

	//==================================================
	// インデックスバッファセットアップ
	//==================================================
	std::vector<uint32_t> indexBuffer;

	for(const auto& meshData : meshDatum)
	{
		for(const auto& indexData : meshData.indices)
		{
			indexBuffer.emplace_back(indexData);
		}
	}

	uint32_t indexBufferSize = indexBuffer.size() * sizeof(uint32_t);
	g_indexBuffer.count = indexBuffer.size();

	// インデックスバッファ用ステージングバッファ作成
	VkBufferCreateInfo indexbufferInfo = {};
	indexbufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	indexbufferInfo.size = indexBufferSize;
	indexbufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	result = vkCreateBuffer(g_VulkanDevice, &indexbufferInfo, nullptr, &stagingBuffers.indices.buffer);
	checkVulkanError(result, TEXT("ステージング用インデックスバッファ作成に失敗"));

	// インデックスバッファ用ステージングバッファメモリ作成
	vkGetBufferMemoryRequirements(g_VulkanDevice, stagingBuffers.indices.buffer, &memReqs);
	memoryAlloc.allocationSize = memReqs.size;
	getMemoryType(g_currentGPU, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memoryAlloc.memoryTypeIndex);
	result = vkAllocateMemory(g_VulkanDevice, &memoryAlloc, nullptr, &stagingBuffers.indices.memory);
	checkVulkanError(result, TEXT("ステージング用インデックスバッファメモリ確保に失敗"));

	// メモリマッピング
	result = vkMapMemory(g_VulkanDevice, stagingBuffers.indices.memory, 0, indexBufferSize, 0, &pData);
	checkVulkanError(result, TEXT("ステージング用インデックスバッファメモリマッピングに失敗"));

	// 内容をコピー
	memcpy(pData, indexBuffer.data(), indexBufferSize);

	// メモリアンマップしてバッファをバインドする
	vkUnmapMemory(g_VulkanDevice, stagingBuffers.indices.memory);
	result = vkBindBufferMemory(g_VulkanDevice, stagingBuffers.indices.buffer, stagingBuffers.indices.memory, 0);
	checkVulkanError(result, TEXT("バッファのバインドに失敗"));

	// デバイスにのみ見えるデスティネーションバッファを作成する
	// インデックスバッファは頂点バッファとして使用します
	indexbufferInfo.usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
	result = vkCreateBuffer(g_VulkanDevice, &indexbufferInfo, nullptr, &g_indexBuffer.buf);
	checkVulkanError(result, TEXT("インデックスバッファ作成に失敗"));

	vkGetBufferMemoryRequirements(g_VulkanDevice, g_indexBuffer.buf, &memReqs);
	memoryAlloc.allocationSize = memReqs.size;
	getMemoryType(g_currentGPU, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAlloc.memoryTypeIndex);
	result = vkAllocateMemory(g_VulkanDevice, &memoryAlloc, nullptr, &g_indexBuffer.mem);
	checkVulkanError(result, TEXT("インデックスバッファのメモリ確保に失敗"));

	result = vkBindBufferMemory(g_VulkanDevice, g_indexBuffer.buf, g_indexBuffer.mem, 0);
	checkVulkanError(result, TEXT("バッファのバインドに失敗"));
	g_indexBuffer.count = indexBuffer.size();

	VkCommandBufferBeginInfo cmdBufferBeginInfo = {};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = nullptr;

	VkBufferCopy copyRegion = {};

	// コマンドバッファ発行
	vkBeginCommandBuffer(copyCommandBuffer, &cmdBufferBeginInfo);

	// 頂点バッファ
	copyRegion.size = vertexBufferSize;
	vkCmdCopyBuffer(
		copyCommandBuffer,
		stagingBuffers.vertices.buffer,
		g_vertexBuffer.buf,
		1,
		&copyRegion);

	// インデックスバッファ
	copyRegion.size = indexBufferSize;
	vkCmdCopyBuffer(
		copyCommandBuffer,
		stagingBuffers.indices.buffer,
		g_indexBuffer.buf,
		1,
		&copyRegion);

	vkEndCommandBuffer(copyCommandBuffer);

	// キューにサブミットする
	VkSubmitInfo copySubmitInfo = {};
	copySubmitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	copySubmitInfo.commandBufferCount = 1;
	copySubmitInfo.pCommandBuffers = &copyCommandBuffer;

	result = vkQueueSubmit(g_VulkanQueue, 1, &copySubmitInfo, VK_NULL_HANDLE);
	checkVulkanError(result, TEXT("グラフィックスキューへのサブミット失敗"));

	result = vkQueueWaitIdle(g_VulkanQueue);
	checkVulkanError(result, TEXT("グラフィックキューのアイドル待ちに失敗"));

	vkFreeCommandBuffers(g_VulkanDevice, g_VulkanCommandPool, 1, &copyCommandBuffer);

	// ステージングバッファを破棄する
	vkDestroyBuffer(g_VulkanDevice, stagingBuffers.vertices.buffer, nullptr);
	vkFreeMemory(g_VulkanDevice, stagingBuffers.vertices.memory, nullptr);
	vkDestroyBuffer(g_VulkanDevice, stagingBuffers.indices.buffer, nullptr);
	vkFreeMemory(g_VulkanDevice, stagingBuffers.indices.memory, nullptr);

	// バインディング情報を設定
	g_vertexBuffer.bindingDescriptions.resize(1);
	g_vertexBuffer.bindingDescriptions[0].binding = 0;
	g_vertexBuffer.bindingDescriptions[0].stride = sizeof(MeshImporter::MeshData::VertexFormat);
	g_vertexBuffer.bindingDescriptions[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

	// 頂点属性を設定
	g_vertexBuffer.attributeDescriptions.resize(4);

	// location 0 : 頂点座標
	g_vertexBuffer.attributeDescriptions[0].binding = 0;
	g_vertexBuffer.attributeDescriptions[0].location = 0;
	g_vertexBuffer.attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
	g_vertexBuffer.attributeDescriptions[0].offset = 0;
	g_vertexBuffer.attributeDescriptions[0].binding = 0;

	// Location 1 : UV
	g_vertexBuffer.attributeDescriptions[1].binding = 0;
	g_vertexBuffer.attributeDescriptions[1].location = 1;
	g_vertexBuffer.attributeDescriptions[1].format = VK_FORMAT_R32G32_SFLOAT;
	g_vertexBuffer.attributeDescriptions[1].offset = sizeof(aiVector3D);
	g_vertexBuffer.attributeDescriptions[1].binding = 0;
	
	// Location 2 : 法線
	g_vertexBuffer.attributeDescriptions[2].binding = 0;
	g_vertexBuffer.attributeDescriptions[2].location = 2;
	g_vertexBuffer.attributeDescriptions[2].format = VK_FORMAT_R32G32B32_SFLOAT;
	g_vertexBuffer.attributeDescriptions[2].offset = sizeof(aiVector3D) + sizeof(aiVector2D);
	g_vertexBuffer.attributeDescriptions[2].binding = 0;

	// Location 3 : 頂点色
	g_vertexBuffer.attributeDescriptions[3].binding = 0;
	g_vertexBuffer.attributeDescriptions[3].location = 3;
	g_vertexBuffer.attributeDescriptions[3].format = VK_FORMAT_R32G32B32_SFLOAT;
	g_vertexBuffer.attributeDescriptions[3].offset = sizeof(aiVector3D) + sizeof(aiVector2D) + sizeof(aiVector3D);
	g_vertexBuffer.attributeDescriptions[3].binding = 0;

	// 頂点バッファをアサインする
	g_vertexBuffer.vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	g_vertexBuffer.vi.pNext = nullptr;
	g_vertexBuffer.vi.vertexBindingDescriptionCount = g_vertexBuffer.bindingDescriptions.size();
	g_vertexBuffer.vi.pVertexBindingDescriptions = g_vertexBuffer.bindingDescriptions.data();
	g_vertexBuffer.vi.vertexAttributeDescriptionCount = g_vertexBuffer.attributeDescriptions.size();
	g_vertexBuffer.vi.pVertexAttributeDescriptions = g_vertexBuffer.attributeDescriptions.data();

	//==================================================
	// テクスチャ読み込み(テクスチャは一つとして仮定する)
	//==================================================
	VkFormat format = VK_FORMAT_R8G8B8A8_UNORM;

	// 画像データを読み込み
	int width;
	int height;
	int bpp;

	stbi_uc* pPixelData = stbi_load(meshDatum.front().textureName.c_str(), &width, &height, &bpp, 4);

	// 要求されたテクスチャフォーマットに対してのプロパティを獲得する
	VkFormatProperties formatProperties;
	vkGetPhysicalDeviceFormatProperties(g_currentGPU.device, format, &formatProperties);

	// テクスチャ作成用のコマンドバッファを作成
	cmdBufInfo = {};
	cmdBufInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	cmdBufInfo.commandPool = g_VulkanCommandPool;
	cmdBufInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	cmdBufInfo.commandBufferCount = 1;

	VkCommandBuffer textureCmdBuffer;
	result = vkAllocateCommandBuffers(g_VulkanDevice, &cmdBufInfo, &textureCmdBuffer);
	checkVulkanError(result, TEXT("テクスチャ作成用のコマンドバッファ作成失敗"));

	// テクスチャロードするための個別のコマンドバッファを使用する
	cmdBufferBeginInfo = {};
	cmdBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBufferBeginInfo.pNext = NULL;
	result = vkBeginCommandBuffer(textureCmdBuffer, &cmdBufferBeginInfo);
	checkVulkanError(result, TEXT("テクスチャ作成用のコマンドバッファ記録開始に失敗"));

	// 画像データを持ったステージングバッファを作成
	VkBuffer stagingBuffer;
	VkDeviceMemory stagingMemory;

	VkBufferCreateInfo bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = width * height * 4;	// 幅＊高さ＊1ピクセルの要素

	// このバッファはバッファコピーの転送元として使用する
	bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
	bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

	result = vkCreateBuffer(g_VulkanDevice, &bufferCreateInfo, nullptr, &stagingBuffer);
	checkVulkanError(result, TEXT("テクスチャ作成用のコマンドバッファ作成失敗"));

	// ステージングバッファのメモリ要件を獲得
	vkGetBufferMemoryRequirements(g_VulkanDevice, stagingBuffer, &memReqs);

	VkMemoryAllocateInfo memoryAllocateInfo = {};
	memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	memoryAllocateInfo.pNext = nullptr;
	memoryAllocateInfo.allocationSize = 0;
	memoryAllocateInfo.memoryTypeIndex = 0;
	memoryAllocateInfo.allocationSize = memReqs.size;
	getMemoryType(g_currentGPU, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &memoryAllocateInfo.memoryTypeIndex);

	result = vkAllocateMemory(g_VulkanDevice, &memoryAllocateInfo, nullptr, &stagingMemory);
	checkVulkanError(result, TEXT("テクスチャ作成用のステージングバッファ作成失敗"));

	result = vkBindBufferMemory(g_VulkanDevice, stagingBuffer, stagingMemory, 0);
	checkVulkanError(result, TEXT("テクスチャ作成用のステージングバッファのメモリバインディングに失敗"));

	// テクスチャデータをステージングバッファコピー
	result = vkMapMemory(g_VulkanDevice, stagingMemory, 0, memReqs.size, 0, reinterpret_cast<void**>(&pData));
	checkVulkanError(result, TEXT("テクスチャ作成用のステージングバッファのメモリマッピングに失敗"));

	memcpy(pData, pPixelData, bufferCreateInfo.size);
	vkUnmapMemory(g_VulkanDevice, stagingMemory);

	// バッファイメージの設定
	VkBufferImageCopy bufferCopyRegion = {};
	bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	bufferCopyRegion.imageSubresource.mipLevel = 0;
	bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
	bufferCopyRegion.imageSubresource.layerCount = 1;
	bufferCopyRegion.imageExtent.width = width;
	bufferCopyRegion.imageExtent.height = height;
	bufferCopyRegion.imageExtent.depth = 1;
	bufferCopyRegion.bufferOffset = 0;

	// 最適化したタイル状のターゲットイメージを作成する
	VkImageCreateInfo imageCreateInfo = {};
	imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
	imageCreateInfo.pNext = nullptr;
	imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
	imageCreateInfo.format = format;
	imageCreateInfo.mipLevels = 1;
	imageCreateInfo.arrayLayers = 1;
	imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
	imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
	imageCreateInfo.usage = VK_IMAGE_USAGE_SAMPLED_BIT;
	imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
	imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_PREINITIALIZED;
	imageCreateInfo.extent = { static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1 };
	imageCreateInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;

	result = vkCreateImage(g_VulkanDevice, &imageCreateInfo, nullptr, &g_meshTexture.image);
	checkVulkanError(result, TEXT("テクスチャ作成に失敗"));

	vkGetImageMemoryRequirements(g_VulkanDevice, g_meshTexture.image, &memReqs);

	memoryAllocateInfo.allocationSize = memReqs.size;

	getMemoryType(g_currentGPU, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &memoryAllocateInfo.memoryTypeIndex);
	
	result = vkAllocateMemory(g_VulkanDevice, &memoryAllocateInfo, nullptr, &g_meshTexture.memory);
	checkVulkanError(result, TEXT("テクスチャ用メモリー作成に失敗"));

	result = vkBindImageMemory(g_VulkanDevice, g_meshTexture.image, g_meshTexture.memory, 0);
	checkVulkanError(result, TEXT("テクスチャ用メモリーをバインドに失敗"));

	VkImageSubresourceRange subresourceRange = {};
	subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	subresourceRange.baseMipLevel = 0;
	subresourceRange.levelCount = 1;
	subresourceRange.layerCount = 1;

	// イメージレイアウト設定
	setImageLayout(
		g_VulkanDevice,
		textureCmdBuffer,
		g_meshTexture.image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_PREINITIALIZED,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		subresourceRange);

	// ステージングバッファにテクスチャをコピー
	vkCmdCopyBufferToImage(
		textureCmdBuffer,
		stagingBuffer,
		g_meshTexture.image,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		1,
		&bufferCopyRegion);

	// イメージをシェーダから読み取れるようにする
	setImageLayout(
		g_VulkanDevice,
		textureCmdBuffer,
		g_meshTexture.image,
		VK_IMAGE_ASPECT_COLOR_BIT,
		VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
		VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
		subresourceRange);

	// 描画コマンドの記録終了
	result = vkEndCommandBuffer(textureCmdBuffer);
	checkVulkanError(result, TEXT("テクスチャ作成用のコマンドバッファ記録終了に失敗"));

	// コピーする前に描画コマンドの終了待ちを行うためのフェンスを作成する
	VkFence copyFence;
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.flags = 0;
	result= vkCreateFence(g_VulkanDevice, &fenceCreateInfo, nullptr, &copyFence);
	checkVulkanError(result, TEXT("フェンスの作成に失敗"));

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &textureCmdBuffer;

	result = vkQueueSubmit(g_VulkanQueue, 1, &submitInfo, copyFence);
	checkVulkanError(result, TEXT("キューにサブミットに失敗"));

	result = vkWaitForFences(g_VulkanDevice, 1, &copyFence, VK_TRUE, TIMEOUT_NANO_SEC);
	checkVulkanError(result, TEXT("フェンス待ちに失敗"));

	vkDestroyFence(g_VulkanDevice, copyFence, nullptr);

	// ステージングバッファ用リソースをクリーンアップ
	vkFreeMemory(g_VulkanDevice, stagingMemory, nullptr);
	vkDestroyBuffer(g_VulkanDevice, stagingBuffer, nullptr);

	//==================================================
	// サンプラ作成
	//==================================================
	VkSamplerCreateInfo sampler = {};
	sampler.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
	sampler.magFilter = VK_FILTER_LINEAR;
	sampler.minFilter = VK_FILTER_LINEAR;
	sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
	sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
	sampler.mipLodBias = 0.0f;
	sampler.compareOp = VK_COMPARE_OP_NEVER;
	sampler.minLod = 0.0f;
	sampler.maxLod = 1;
	sampler.maxAnisotropy = 8;
	sampler.anisotropyEnable = VK_TRUE;
	sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
	result = vkCreateSampler(g_VulkanDevice, &sampler, nullptr, &g_meshTexture.sampler);
	checkVulkanError(result, TEXT("サンプラ作成に失敗"));

	// イメージビューを作成
	VkImageViewCreateInfo view = {};
	view.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
	view.pNext = nullptr;
	view.image = VK_NULL_HANDLE;
	view.viewType = VK_IMAGE_VIEW_TYPE_2D;
	view.format = format;
	view.components = { VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B, VK_COMPONENT_SWIZZLE_A };
	view.subresourceRange = { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 };
	view.subresourceRange.levelCount = 1;
	view.image = g_meshTexture.image;
	result = vkCreateImageView(g_VulkanDevice, &view, nullptr, &g_meshTexture.view);
	checkVulkanError(result, TEXT("イメージビューの作成に失敗"));

	// コマンドバッファ開放
	vkFreeCommandBuffers(g_VulkanDevice, g_VulkanCommandPool, 1, &textureCmdBuffer);

	// ピクセルデータ開放
	stbi_image_free(pPixelData);

	//==================================================
	// 各種シェーダ作成
	//==================================================
	VkPipelineShaderStageCreateInfo shaderStageCreateInfo[2];	// 頂点シェーダとフラグメントシェーダ
	
	// 頂点シェーダ作成
	shaderStageCreateInfo[0] = {};
	shaderStageCreateInfo[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
	shaderStageCreateInfo[0].module = loadShader("meshUseTexture.vert.spv", g_VulkanDevice, VK_SHADER_STAGE_VERTEX_BIT);
	shaderStageCreateInfo[0].pName = "main";

	// フラグメントシェーダ作成
	shaderStageCreateInfo[1] = {};
	shaderStageCreateInfo[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
	shaderStageCreateInfo[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
	shaderStageCreateInfo[1].module = loadShader("meshUseTexture.frag.spv", g_VulkanDevice, VK_SHADER_STAGE_FRAGMENT_BIT);
	shaderStageCreateInfo[1].pName = "main";

	//==================================================
	// ユニフォームバッファを作成
	//==================================================
	// 頂点シェーダ用ユニフォームバッファ作成
	bufferCreateInfo = {};
	bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
	bufferCreateInfo.size = sizeof(g_uboMatrices);
	bufferCreateInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;

	// ユニフォームバッファ作成
	result = vkCreateBuffer(g_VulkanDevice, &bufferCreateInfo, nullptr, &g_uniformBuffer.buffer);
	checkVulkanError(result, TEXT("ユニフォームバッファ作成失敗"));

	// メモリの配置や情報を獲得
	VkMemoryAllocateInfo allocInfo = {};

	vkGetBufferMemoryRequirements(g_VulkanDevice, g_uniformBuffer.buffer, &memReqs);
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = 0;
	allocInfo.memoryTypeIndex = 0;
	allocInfo.allocationSize = memReqs.size;

	// 現在選択しているGPUから適切なメモリタイプを取得
	getMemoryType(g_currentGPU, memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT, &allocInfo.memoryTypeIndex);

	// ユニフォームバッファをアロケートする
	result = vkAllocateMemory(g_VulkanDevice, &allocInfo, nullptr, &g_uniformBuffer.memory);
	checkVulkanError(result, TEXT("ユニフォームバッファのアロケート失敗"));

	// バッファにメモリをバインドする
	result = vkBindBufferMemory(g_VulkanDevice, g_uniformBuffer.buffer, g_uniformBuffer.memory, 0);
	checkVulkanError(result, TEXT("ユニフォームバッファのアロケート失敗"));

	// ユニフォームディスクリプタに情報を格納
	g_uniformBuffer.descriptor.buffer = g_uniformBuffer.buffer;
	g_uniformBuffer.descriptor.offset = 0;
	g_uniformBuffer.descriptor.range = sizeof(g_uboMatrices);

	//==================================================
	// ディスクリプタプールを作成
	//==================================================
	std::vector<VkDescriptorPoolSize> poolSizes;

	VkDescriptorPoolSize decriptorPoolSize;
	decriptorPoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	decriptorPoolSize.descriptorCount = 1;
	poolSizes.emplace_back(decriptorPoolSize);

	decriptorPoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	decriptorPoolSize.descriptorCount = 1;
	poolSizes.emplace_back(decriptorPoolSize);

	VkDescriptorPoolCreateInfo descriptorPoolInfo = {};
	descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	descriptorPoolInfo.pNext = nullptr;
	descriptorPoolInfo.poolSizeCount = poolSizes.size();
	descriptorPoolInfo.pPoolSizes = poolSizes.data();
	descriptorPoolInfo.maxSets = 1;

	result = vkCreateDescriptorPool(g_VulkanDevice, &descriptorPoolInfo, nullptr, &g_descriptorPool);
	checkVulkanError(result, TEXT("ディスクリプタプールの作成失敗"));

	//==================================================
	// ディスクリプタセットレイアウトを作成
	//==================================================
	std::vector<VkDescriptorSetLayoutBinding> setLayoutBindings;

	// 頂点シェーダにユニフォームバッファを渡す
	VkDescriptorSetLayoutBinding layoutBinding = {};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
	layoutBinding.pImmutableSamplers = nullptr;
	layoutBinding.binding = 0;
	setLayoutBindings.emplace_back(layoutBinding);

	// フラグメントシェーダにサンプラを渡す
	layoutBinding = {};
	layoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	layoutBinding.descriptorCount = 1;
	layoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
	layoutBinding.pImmutableSamplers = nullptr;
	layoutBinding.binding = 1;
	setLayoutBindings.emplace_back(layoutBinding);

	VkDescriptorSetLayoutCreateInfo descriptorLayout = {};
	descriptorLayout.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
	descriptorLayout.pNext = nullptr;
	descriptorLayout.bindingCount = setLayoutBindings.size();
	descriptorLayout.pBindings = setLayoutBindings.data();

	result = vkCreateDescriptorSetLayout(g_VulkanDevice, &descriptorLayout, nullptr, &g_descriptorSetLayout);
	checkVulkanError(result, TEXT("ディスクリプタセットレイアウトの作成失敗"));

	//==================================================
	// ディスクリプタセットを作成
	//==================================================
	VkDescriptorSetAllocateInfo discriptorSetAllocInfo = {};
	discriptorSetAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	discriptorSetAllocInfo.descriptorPool = g_descriptorPool;
	discriptorSetAllocInfo.descriptorSetCount = 1;
	discriptorSetAllocInfo.pSetLayouts = &g_descriptorSetLayout;

	result = vkAllocateDescriptorSets(g_VulkanDevice, &discriptorSetAllocInfo, &g_descriptorSet);
	checkVulkanError(result, TEXT("ディスクリプタセットのアロケート失敗"));

	std::vector<VkWriteDescriptorSet> writeDescriptorSets;

	// 頂点シェーダ用ユニフォームバッファ作成
	VkWriteDescriptorSet writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = g_descriptorSet;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
	writeDescriptorSet.pBufferInfo = &g_uniformBuffer.descriptor;
	writeDescriptorSet.dstBinding = 0;	// 0番目にバインディングする
	writeDescriptorSets.emplace_back(writeDescriptorSet);

	// カラーマップ設定用
	VkDescriptorImageInfo descriptorImageInfo = {};
	descriptorImageInfo.sampler = g_meshTexture.sampler;
	descriptorImageInfo.imageView = g_meshTexture.view;
	descriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

	writeDescriptorSet = {};
	writeDescriptorSet.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
	writeDescriptorSet.dstSet = g_descriptorSet;
	writeDescriptorSet.descriptorCount = 1;
	writeDescriptorSet.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
	writeDescriptorSet.pImageInfo = &descriptorImageInfo;
	writeDescriptorSet.dstBinding = 1;	// 1番目にバインディングする
	writeDescriptorSets.emplace_back(writeDescriptorSet);

	vkUpdateDescriptorSets(g_VulkanDevice, writeDescriptorSets.size(), writeDescriptorSets.data(), 0, nullptr);

	//==================================================
	// パイプラインレイアウトを作成
	//==================================================
	VkPipelineLayoutCreateInfo pPipelineLayoutCreateInfo = {};
	pPipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pPipelineLayoutCreateInfo.pNext = nullptr;
	pPipelineLayoutCreateInfo.pushConstantRangeCount = 0;
	pPipelineLayoutCreateInfo.pPushConstantRanges = nullptr;
	pPipelineLayoutCreateInfo.setLayoutCount = 1;
	pPipelineLayoutCreateInfo.pSetLayouts = &g_descriptorSetLayout;

	result = vkCreatePipelineLayout(g_VulkanDevice, &pPipelineLayoutCreateInfo, nullptr, &g_pipelineLayout);
	checkVulkanError(result, TEXT("パイプラインレイアウト作成に失敗"));

	//==================================================
	// グラフィックスパイプラインを作成
	//==================================================
	// 頂点インプットステート設定
	VkPipelineInputAssemblyStateCreateInfo inputAssemblyState = {};
	inputAssemblyState.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	inputAssemblyState.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

	// ラスタライザステート設定
	VkPipelineRasterizationStateCreateInfo rasterizationState = {};
	rasterizationState.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizationState.polygonMode = VK_POLYGON_MODE_FILL;
	rasterizationState.cullMode = VK_CULL_MODE_NONE;
	rasterizationState.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizationState.depthClampEnable = VK_FALSE;
	rasterizationState.rasterizerDiscardEnable = VK_FALSE;
	rasterizationState.depthBiasEnable = VK_FALSE;

	// ブレンドステート設定
	VkPipelineColorBlendAttachmentState blendAttachmentState[1] = {};
	blendAttachmentState[0].colorWriteMask = 0xf;
	blendAttachmentState[0].blendEnable = VK_FALSE;

	VkPipelineColorBlendStateCreateInfo colorBlendState = {};
	colorBlendState.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	colorBlendState.attachmentCount = 1;
	colorBlendState.pAttachments = blendAttachmentState;

	// ビューポート設定
	VkPipelineViewportStateCreateInfo viewportState = {};
	viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	// 動的ステート設定
	std::vector<VkDynamicState> dynamicStateEnables;
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_VIEWPORT);
	dynamicStateEnables.push_back(VK_DYNAMIC_STATE_SCISSOR);

	VkPipelineDynamicStateCreateInfo dynamicState = {};
	dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamicState.pDynamicStates = dynamicStateEnables.data();
	dynamicState.dynamicStateCount = dynamicStateEnables.size();

	// 深度ステンシルステート設定
	VkPipelineDepthStencilStateCreateInfo depthStencilState = {};
	depthStencilState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depthStencilState.depthTestEnable = VK_TRUE;
	depthStencilState.depthWriteEnable = VK_TRUE;
	depthStencilState.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
	depthStencilState.depthBoundsTestEnable = VK_FALSE;
	depthStencilState.back.failOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.passOp = VK_STENCIL_OP_KEEP;
	depthStencilState.back.compareOp = VK_COMPARE_OP_ALWAYS;
	depthStencilState.stencilTestEnable = VK_FALSE;
	depthStencilState.front = depthStencilState.back;

	// マルチサンプルステート設定
	VkPipelineMultisampleStateCreateInfo multisampleState = {};
	multisampleState.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampleState.pSampleMask = nullptr;
	multisampleState.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

	// グラフィックスパイプラインを作成
	VkGraphicsPipelineCreateInfo pipelineCreateInfo = {};
	pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	pipelineCreateInfo.layout = g_pipelineLayout;
	pipelineCreateInfo.renderPass = g_renderPass;
	pipelineCreateInfo.stageCount = ARRAYSIZE(shaderStageCreateInfo);
	pipelineCreateInfo.pStages = shaderStageCreateInfo;
	pipelineCreateInfo.pVertexInputState = &g_vertexBuffer.vi;
	pipelineCreateInfo.pInputAssemblyState = &inputAssemblyState;
	pipelineCreateInfo.pRasterizationState = &rasterizationState;
	pipelineCreateInfo.pColorBlendState = &colorBlendState;
	pipelineCreateInfo.pMultisampleState = &multisampleState;
	pipelineCreateInfo.pViewportState = &viewportState;
	pipelineCreateInfo.pDepthStencilState = &depthStencilState;
	pipelineCreateInfo.renderPass = g_renderPass;
	pipelineCreateInfo.pDynamicState = &dynamicState;

	result = vkCreateGraphicsPipelines(g_VulkanDevice, g_pipelineCache, 1, &pipelineCreateInfo, nullptr, &g_pipeline);
	checkVulkanError(result, TEXT("グラフィックパイプライン作成失敗"));

	return true;
}

//==============================================================================
// リソース破棄
//==============================================================================
void destroyResource()
{
	if(g_pipeline)
	{
		vkDestroyPipeline(g_VulkanDevice, g_pipeline, nullptr);
		g_pipeline = nullptr;
	}

	if(g_pipelineLayout)
	{
		vkDestroyPipelineLayout(g_VulkanDevice, g_pipelineLayout, nullptr);
		g_pipelineLayout = nullptr;
	}

	if(g_descriptorSetLayout)
	{
		vkDestroyDescriptorSetLayout(g_VulkanDevice, g_descriptorSetLayout, nullptr);
		g_descriptorSetLayout = nullptr;
	}

	if(g_descriptorPool)
	{
		vkDestroyDescriptorPool(g_VulkanDevice, g_descriptorPool, nullptr);
	}

	if(g_uniformBuffer.buffer)
	{
		vkDestroyBuffer(g_VulkanDevice, g_uniformBuffer.buffer, nullptr);
		g_uniformBuffer.buffer = nullptr;
	}

	if(g_uniformBuffer.memory)
	{
		vkFreeMemory(g_VulkanDevice, g_uniformBuffer.memory, nullptr);
		g_uniformBuffer.memory = nullptr;
	}

	if(g_indexBuffer.buf)
	{
		vkDestroyBuffer(g_VulkanDevice, g_indexBuffer.buf, nullptr);
		g_indexBuffer.buf = nullptr;
	}

	if(g_indexBuffer.mem)
	{
		vkFreeMemory(g_VulkanDevice, g_indexBuffer.mem, nullptr);
		g_indexBuffer.buf = nullptr;
	}

	if(g_vertexBuffer.buf)
	{
		vkDestroyBuffer(g_VulkanDevice, g_vertexBuffer.buf, nullptr);
		g_vertexBuffer.buf = nullptr;
	}

	if(g_vertexBuffer.mem)
	{
		vkFreeMemory(g_VulkanDevice, g_vertexBuffer.mem, nullptr);
		g_vertexBuffer.buf = nullptr;
	}
}

//==============================================================================
// 描画
//==============================================================================
void Render()
{
	VkResult result;
	VkCommandBuffer command = g_commandBuffers[g_currentBufferIndex];

	//==================================================
	// コマンド記録開始
	//==================================================
	VkCommandBufferInheritanceInfo commandBufferInheritanceInfo = {};
	commandBufferInheritanceInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO;
	commandBufferInheritanceInfo.pNext = nullptr;
	commandBufferInheritanceInfo.renderPass = nullptr;
	commandBufferInheritanceInfo.subpass = 0;
	commandBufferInheritanceInfo.framebuffer = g_frameBuffers[g_currentBufferIndex];
	commandBufferInheritanceInfo.occlusionQueryEnable = VK_FALSE;
	commandBufferInheritanceInfo.queryFlags = 0;
	commandBufferInheritanceInfo.pipelineStatistics = 0;

	VkCommandBufferBeginInfo cmdBeginInfo = {};
	cmdBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
	cmdBeginInfo.pNext = nullptr;
	cmdBeginInfo.flags = 0;
	cmdBeginInfo.pInheritanceInfo = &commandBufferInheritanceInfo;

	vkBeginCommandBuffer(command, &cmdBeginInfo);

	//==================================================
	// カラーバッファをクリア
	//==================================================
	static float count = 0;
	count += 0.0001f;
	count = fmodf(count, 1.0f);
	VkClearColorValue clearColor;
	clearColor.float32[0] = 0.0f;	// R
	clearColor.float32[1] = count;	// G
	clearColor.float32[2] = 1.0f;	// B
	clearColor.float32[3] = 1.0f;

	VkImageSubresourceRange imageSubresourceRange;
	imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageSubresourceRange.baseMipLevel = 0;
	imageSubresourceRange.levelCount = 1;
	imageSubresourceRange.baseArrayLayer = 0;
	imageSubresourceRange.layerCount = 1;

	vkCmdClearColorImage(
		command,
		g_backBuffersTextures[g_currentBufferIndex].image,
		VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
		&clearColor,
		1,
		&imageSubresourceRange);

	//==================================================
	// 深度バッファをクリア
	//==================================================
	VkClearDepthStencilValue clearDepthStencil;
	clearDepthStencil.depth = 1.0f;
	clearDepthStencil.stencil = 0;

	imageSubresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
	imageSubresourceRange.baseMipLevel = 0;
	imageSubresourceRange.levelCount = 1;
	imageSubresourceRange.baseArrayLayer = 0;
	imageSubresourceRange.layerCount = 1;

	vkCmdClearDepthStencilImage(
		command,
		g_depthBufferTexture.image,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
		&clearDepthStencil,
		1,
		&imageSubresourceRange);

	//==================================================
	// リソースバリアの設定
	//==================================================
	VkImageMemoryBarrier imageMemoryBarrier;
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
	imageMemoryBarrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
	imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
	imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
	imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
	imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
	imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
	imageMemoryBarrier.subresourceRange.levelCount = 1;
	imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
	imageMemoryBarrier.subresourceRange.layerCount = 1;
	imageMemoryBarrier.image = g_backBuffersTextures[g_currentBufferIndex].image;

	vkCmdPipelineBarrier(
		command,
		VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
		VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
		0,
		0,
		nullptr,
		0,
		nullptr,
		1,
		&imageMemoryBarrier);

	//==================================================
	// ビューポート設定
	//==================================================
	VkViewport viewport = {};
	viewport.width = static_cast<float>(SCREEN_WIDTH);
	viewport.height = static_cast<float>(SCREEN_HEIGHT);
	viewport.minDepth = 0.0f;
	viewport.maxDepth = 1.0f;
	vkCmdSetViewport(command, 0, 1, &viewport);

	//==================================================
	// シザー設定
	//==================================================
	VkRect2D scissor = {};
	scissor.extent.width = SCREEN_WIDTH;
	scissor.extent.height = SCREEN_HEIGHT;
	scissor.offset.x = 0;
	scissor.offset.y = 0;
	vkCmdSetScissor(command, 0, 1, &scissor);

	//==================================================
	// ディスクリプタセットをバインド
	//==================================================
	vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipelineLayout, 0, 1, &g_descriptorSet, 0, nullptr);

	//==================================================
	// ユニフォームバッファ更新
	//==================================================
	// 各種行列を更新
	g_uboMatrices.projectionMatrix = glm::perspective(glm::radians(60.0f), static_cast<float>(SCREEN_WIDTH) / static_cast<float>(SCREEN_HEIGHT), 0.1f, 1000.f);

	g_uboMatrices.viewMatrix = glm::translate(glm::mat4(), glm::vec3(0.0f, 0.0f, -2.5));

	static float rotateY = 0.0f;
	rotateY += 0.1f;
	rotateY = fmodf(rotateY, 360.0f);
	g_uboMatrices.worldMatrix = glm::mat4();
	g_uboMatrices.worldMatrix = glm::rotate(g_uboMatrices.worldMatrix, glm::radians(rotateY), glm::vec3(0.0f, 1.0f, 0.0f));

	// ユニフォームバッファに情報をマッピングする
	uint8_t *pData;
	result = vkMapMemory(g_VulkanDevice, g_uniformBuffer.memory, 0, sizeof(g_uboMatrices), 0, reinterpret_cast<void**>(&pData));
	checkVulkanError(result, TEXT("ユニフォームバッファのマッピング失敗"));

	memcpy(pData, &g_uboMatrices, sizeof(g_uboMatrices));
	vkUnmapMemory(g_VulkanDevice, g_uniformBuffer.memory);

	//==================================================
	// メッシュ描画
	//==================================================

	// パイプラインをバインド
	vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, g_pipeline);

	// 頂点バッファをバインド
	VkDeviceSize offsets[1] = { 0 };
	vkCmdBindVertexBuffers(command, 0, 1, &g_vertexBuffer.buf, offsets);

	// インデックスバッファをバインド
	vkCmdBindIndexBuffer(command, g_indexBuffer.buf, 0, VK_INDEX_TYPE_UINT32);

	// 描画コマンド発行
	vkCmdDrawIndexed(command, g_indexBuffer.count, 1, 0, 0, 0);

	//==================================================
	// コマンドの記録を終了
	//==================================================
	vkEndCommandBuffer(command);

	//==================================================
	// コマンドを実行し，表示する
	//==================================================
	VkPipelineStageFlags pipeStageFlags = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;

	VkSubmitInfo submitInfo = {};
	submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
	submitInfo.pNext = nullptr;
	submitInfo.waitSemaphoreCount = 0;
	submitInfo.pWaitSemaphores = nullptr;
	submitInfo.pWaitDstStageMask = &pipeStageFlags;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &command;
	submitInfo.signalSemaphoreCount = 0;
	submitInfo.pSignalSemaphores = nullptr;

	// コマンドを実行
	result = vkQueueSubmit(g_VulkanQueue, 1, &submitInfo, g_VulkanFence);
	checkVulkanError(result, TEXT("グラフィックスキューへのサブミット失敗"));

	// 完了を待機
	result = vkWaitForFences(g_VulkanDevice, 1, &g_VulkanFence, VK_TRUE, TIMEOUT_NANO_SEC);

	// 成功したら表示
	if(result == VK_SUCCESS)
	{
		VkPresentInfoKHR present = {};
		present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		present.pNext = nullptr;
		present.swapchainCount = 1;
		present.pSwapchains = &g_VulkanSwapChain;
		present.pImageIndices = &g_currentBufferIndex;
		present.pWaitSemaphores = nullptr;
		present.waitSemaphoreCount = 0;
		present.pResults = nullptr;

		result = vkQueuePresentKHR(g_VulkanQueue, &present);
		checkVulkanError(result, TEXT("プレゼント失敗"));
	}
	else if(result == VK_TIMEOUT)
	{
		checkVulkanError(VK_TIMEOUT, TEXT("タイムアウトしました"));
	}

	// フェンスをリセット
	result = vkResetFences(g_VulkanDevice, 1, &g_VulkanFence);
	checkVulkanError(result, TEXT("フェンスのリセット失敗"));

	// 次のイメージを取得
	result = vkAcquireNextImageKHR(
		g_VulkanDevice,
		g_VulkanSwapChain,
		TIMEOUT_NANO_SEC,
		g_VulkanSemahoreRenderComplete,
		nullptr,
		&g_currentBufferIndex);
	checkVulkanError(result, TEXT("次の有効なイメージインデックスの獲得に失敗"));
}

//==============================================================================
// ウィンドウプロシージャ
//==============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// メッセージ分岐
	switch(msg)
	{
	case WM_KEYDOWN:	// キーが押された時
		switch(wparam)
		{
		case VK_ESCAPE:
			DestroyWindow(hwnd);
			break;
		}
		break;

	case WM_DESTROY:	// ウィンドウ破棄
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//==============================================================================
// エントリーポイント
//==============================================================================
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	HWND hwnd;
	MSG msg;
	WNDCLASS winc;

	winc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	winc.lpfnWndProc = WndProc;					// ウィンドウプロシージャ
	winc.cbClsExtra = 0;
	winc.cbWndExtra = 0;
	winc.hInstance = hInstance;
	winc.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	winc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	winc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	winc.lpszMenuName = nullptr;
	winc.lpszClassName = CLASS_NAME;

	// ウィンドウクラス登録
	if(RegisterClass(&winc) == false)
	{
		return 1;
	}

	// ウィンドウ作成
	hwnd = CreateWindow(
		CLASS_NAME,
		CLASS_NAME,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE,
		0, 0,
		SCREEN_WIDTH + GetSystemMetrics(SM_CXFRAME) * 2,
		SCREEN_HEIGHT + GetSystemMetrics(SM_CYFRAME) * 2 + GetSystemMetrics(SM_CYCAPTION),
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if(hwnd == nullptr)
	{
		return 1;
	}

	// Vulkan初期化
	if(initVulkan(hInstance, hwnd) == false)
	{
		return 1;
	}

	// リソース作成
	if(createResource() == false)
	{
		return 1;
	}

	// ウィンドウ表示
	ShowWindow(hwnd, nCmdShow);

	// メッセージループ
	do {
		if(PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// メイン
			Render();
		}
	} while(msg.message != WM_QUIT);

	// リソース破棄
	destroyResource();

	// Vulkan破棄
	destroyVulkan();

	// 登録したクラスを解除
	UnregisterClass(CLASS_NAME, hInstance);

	return 0;
}
