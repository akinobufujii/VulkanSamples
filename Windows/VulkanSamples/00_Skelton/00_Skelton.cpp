//==============================================================================
// �C���N���[�h
//==============================================================================
#include <tchar.h>
#include <Windows.h>
#include <assert.h>
#include <vector>
#define _USE_MATH_DEFINES
#include <cmath>

#define VK_USE_PLATFORM_WIN32_KHR	// Win32�p�̊g���@�\��L���ɂ���
#define VK_PROTOTYPES

#include <vulkan/vulkan.h>
#include <vulkan/vk_sdk_platform.h>

//==============================================================================
// �萔
//==============================================================================
static const uint32_t SCREEN_WIDTH = 1280;			// ��ʕ�
static const uint32_t SCREEN_HEIGHT = 720;			// ��ʍ���
static LPCTSTR	CLASS_NAME = TEXT("00_Skelton");	// �E�B���h�E�l�[��
static LPCSTR APPLICATION_NAME = "00_Skelton";		// �A�v���P�[�V������
static const UINT SWAP_CHAIN_COUNT = 2;				// �X���b�v�`�F�[����
static const UINT TIMEOUT_NANO_SEC = 100000000;		// �R�}���h���s�̃^�C���A�E�g����(�i�m�b)

//==============================================================================
// �e���`
//==============================================================================
// Vulkan�̕����f�o�C�X(GPU)���
struct VulkanGPU
{
	VkPhysicalDevice					device;					// GPU�f�o�C�X
	VkPhysicalDeviceProperties			deviceProperties;		// GPU�f�o�C�X�̃v���p�e�B
	VkPhysicalDeviceMemoryProperties	deviceMemoryProperties;	// GPU�f�o�C�X�̃������v���p�e�B
};

// Vulkan�̃e�N�X�`�����
struct VulkanTexture
{
	VkImage			image;	// �C���[�W
	VkImageView		view;	// �r���[
	VkDeviceMemory  memory;	// ������
};

//==============================================================================
// �O���[�o���ϐ�
//==============================================================================
VkInstance									g_VulkanInstance				= nullptr;	// Vulkan�̃C���X�^���X
VkSurfaceKHR								g_VulkanSurface					= nullptr;	// Vulkan�̃T�[�t�F�X���
std::vector<VulkanGPU>						g_GPUs							= {};		// GPU���
VulkanGPU									g_currentGPU;								// ���ݑI�����Ă���GPU���
VkDevice									g_VulkanDevice					= nullptr;	// Vulkan�f�o�C�X
VkQueue										g_VulkanQueue					= nullptr;	// �O���t�B�b�N�X�L���[
VkSemaphore									g_VulkanSemahoreRenderComplete	= nullptr;	// �R�}���h�o�b�t�@���s�p�Z�}�t�H
VkCommandPool								g_VulkanCommandPool				= nullptr;	// �R�}���h�v�[���I�u�W�F�N�g
VkFence										g_VulkanFence					= nullptr;	// �t�F���X�I�u�W�F�N�g
std::vector<VkCommandBuffer>				g_commandBuffers				= {};		// �R�}���h�o�b�t�@�z��(�X���b�v�`�F�[�����Ɉˑ�)
uint32_t									g_currentBufferIndex			= 0;		// ���݂̃o�b�t�@�C���f�b�N�X
VkSwapchainKHR								g_VulkanSwapChain				= nullptr;	// �X���b�v�`�F�[���I�u�W�F�N�g
std::vector<VulkanTexture>					g_backBuffersTextures			= {};		// �o�b�N�o�b�t�@���
VulkanTexture								g_depthBufferTexture;						// �[�x�e�N�X�`��
std::vector<VkFramebuffer>					g_frameBuffers					= {};		// �t���[���o�b�t�@�z��

//==============================================================================
// Vulkan�G���[�`�F�b�N�p�֐�
//==============================================================================
void checkVulkanError(VkResult result, LPTSTR message)
{
	assert(result == VK_SUCCESS && message);
}

