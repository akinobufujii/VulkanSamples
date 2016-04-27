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

//==============================================================================
// 定数
//==============================================================================
static const uint32_t SCREEN_WIDTH = 1280;			// 画面幅
static const uint32_t SCREEN_HEIGHT = 720;			// 画面高さ
static LPCTSTR	CLASS_NAME = TEXT("00_Skelton");	// ウィンドウネーム
static LPCSTR APPLICATION_NAME = "00_Skelton";		// アプリケーション名
static const UINT SWAP_CHAIN_COUNT = 2;				// スワップチェーン数
static const UINT TIMEOUT_NANO_SEC = 100000000;		// コマンド実行のタイムアウト時間(ナノ秒)

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
	VkImage			image;	// イメージ
	VkImageView		view;	// ビュー
	VkDeviceMemory  memory;	// メモリ
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

//==============================================================================
// Vulkanエラーチェック用関数
//==============================================================================
void checkVulkanError(VkResult result, LPTSTR message)
{
	assert(result == VK_SUCCESS && message);
}

//==============================================================================
// イメージレイアウト設定関数
//==============================================================================
void setImageLayout(
	VkDevice            device,
	VkCommandBuffer     commandBuffer,
	VkImage             image,
	VkImageAspectFlags  aspectFlags,
	VkImageLayout       oldLayout,
	VkImageLayout       newLayout)
{
	assert(device != nullptr);
	assert(commandBuffer != nullptr);

	VkImageMemoryBarrier imageMemoryBarrier = {};
	imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
	imageMemoryBarrier.pNext = nullptr;
	imageMemoryBarrier.srcAccessMask = 0;
	imageMemoryBarrier.dstAccessMask = 0;
	imageMemoryBarrier.oldLayout = oldLayout;
	imageMemoryBarrier.newLayout = newLayout;
	imageMemoryBarrier.image = image;
	imageMemoryBarrier.subresourceRange = { aspectFlags, 0, 1, 0, 1 };

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

	VkInstanceCreateInfo instanceCreateInfo = {};
	instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instanceCreateInfo.pNext = nullptr;
	instanceCreateInfo.pApplicationInfo = &applicationInfo;

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

	std::vector<LPCSTR> enabledExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

	VkDeviceCreateInfo deviceCreateInfo = {};
	deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	deviceCreateInfo.pNext = nullptr;
	deviceCreateInfo.queueCreateInfoCount = 1;
	deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
	deviceCreateInfo.pEnabledFeatures = nullptr;

	if(enabledExtensions.size() > 0)
	{
		deviceCreateInfo.enabledExtensionCount = enabledExtensions.size();
		deviceCreateInfo.ppEnabledExtensionNames = enabledExtensions.data();
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
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
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
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	//==================================================
	// フレームバッファの生成
	//==================================================
	VkImageView attachments[2];	// 0=カラーバッファ、1=深度バッファ

	VkFramebufferCreateInfo frameBufferCreateInfo = {};
	frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
	frameBufferCreateInfo.pNext = nullptr;
	frameBufferCreateInfo.flags = 0;
	frameBufferCreateInfo.renderPass = VK_NULL_HANDLE;
	frameBufferCreateInfo.attachmentCount = 2;
	frameBufferCreateInfo.pAttachments = attachments;
	frameBufferCreateInfo.width = SCREEN_WIDTH;
	frameBufferCreateInfo.height = SCREEN_HEIGHT;
	frameBufferCreateInfo.layers = 1;

	g_frameBuffers.resize(SWAP_CHAIN_COUNT);
	for(uint32_t i = 0; i < SWAP_CHAIN_COUNT; ++i)
	{
		attachments[0] = g_backBuffersTextures[i].view;
		attachments[1] = g_depthBufferTexture.view;
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

	return true;
}

//==============================================================================
// Vulkan破棄
//==============================================================================
void destroyVulkan()
{
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
	submitInfo.pCommandBuffers = &g_commandBuffers[g_currentBufferIndex];
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
	winc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	winc.hCursor = LoadCursor(NULL, IDC_ARROW);
	winc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	winc.lpszMenuName = NULL;
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
		0, 0, SCREEN_WIDTH, SCREEN_HEIGHT,
		NULL,
		NULL,
		hInstance,
		NULL);

	if(hwnd == NULL)
	{
		return 1;
	}

	// Vulkan初期化
	if(initVulkan(hInstance, hwnd) == false)
	{
		return 1;
	}

	// ウィンドウ表示
	ShowWindow(hwnd, nCmdShow);

	// メッセージループ
	do {
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
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

	// Vulkan破棄
	destroyVulkan();

	// 登録したクラスを解除
	UnregisterClass(CLASS_NAME, hInstance);

	return 0;
}