//==============================================================================
// �C���[�W���C�A�E�g�ݒ�֐�
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
// Vulkan������
//==============================================================================
bool initVulkan(HINSTANCE hinst, HWND wnd)
{
	VkResult result;

	//==================================================
	// Vulkan�̃C���X�^���X�쐬
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
	checkVulkanError(result, TEXT("Vulkan�C���X�^���X�쐬���s"));

	//==================================================
	// �����f�o�C�X(GPU�f�o�C�X)
	//==================================================
	// �����f�o�C�X�����l��
	uint32_t gpuCount = 0;
	vkEnumeratePhysicalDevices(g_VulkanInstance, &gpuCount, nullptr);
	assert(gpuCount > 0 && TEXT("�����f�o�C�X���̊l�����s"));

	// �����f�o�C�X�����
	std::vector<VkPhysicalDevice> physicalDevices(gpuCount);
	result = vkEnumeratePhysicalDevices(g_VulkanInstance, &gpuCount, physicalDevices.data());
	checkVulkanError(result, TEXT("�����f�o�C�X�̗񋓂Ɏ��s���܂���"));

	// ���ׂĂ�GPU�����i�[
	g_GPUs.resize(gpuCount);
	for(uint32_t i = 0; i < gpuCount; ++i)
	{
		g_GPUs[i].device = physicalDevices[i];

		// �����f�o�C�X�̃v���p�e�B�l��
		vkGetPhysicalDeviceProperties(g_GPUs[i].device, &g_GPUs[i].deviceProperties);

		// �����f�o�C�X�̃������v���p�e�B�l��
		vkGetPhysicalDeviceMemoryProperties(g_GPUs[i].device, &g_GPUs[i].deviceMemoryProperties);
	}

	// �����̃T���v���ł͍ŏ��ɗ񋓂��ꂽGPU�f�o�C�X���g�p����
	g_currentGPU = g_GPUs[0];

	// �O���t�B�b�N�X������T�|�[�g����L���[������
	uint32_t queueCount;
	vkGetPhysicalDeviceQueueFamilyProperties(g_currentGPU.device, &queueCount, nullptr);
	assert(queueCount >= 1 && TEXT("�����f�o�C�X�L���[�̌������s"));

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
	assert(graphicsQueueIndex < queueCount && TEXT("�O���t�B�b�N�X���T�|�[�g����L���[���������܂���ł���"));

	//==================================================
	// Vulkan�f�o�C�X�쐬
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
	checkVulkanError(result, TEXT("Vulkan�f�o�C�X�쐬���s"));

	// �O���t�B�b�N�X�L���[�l��
	vkGetDeviceQueue(g_VulkanDevice, graphicsQueueIndex, 0, &g_VulkanQueue);

	//==================================================
	// �t�F���X�I�u�W�F�N�g�쐬
	//==================================================
	VkFenceCreateInfo fenceCreateInfo = {};
	fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
	fenceCreateInfo.pNext = nullptr;
	fenceCreateInfo.flags = 0;
	result = vkCreateFence(g_VulkanDevice, &fenceCreateInfo, nullptr, &g_VulkanFence);
	checkVulkanError(result, TEXT("�t�F���X�I�u�W�F�N�g�쐬���s"));

	//==================================================
	// �����i�Z�}�t�H�j�I�u�W�F�N�g�쐬
	//==================================================
	VkSemaphoreCreateInfo semaphoreCreateInfo = {};
	semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
	semaphoreCreateInfo.pNext = nullptr;
	semaphoreCreateInfo.flags = 0;

	// �R�}���h�o�b�t�@���s�p�Z�}�t�H�쐬
	result = vkCreateSemaphore(g_VulkanDevice, &semaphoreCreateInfo, nullptr, &g_VulkanSemahoreRenderComplete);
	checkVulkanError(result, TEXT("�R�}���h�o�b�t�@���s�p�Z�}�t�H�쐬���s"));

	//==================================================
	// �R�}���h�v�[���쐻
	//==================================================
	VkCommandPoolCreateInfo cmdPoolInfo = {};
	cmdPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
	cmdPoolInfo.queueFamilyIndex = 0;
	cmdPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
	result = vkCreateCommandPool(g_VulkanDevice, &cmdPoolInfo, nullptr, &g_VulkanCommandPool);
	checkVulkanError(result, TEXT("�R�}���h�v�[���쐬���s"));

	//==================================================
	// �R�}���h�o�b�t�@�쐬
	//==================================================
	// ���������m��.
	g_commandBuffers.resize(SWAP_CHAIN_COUNT);

	VkCommandBufferAllocateInfo commandBufferAllocateInfo = {};
	commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
	commandBufferAllocateInfo.pNext = nullptr;
	commandBufferAllocateInfo.commandPool = g_VulkanCommandPool;
	commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
	commandBufferAllocateInfo.commandBufferCount = SWAP_CHAIN_COUNT;

	result = vkAllocateCommandBuffers(g_VulkanDevice, &commandBufferAllocateInfo, g_commandBuffers.data());
	checkVulkanError(result, TEXT("�R�}���h�o�b�t�@�쐬���s"));

	//==================================================
	// OS(�����Win32)�p�̃T�[�t�F�X���쐬����
	//==================================================
	VkWin32SurfaceCreateInfoKHR surfaceCreateInfo = {};
	surfaceCreateInfo.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	surfaceCreateInfo.hinstance = hinst;
	surfaceCreateInfo.hwnd = wnd;
	result = vkCreateWin32SurfaceKHR(g_VulkanInstance, &surfaceCreateInfo, nullptr, &g_VulkanSurface);
	checkVulkanError(result, TEXT("�T�[�t�F�X�쐬���s"));

	//==================================================
	// �X���b�v�`�F�[�����쐬����
	//==================================================
	VkFormat        imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
	VkColorSpaceKHR imageColorSpace = VK_COLORSPACE_SRGB_NONLINEAR_KHR;

	uint32_t surfaceFormatCount = 0;
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(g_currentGPU.device, g_VulkanSurface, &surfaceFormatCount, nullptr);
	checkVulkanError(result, TEXT("�T�|�[�g���Ă���J���[�t�H�[�}�b�g���̊l�����s"));

	std::vector<VkSurfaceFormatKHR> surfaceFormats;
	surfaceFormats.resize(surfaceFormatCount);
	result = vkGetPhysicalDeviceSurfaceFormatsKHR(g_currentGPU.device, g_VulkanSurface, &surfaceFormatCount, surfaceFormats.data());
	checkVulkanError(result, TEXT("�T�|�[�g���Ă���J���[�t�H�[�}�b�g�̊l�����s"));

	// ��v����J���[�t�H�[�}�b�g����������
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

	// �T�[�t�F�X�̋@�\���l������
	VkSurfaceCapabilitiesKHR surfaceCapabilities;
	VkSurfaceTransformFlagBitsKHR surfaceTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
	result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
		g_currentGPU.device,
		g_VulkanSurface,
		&surfaceCapabilities);
	checkVulkanError(result, TEXT("�T�[�t�F�X�̋@�\�̊l�����s"));

	if((surfaceCapabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) == 0)
	{
		surfaceTransform = surfaceCapabilities.currentTransform;
	}

	// �v���[���g�@�\���l������
	VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
	uint32_t presentModeCount;
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(
		g_currentGPU.device,
		g_VulkanSurface,
		&presentModeCount,
		nullptr);
	checkVulkanError(result, TEXT("�v���[���g�@�\���̊l�����s"));

	std::vector<VkPresentModeKHR> presentModes;
	presentModes.resize(presentModeCount);
	result = vkGetPhysicalDeviceSurfacePresentModesKHR(
		g_currentGPU.device,
		g_VulkanSurface,
		&presentModeCount,
		presentModes.data());
	checkVulkanError(result, TEXT("�v���[���g�@�\�̊l�����s"));

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

	// �X���b�v�`�F�[���쐬
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
	checkVulkanError(result, TEXT("�T�[�t�F�X�쐬���s"));

	//==================================================
	// �C���[�W�̍쐬
	//==================================================
	uint32_t swapChainCount = 0;
	result = vkGetSwapchainImagesKHR(g_VulkanDevice, g_VulkanSwapChain, &swapChainCount, nullptr);
	checkVulkanError(result, TEXT("�X���b�v�`�F�[���C���[�W���̊l�����s"));

	g_backBuffersTextures.resize(swapChainCount);

	std::vector<VkImage> images;
	images.resize(swapChainCount);
	result = vkGetSwapchainImagesKHR(g_VulkanDevice, g_VulkanSwapChain, &swapChainCount, images.data());
	checkVulkanError(result, TEXT("�X���b�v�`�F�[���C���[�W�̊l�����s"));

	for(uint32_t i = 0; i < swapChainCount; ++i)
	{
		g_backBuffersTextures[i].image = images[i];
	}

	images.clear();

	//==================================================
	// �C���[�W�r���[�̐���
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
		checkVulkanError(result, TEXT("�C���[�W�r���[�̍쐬���s"));

		setImageLayout(
			g_VulkanDevice,
			g_commandBuffers[g_currentBufferIndex],
			backBuffer.image,
			VK_IMAGE_ASPECT_COLOR_BIT,
			VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);
	}

	//==================================================
	// �[�x�X�e���V���o�b�t�@�̐���
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
		checkVulkanError(VK_RESULT_MAX_ENUM, TEXT("�T�|�[�g����Ă��Ȃ��t�H�[�}�b�g�ł�"));
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
	checkVulkanError(result, TEXT("�[�x�e�N�X�`���p�C���[�W�r���[�쐬���s"));

	// �������v�����l��
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

	// �������m��
	VkMemoryAllocateInfo allocInfo = {};
	allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
	allocInfo.pNext = nullptr;
	allocInfo.allocationSize = memoryRequirements.size;
	allocInfo.memoryTypeIndex = typeIndex;

	result = vkAllocateMemory(g_VulkanDevice, &allocInfo, nullptr, &g_depthBufferTexture.memory);
	checkVulkanError(result, TEXT("�[�x�e�N�X�`���p�������m�ێ��s"));

	result = vkBindImageMemory(g_VulkanDevice, g_depthBufferTexture.image, g_depthBufferTexture.memory, 0);
	checkVulkanError(result, TEXT("�[�x�e�N�X�`���������Ƀo�C���h���s"));

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
	checkVulkanError(result, TEXT("�[�x�e�N�X�`���C���[�W�r���[�쐬���s"));

	setImageLayout(
		g_VulkanDevice,
		g_commandBuffers[g_currentBufferIndex],
		g_depthBufferTexture.image,
		VK_IMAGE_ASPECT_DEPTH_BIT,
		VK_IMAGE_LAYOUT_UNDEFINED,
		VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

	//==================================================
	// �t���[���o�b�t�@�̐���
	//==================================================
	VkImageView attachments[2];	// 0=�J���[�o�b�t�@�A1=�[�x�o�b�t�@

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
		checkVulkanError(result, TEXT("�t���[���o�b�t�@�쐬���s"));
	}

	//==================================================
	// �R�}���h�����s
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
	checkVulkanError(result, TEXT("�O���t�B�b�N�L���[�̃T�u�~�b�g�Ɏ��s"));

	result = vkQueueWaitIdle(g_VulkanQueue);
	checkVulkanError(result, TEXT("�O���t�B�b�N�L���[�̃A�C�h���҂��Ɏ��s"));

	//==================================================
	// �t���[����p��
	//==================================================
	result = vkAcquireNextImageKHR(
		g_VulkanDevice,
		g_VulkanSwapChain,
		UINT64_MAX,
		g_VulkanSemahoreRenderComplete,
		nullptr,
		&g_currentBufferIndex);
	checkVulkanError(result, TEXT("���̗L���ȃC���[�W�C���f�b�N�X�̊l���Ɏ��s"));

	return true;
}

//==============================================================================
// Vulkan�j��
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
// �`��
//==============================================================================
void Render()
{
	VkResult result;
	VkCommandBuffer command = g_commandBuffers[g_currentBufferIndex];

	//==================================================
	// �R�}���h�L�^�J�n
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
	// �J���[�o�b�t�@���N���A
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
	// �[�x�o�b�t�@���N���A
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
	// ���\�[�X�o���A�̐ݒ�
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
	// �R�}���h�̋L�^���I��
	//==================================================
	vkEndCommandBuffer(command);

	//==================================================
	// �R�}���h�����s���C�\������
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

	// �R�}���h�����s
	result = vkQueueSubmit(g_VulkanQueue, 1, &submitInfo, g_VulkanFence);
	checkVulkanError(result, TEXT("�O���t�B�b�N�X�L���[�ւ̃T�u�~�b�g���s"));

	// ������ҋ@
	result = vkWaitForFences(g_VulkanDevice, 1, &g_VulkanFence, VK_TRUE, TIMEOUT_NANO_SEC);

	// ����������\��
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
		checkVulkanError(result, TEXT("�v���[���g���s"));
	}
	else if(result == VK_TIMEOUT)
	{
		checkVulkanError(VK_TIMEOUT, TEXT("�^�C���A�E�g���܂���"));
	}

	// �t�F���X�����Z�b�g
	result = vkResetFences(g_VulkanDevice, 1, &g_VulkanFence);
	checkVulkanError(result, TEXT("�t�F���X�̃��Z�b�g���s"));

	// ���̃C���[�W���擾
	result = vkAcquireNextImageKHR(
		g_VulkanDevice,
		g_VulkanSwapChain,
		TIMEOUT_NANO_SEC,
		g_VulkanSemahoreRenderComplete,
		nullptr,
		&g_currentBufferIndex);
	checkVulkanError(result, TEXT("���̗L���ȃC���[�W�C���f�b�N�X�̊l���Ɏ��s"));
}

//==============================================================================
// �E�B���h�E�v���V�[�W��
//==============================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	// ���b�Z�[�W����
	switch(msg)
	{
	case WM_KEYDOWN:	// �L�[�������ꂽ��
		switch(wparam)
		{
		case VK_ESCAPE:
			DestroyWindow(hwnd);
			break;
		}
		break;

	case WM_DESTROY:	// �E�B���h�E�j��
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hwnd, msg, wparam, lparam);
}

//==============================================================================
// �G���g���[�|�C���g
//==============================================================================
int APIENTRY _tWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPTSTR lpCmdLine, int nCmdShow)
{
	HWND hwnd;
	MSG msg;
	WNDCLASS winc;

	winc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	winc.lpfnWndProc = WndProc;					// �E�B���h�E�v���V�[�W��
	winc.cbClsExtra = 0;
	winc.cbWndExtra = 0;
	winc.hInstance = hInstance;
	winc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	winc.hCursor = LoadCursor(NULL, IDC_ARROW);
	winc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	winc.lpszMenuName = NULL;
	winc.lpszClassName = CLASS_NAME;

	// �E�B���h�E�N���X�o�^
	if(RegisterClass(&winc) == false)
	{
		return 1;
	}

	// �E�B���h�E�쐬
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

	// Vulkan������
	if(initVulkan(hInstance, hwnd) == false)
	{
		return 1;
	}

	// �E�B���h�E�\��
	ShowWindow(hwnd, nCmdShow);

	// ���b�Z�[�W���[�v
	do {
		if(PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			// ���C��
			Render();
		}
	} while(msg.message != WM_QUIT);

	// Vulkan�j��
	destroyVulkan();

	// �o�^�����N���X������
	UnregisterClass(CLASS_NAME, hInstance);

	return 0;
}
