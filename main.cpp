#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <iostream>
#include <vector>

#define VULKAN_ERROR_CHECK(result, message) \
	if (result != VK_SUCCESS) { \
		assert(0 && message); \
		system("pause"); \
		exit(-1); \
	}

#define VULKAN_ERROR(message) \
	assert(0 && message); \
	system("pause"); \
	exit(-1);

#define VULKAN_ASSERT(test) \
	assert(test);
	
#define VULKAN_USE_VALIDATION_LAYERS

#define MAX_REQUIRED_EXTENSIONS        32
#define MAX_LAYERS                      8
#define MAX_DEVICES                     4
#define MAX_QUEUE_FAMILIES              8
#define INVALID_QUEUE_FAMILY_INDEX   9999
#define MAX_SWAPCHAIN_IMAGES            8
#define MAX_COMMAND_BUFFERS MAX_SWAPCHAIN_IMAGES

#define MAX_FORMATS                     4
#define MAX_PRESENT_MODES               4

#define Kilobytes(count)             (1024*(count))
#define Megabytes(count)  (count*1024*Kilobytes(1))
#define MAX_FILE_SIZE                 Megabytes(1) 

#define MAX(a, b) (((a)>(b))?(a):(b))
#define MIN(a, b) (((a)<(b))?(a):(b))

#define ArrayCount(a) sizeof(a)/sizeof(a[0])

typedef uint32_t u32;

struct FileContents
{
	char data[MAX_FILE_SIZE];
	u32 fileSize = 0;
};

struct Memory
{
	FileContents mainShaderVertContents;
	FileContents mainShaderFragContents;
};

void ReadFile(const char *filename, FileContents *contents)
{
	FILE *file = fopen(filename, "rb");
	if (file != nullptr)
	{
		fseek(file, 0, SEEK_END);
		contents->fileSize = ftell(file);

		rewind(file);
		u32 readBytes = fread(contents->data, sizeof(char), contents->fileSize, file);
		fclose(file);

		VULKAN_ASSERT(readBytes == contents->fileSize);
	}
}

struct QueueFamilyIndices
{
	u32 graphicsFamily = INVALID_QUEUE_FAMILY_INDEX;
	u32 presentFamily = INVALID_QUEUE_FAMILY_INDEX;

	bool isComplete() const {
		return
			graphicsFamily != INVALID_QUEUE_FAMILY_INDEX &&
			presentFamily != INVALID_QUEUE_FAMILY_INDEX;
	}
};


VkResult CreateDebugUtilsMessengerEXT(
		VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
		const VkAllocationCallbacks* pAllocator,
		VkDebugUtilsMessengerEXT* pDebugMessenger)
{
	auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
	if (func != nullptr) {
		return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void DestroyDebugUtilsMessengerEXT(
		VkInstance instance,
		VkDebugUtilsMessengerEXT debugMessenger,
		const VkAllocationCallbacks* pAllocator)
{
	auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func != nullptr) {
		func(instance, debugMessenger, pAllocator);
	}
}


class HelloTriangleApplication
{
	public:

		void run()
		{
			initMemory();
			initWindow();
			initVulkan();
			mainLoop();
			cleanup();
		}

	private:

		Memory *memory = nullptr;

		const u32 WIDTH = 800;
		const u32 HEIGHT = 600;
		GLFWwindow *window = nullptr;

		VkInstance instance;
		VkDebugUtilsMessengerEXT debugMessenger;
		VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
		VkDevice device;
		VkSurfaceKHR surface;
		VkQueue graphicsQueue;
		VkQueue presentQueue;
		VkSwapchainKHR swapChain;
		VkFormat swapChainImageFormat;
		VkExtent2D swapChainExtent;
		VkRenderPass renderPass;
		VkPipelineLayout pipelineLayout;
		VkPipeline graphicsPipeline;
		VkCommandPool commandPool;

		VkSemaphore imageAvailableSemaphore;
		VkSemaphore renderFinishedSemaphore;

		VkCommandBuffer commandBuffers[MAX_SWAPCHAIN_IMAGES];

		VkImage swapChainImages[MAX_SWAPCHAIN_IMAGES];
		u32 swapChainImagesCount = 0;

		VkImageView swapChainImageViews[MAX_SWAPCHAIN_IMAGES];

		VkFramebuffer swapChainFramebuffers[MAX_SWAPCHAIN_IMAGES];

		const char *requiredExtensions[MAX_REQUIRED_EXTENSIONS];
		u32 requiredExtensionsCount = 0;

		const char *requiredDeviceExtensions[MAX_REQUIRED_EXTENSIONS];
		u32 requiredDeviceExtensionsCount = 0;

		const char *validationLayers[MAX_LAYERS];
		u32 validationLayersCount = 0;

		void initMemory()
		{
			memory = new Memory;
			VULKAN_ASSERT(memory != nullptr);
		}

		void initWindow()
		{
			validationLayers[validationLayersCount++] = "VK_LAYER_KHRONOS_validation";

			glfwInit();
			glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
			glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
			window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
		}

		void initVulkan()
		{
			createInstance();
			setupDebugMessenger();
			createSurface();
			pickPhysicalDevice();
			createLogicalDevice();
			createSwapChain();
			createImageViews();
			createRenderPass();
			createGraphicsPipeline();
			createFramebuffers();
			createCommandPool();
			createCommandBuffers();
			createSemaphores();
		}

		void mainLoop()
		{
			while (!glfwWindowShouldClose(window))
			{
				glfwPollEvents();
				drawFrame();
			}

			vkDeviceWaitIdle(device);
		}

		void cleanup()
		{
			vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
			vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
			vkDestroyCommandPool(device, commandPool, nullptr);
			for (u32 i = 0; i < swapChainImagesCount; ++i)
			{
				vkDestroyFramebuffer(device, swapChainFramebuffers[i], nullptr);
			}
			vkDestroyPipeline(device, graphicsPipeline, nullptr);
			vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
			vkDestroyRenderPass(device, renderPass, nullptr);
			for (u32 i = 0; i < swapChainImagesCount; ++i) {
				vkDestroyImageView(device, swapChainImageViews[i], nullptr);
			}
			vkDestroySwapchainKHR(device, swapChain, nullptr);
			vkDestroyDevice(device, nullptr);
#ifdef VULKAN_USE_VALIDATION_LAYERS
			DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
#endif
			vkDestroySurfaceKHR(instance, surface, nullptr);
			vkDestroyInstance(instance, nullptr);
			glfwDestroyWindow(window);
			glfwTerminate();

			delete memory;
		}

		void createInstance()
		{
			VkApplicationInfo appInfo = {};
			appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
			appInfo.pApplicationName = "Hello triangle";
			appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.pEngineName = "No Engine";
			appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
			appInfo.apiVersion = VK_API_VERSION_1_0;

			VkInstanceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
			createInfo.pApplicationInfo = &appInfo;

			getRequiredExtensions();
			createInfo.enabledExtensionCount = requiredExtensionsCount;
			createInfo.ppEnabledExtensionNames = requiredExtensions;

#ifdef VULKAN_USE_VALIDATION_LAYERS
			// NOTE(jesus): It's outside the if to ensure its existance at least until
			// the function vkCreateInstance(...) is executed
			VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

			if (checkValidationLayerSupport())
			{
				createInfo.enabledLayerCount = validationLayersCount;
				createInfo.ppEnabledLayerNames = validationLayers;

				populateDebugMessengerCreateInfo(&debugCreateInfo);
				createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
			}
			else
			{
				VULKAN_ERROR("Not all the requested validation layers are available!");
			}
#else
			createInfo.enabledLayerCount = 0;
			createInfo.pNext = nullptr;
#endif

			VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
			VULKAN_ERROR_CHECK(result, "vkCreateInstance failed");
		}

		void setupDebugMessenger()
		{
#ifdef VULKAN_USE_VALIDATION_LAYERS
			VkDebugUtilsMessengerCreateInfoEXT createInfo;
			populateDebugMessengerCreateInfo(&createInfo);

			if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
				VULKAN_ERROR("Failed to set up debug messenger!");
			}
#endif
		}

		void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT *createInfo)
		{
			*createInfo = {};
			createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
			createInfo->messageSeverity =
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
				//VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
			createInfo->messageType =
				VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
				VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
			createInfo->pfnUserCallback = debugCallback;
		}

		static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
				VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
				VkDebugUtilsMessageTypeFlagsEXT messageType,
				const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
				void* pUserData)
		{
			printf("validation layer: %s\n", pCallbackData->pMessage);
			return VK_FALSE;
		}

		void getRequiredExtensions()
		{
			const char ** glfwExtensions;
			u32 glfwExtensionCount = 0;
			glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

			VULKAN_ASSERT(glfwExtensionCount <= MAX_REQUIRED_EXTENSIONS);
			requiredExtensionsCount = glfwExtensionCount;

			for (u32 i = 0; i < requiredExtensionsCount; ++i)
			{
				requiredExtensions[i] = glfwExtensions[i];
			}

#ifdef VULKAN_USE_VALIDATION_LAYERS
			VULKAN_ASSERT(requiredExtensionsCount + 1 <= MAX_REQUIRED_EXTENSIONS);
			requiredExtensions[requiredExtensionsCount++] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
#endif

			checkRequiredExtensionsSupport();
		}

		void checkRequiredExtensionsSupport()
		{
			u32 availableExtensionsCount = 0;
			VkExtensionProperties availableExtensions[32];
			vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, nullptr);
			vkEnumerateInstanceExtensionProperties(nullptr, &availableExtensionsCount, availableExtensions);

			printf("Available extensions:\n");
			for (u32 i = 0; i < availableExtensionsCount; ++i)
			{
				printf("\t%s\n", availableExtensions[i].extensionName);
			}

			printf("Required extensions:\n");
			for (u32 i = 0; i < requiredExtensionsCount; ++i)
			{
				printf("\t%s\n", requiredExtensions[i]);

				bool found = false;
				for (u32 j = 0; j < availableExtensionsCount; ++j)
				{
					if (strcmp(requiredExtensions[i], availableExtensions[j].extensionName) == 0)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					VULKAN_ERROR("Vulkan not providing all the required extensions.");
				}
			}
		}

		bool checkValidationLayerSupport()
		{
			u32 availableLayersCount;
			VkLayerProperties availableLayers[32];
			vkEnumerateInstanceLayerProperties(&availableLayersCount, nullptr);
			vkEnumerateInstanceLayerProperties(&availableLayersCount, availableLayers);

			for (u32 i = 0; i < validationLayersCount; ++i)
			{
				bool found = false;

				for (u32 j = 0; j < availableLayersCount; ++j)
				{
					if (strcmp(validationLayers[i], availableLayers[j].layerName) == 0)
					{
						found = true;
						break;
					}
				}

				if (!found)
				{
					return false;
				}
			}

			return true;
		}


		// Physical devices

		void pickPhysicalDevice()
		{
			// TODO(jesus): Is this ok here?
			requiredDeviceExtensionsCount = 0;
			requiredDeviceExtensions[requiredDeviceExtensionsCount++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;

			u32 deviceCount = 0;
			vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
			VULKAN_ASSERT(deviceCount > 0);
			VULKAN_ASSERT(deviceCount <= MAX_DEVICES);

			u32 physicalDevicesScore[MAX_DEVICES];
			VkPhysicalDevice physicalDevices[MAX_DEVICES];
			vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices);
			for (u32 i = 0; i < deviceCount; ++i)
			{
				u32 score = computePhysicalDeviceScore(physicalDevices[i]);
				physicalDevicesScore[i] = score;
			}

			u32 maxScore = 0;
			for (u32 i = 0; i < deviceCount; ++i)
			{
				if (physicalDevicesScore[i] > maxScore)
				{
					maxScore = physicalDevicesScore[i];
					physicalDevice = physicalDevices[i];
				}	
			}

			VULKAN_ASSERT(physicalDevice != VK_NULL_HANDLE);
		}


		u32 computePhysicalDeviceScore(VkPhysicalDevice device)
		{
			int score = 1;

			VkPhysicalDeviceProperties properties;
			vkGetPhysicalDeviceProperties(device, &properties);

			VkPhysicalDeviceFeatures features;
			vkGetPhysicalDeviceFeatures(device, &features);

			if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
				score += 1000;
			}

			QueueFamilyIndices queueFamilyIndices = findQueueFamilies(device);
			if (!queueFamilyIndices.isComplete()) {
				score = 0;
			}

			bool extensionsSupported = checkDeviceExtensionSupport(device);
			if (!extensionsSupported) {
				score = 0;
			}
			else
			{
				SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
				bool swapChainAdequate = swapChainSupport.formatCount > 0 && swapChainSupport.presentModeCount > 0;
				if (!swapChainAdequate)
				{
					score = 0;
				}
			}

			// If the application requires geometry shaders...
			//if (features.geometryShader == false) {
			//	score = 0;
			//}

			return score;
		}

		bool checkDeviceExtensionSupport(VkPhysicalDevice device)
		{
			u32 availableExtensionsCount;
			vkEnumerateDeviceExtensionProperties(device, nullptr, &availableExtensionsCount, nullptr);
			VULKAN_ASSERT(availableExtensionsCount <= 128);

			VkExtensionProperties availableExtensions[128];
			vkEnumerateDeviceExtensionProperties(device, nullptr, &availableExtensionsCount, availableExtensions);

			for (u32 i = 0; i < requiredDeviceExtensionsCount; ++i)
			{
				bool found = true;
				for (u32 j = 0; j < availableExtensionsCount; ++j)
				{
					if (strcmp(requiredDeviceExtensions[i], availableExtensions[j].extensionName) == 0)
					{
						found = true;
						break;
					}
				}
				if (!found)
				{
					return false;
				}
			}

			return true;
		}


		// Queue families

		QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device)
		{
			QueueFamilyIndices indices;

			u32 queueFamilyCount = 0;
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
			VULKAN_ASSERT(queueFamilyCount <= MAX_QUEUE_FAMILIES);

			VkQueueFamilyProperties queueFamilies[MAX_QUEUE_FAMILIES];
			vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies);

			for (u32 i = 0; i < queueFamilyCount; ++i)
			{
				if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)
				{
					indices.graphicsFamily = i;
				}

				VkBool32 presentSupport = false;
				vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
				if (presentSupport) {
					indices.presentFamily = i;
				}


				if (indices.isComplete()) {
					break;
				}
			}

			return indices;
		}


		// Logical devices

		void createLogicalDevice()
		{
			float queuePriority = 1.0f;
			QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

			u32 queueFamilyCount = 2;
			u32 queueFamilies[] = { indices.graphicsFamily, indices.presentFamily };
			u32 uniqueQueueFamilies[MAX_QUEUE_FAMILIES];
			u32 uniqueQueueFamilyCount = 0;
			for (u32 i = 0; i < queueFamilyCount; ++i)
			{
				u32 family = queueFamilies[i];
				bool found = false;
				for (u32 j = 0; j < uniqueQueueFamilyCount; ++j) {
					if (uniqueQueueFamilies[j] == family) {
						found = true;
						break;
					}
				}
				if (!found) {
					uniqueQueueFamilies[uniqueQueueFamilyCount++] = family;
				}
			}
			VkDeviceQueueCreateInfo uniqueQueueCreateInfos[MAX_QUEUE_FAMILIES] = {};
			for (u32 i = 0; i < uniqueQueueFamilyCount; ++i)
			{
				uniqueQueueCreateInfos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
				uniqueQueueCreateInfos[i].queueFamilyIndex = uniqueQueueFamilies[i];
				uniqueQueueCreateInfos[i].queueCount = 1;
				uniqueQueueCreateInfos[i].pQueuePriorities = &queuePriority;
			}

			VkPhysicalDeviceFeatures deviceFeatures = {};

			VkDeviceCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
			createInfo.queueCreateInfoCount = uniqueQueueFamilyCount;
			createInfo.pQueueCreateInfos = uniqueQueueCreateInfos;
			createInfo.pEnabledFeatures = &deviceFeatures;
			createInfo.enabledExtensionCount = requiredDeviceExtensionsCount;
			createInfo.ppEnabledExtensionNames = requiredDeviceExtensions;

#ifdef VULKAN_USE_VALIDATION_LAYERS
			createInfo.enabledLayerCount = validationLayersCount;
			createInfo.ppEnabledLayerNames = validationLayers;
#else
			createInfo.enabledLayerCount = 0;
#endif

			VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &device);
			VULKAN_ERROR_CHECK(result, "vkCreateDevice");

			vkGetDeviceQueue(device, indices.graphicsFamily, 0, &graphicsQueue);
			vkGetDeviceQueue(device, indices.presentFamily, 0, &presentQueue);
		}


		// Window surface

		void createSurface()
		{
			VkResult result = glfwCreateWindowSurface(instance, window, nullptr, &surface);
			VULKAN_ERROR_CHECK(result, "glfwCreateWindowSurface");
		}


		// Swapchain

		struct SwapChainSupportDetails{
			VkSurfaceCapabilitiesKHR capabilities;
			VkSurfaceFormatKHR formats[MAX_FORMATS];
			VkPresentModeKHR presentModes[MAX_PRESENT_MODES];
			u32 formatCount = 0;
			u32 presentModeCount = 0;
		};

		void createSwapChain()
		{
			SwapChainSupportDetails swapChainSupport = querySwapChainSupport(physicalDevice);

			VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats, swapChainSupport.formatCount);
			VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes, swapChainSupport.presentModeCount);
			VkExtent2D extent = chooseSwapExtent(&swapChainSupport.capabilities);
			
			u32 imageCount = swapChainSupport.capabilities.minImageCount + 1;
			if (imageCount > swapChainSupport.capabilities.maxImageCount &&
					         swapChainSupport.capabilities.maxImageCount != 0) { // 0 means no maximum
				imageCount = swapChainSupport.capabilities.maxImageCount;
			}

			VkSwapchainCreateInfoKHR createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
			createInfo.surface = surface;
			createInfo.minImageCount = imageCount;
			createInfo.imageFormat = surfaceFormat.format;
			createInfo.imageColorSpace = surfaceFormat.colorSpace;
			createInfo.imageExtent = extent;
			createInfo.imageArrayLayers = 1;
			createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

			QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);
			u32 familyIndices[] = {queueFamilyIndices.graphicsFamily, queueFamilyIndices.presentFamily};

			if (queueFamilyIndices.graphicsFamily != queueFamilyIndices.presentFamily)
			{
				createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
				createInfo.queueFamilyIndexCount = 2;
				createInfo.pQueueFamilyIndices = familyIndices;
			}
			else
			{
				createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
				createInfo.queueFamilyIndexCount = 0;
				createInfo.pQueueFamilyIndices = nullptr;
			}

			createInfo.preTransform = swapChainSupport.capabilities.currentTransform; // no transforms
			createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // ignore alpha
			createInfo.presentMode = presentMode;
			createInfo.clipped = VK_TRUE; // clip hidden pixels (e.g. by another window)
			createInfo.oldSwapchain = VK_NULL_HANDLE;

			VkResult result = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain);
			VULKAN_ERROR_CHECK(result, "vkCreateSwapchainKHR");

			vkGetSwapchainImagesKHR(device, swapChain, &swapChainImagesCount, nullptr);
			VULKAN_ASSERT(swapChainImagesCount <= MAX_SWAPCHAIN_IMAGES);
			vkGetSwapchainImagesKHR(device, swapChain, &swapChainImagesCount, swapChainImages);

			swapChainImageFormat = surfaceFormat.format;
			swapChainExtent = extent;
		}

		SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device)
		{
			SwapChainSupportDetails details;

			vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

			u32 formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);
			VULKAN_ASSERT(formatCount <= MAX_FORMATS);

			if (formatCount != 0)
			{
				details.formatCount = formatCount;
				vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats);
			}

			u32 presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);
			VULKAN_ASSERT(presentModeCount <= MAX_PRESENT_MODES);

			if (presentModeCount != 0)
			{
				details.presentModeCount = presentModeCount;
				vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes);
			}

			return details;
		}

		VkSurfaceFormatKHR chooseSwapSurfaceFormat(VkSurfaceFormatKHR *formats, u32 formatsCount)
		{
			for (u32 i = 0; i < formatsCount; ++i)
			{
				if (formats[i].format == VK_FORMAT_B8G8R8A8_UNORM && formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
				{
					return formats[i];
				}
			}

			return formats[0];
		}

		VkPresentModeKHR chooseSwapPresentMode(VkPresentModeKHR *presentModes, u32 presentModesCount)
		{
			for (u32 i = 0; i < presentModesCount; ++i)
			{
				if (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
				{
					return presentModes[i];
				}
			}

			return VK_PRESENT_MODE_FIFO_KHR;
		}

		VkExtent2D chooseSwapExtent(VkSurfaceCapabilitiesKHR *capabilities)
		{
			if (capabilities->currentExtent.width != UINT32_MAX) {
				return capabilities->currentExtent;
			} else {
				VkExtent2D actualExtent = {WIDTH, HEIGHT};
				actualExtent.width = MAX(capabilities->minImageExtent.width, MIN(capabilities->maxImageExtent.width, actualExtent.width));
				actualExtent.height = MAX(capabilities->minImageExtent.height, MIN(capabilities->maxImageExtent.height, actualExtent.height));
				return actualExtent;
			}
		}

		void createImageViews()
		{
			for (u32 i = 0; i < swapChainImagesCount; ++i)
			{
				VkImageViewCreateInfo createInfo = {};
				createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
				createInfo.image = swapChainImages[i];
				createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
				createInfo.format = swapChainImageFormat;
				createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
				createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
				createInfo.subresourceRange.baseMipLevel = 0;
				createInfo.subresourceRange.levelCount = 1;
				createInfo.subresourceRange.baseArrayLayer = 0;
				createInfo.subresourceRange.layerCount = 1;

				VkResult result = vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]);
				VULKAN_ERROR_CHECK(result, "vkCreateImageView");
			}
		}

		void createRenderPass()
		{
			// Attachments

			VkAttachmentDescription colorAttachment = {};
			colorAttachment.format = swapChainImageFormat;
			colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
			colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
			colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
			colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
			colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;     // NOTE(jesus): don't really understand
			colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // NOTE(jesus): don't really understand

			// Subpasses (bound to one or several attachments)

			VkAttachmentReference colorAttachmentRef = {};
			colorAttachmentRef.attachment = 0;
			colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

			VkSubpassDescription subpass = {};
			subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; // There are also compute subpasses
			subpass.colorAttachmentCount = 1;
			subpass.pColorAttachments = &colorAttachmentRef;

			VkSubpassDependency dependency = {};
			dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
			dependency.dstSubpass = 0;
			dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.srcAccessMask = 0;
			dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
			dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

			// Render pass

			VkRenderPassCreateInfo renderPassInfo = {};
			renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
			renderPassInfo.attachmentCount = 1;
			renderPassInfo.pAttachments = &colorAttachment;
			renderPassInfo.subpassCount = 1;
			renderPassInfo.pSubpasses = &subpass;
			renderPassInfo.dependencyCount = 1;
			renderPassInfo.pDependencies = &dependency;

			VkResult result = vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass);
			VULKAN_ERROR_CHECK(result, "vkCreateRenderPass");
		}

		void createGraphicsPipeline()
		{
			// Shader stage

			ReadFile("main_shader_vert.spv", &memory->mainShaderVertContents);
			ReadFile("main_shader_frag.spv", &memory->mainShaderFragContents);

			VkShaderModule vertShaderModule = createShaderModule(memory->mainShaderVertContents.data,
					                                             memory->mainShaderVertContents.fileSize);
			VkShaderModule fragShaderModule = createShaderModule(memory->mainShaderFragContents.data,
					                                             memory->mainShaderFragContents.fileSize);

			VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
			vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
			vertShaderStageInfo.module = vertShaderModule;
			vertShaderStageInfo.pName = "main";

			VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
			fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
			fragShaderStageInfo.module = fragShaderModule;
			fragShaderStageInfo.pName = "main";

			VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

			// VertexInputState

			VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
			vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
			vertexInputInfo.vertexBindingDescriptionCount = 0;
			vertexInputInfo.pVertexBindingDescriptions = nullptr; // Optional
			vertexInputInfo.vertexAttributeDescriptionCount = 0;
			vertexInputInfo.pVertexAttributeDescriptions = nullptr; // Optional

			// InputAssemblyState

			VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo = {};
			inputAssemblyInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
			inputAssemblyInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
			inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;

			// Viewports and scrissors

			VkViewport viewport = {};
			viewport.x = 0.0f;
			viewport.y = 0.0f;
			viewport.width = (float)swapChainExtent.width;
			viewport.height = (float)swapChainExtent.height;
			viewport.minDepth = 0.0f;
			viewport.maxDepth = 1.0f;

			VkRect2D scissor = {};
			scissor.offset = {0, 0};
			scissor.extent = swapChainExtent;

			VkPipelineViewportStateCreateInfo viewportStateInfo = {};
			viewportStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
			viewportStateInfo.viewportCount = 1;
			viewportStateInfo.pViewports = &viewport;
			viewportStateInfo.scissorCount = 1;
			viewportStateInfo.pScissors = &scissor;

			// Rasterizer

			VkPipelineRasterizationStateCreateInfo rasterizerInfo = {};
			rasterizerInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
			rasterizerInfo.depthClampEnable = VK_FALSE;
			rasterizerInfo.rasterizerDiscardEnable = VK_FALSE; // disable any output (GPU feature if == TRUE)
			rasterizerInfo.polygonMode = VK_POLYGON_MODE_FILL;  // FILL, LINE, POINT (GPU feature if != FILL)
			rasterizerInfo.lineWidth = 1.0f;                    // GPU feature if > 1.0
			rasterizerInfo.cullMode = VK_CULL_MODE_BACK_BIT;
			rasterizerInfo.frontFace = VK_FRONT_FACE_CLOCKWISE; // Maybe Counterclockwise better?
			rasterizerInfo.depthBiasEnable = VK_FALSE;
			rasterizerInfo.depthBiasConstantFactor = 0.0f; // Optional
			rasterizerInfo.depthBiasClamp = 0.0f;           // Optional
			rasterizerInfo.depthBiasSlopeFactor = 0.0f;    // Optional

			// Multisampling: disabled (enable requires a GPU feature)

			VkPipelineMultisampleStateCreateInfo multisamplingInfo = {};
			multisamplingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
			multisamplingInfo.sampleShadingEnable = VK_FALSE;
			multisamplingInfo.rasterizationSamples= VK_SAMPLE_COUNT_1_BIT;
			multisamplingInfo.minSampleShading = 1.0f;          // Optional
			multisamplingInfo.pSampleMask = nullptr;            // Optional
			multisamplingInfo.alphaToCoverageEnable = VK_FALSE; // Optional
			multisamplingInfo.alphaToOneEnable = VK_FALSE;      // Optional

			// Depth and stencil (disabled by now, pass nullptr)

			// Color blending

			VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
			colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
												  VK_COLOR_COMPONENT_G_BIT |
												  VK_COLOR_COMPONENT_B_BIT |
												  VK_COLOR_COMPONENT_A_BIT;
			colorBlendAttachment.blendEnable = VK_FALSE;
			colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
			colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;             // Optional
			colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;  // Optional
			colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
			colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;             // Optional

			VkPipelineColorBlendStateCreateInfo colorBlendingInfo = {};
			colorBlendingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
			colorBlendingInfo.logicOpEnable = VK_FALSE;
			colorBlendingInfo.logicOp = VK_LOGIC_OP_COPY;
			colorBlendingInfo.attachmentCount = 1;
			colorBlendingInfo.pAttachments = &colorBlendAttachment;
			colorBlendingInfo.blendConstants[0] = 0.0f; // Optional
			colorBlendingInfo.blendConstants[1] = 0.0f; // Optional
			colorBlendingInfo.blendConstants[2] = 0.0f; // Optional
			colorBlendingInfo.blendConstants[3] = 0.0f; // Optional

			// Dynamic state (parts that can change without recreating the pipeline)

			VkDynamicState dynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };

			VkPipelineDynamicStateCreateInfo dynamicStateInfo = {};
			dynamicStateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
			dynamicStateInfo.dynamicStateCount = 2;
			dynamicStateInfo.pDynamicStates = dynamicStates;

			// Pipeline layout

			VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
			pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
			pipelineLayoutInfo.setLayoutCount = 0;            // Optional
			pipelineLayoutInfo.pSetLayouts = nullptr;         // Optional
			pipelineLayoutInfo.pushConstantRangeCount = 0;     // Optional
			pipelineLayoutInfo.pPushConstantRanges = nullptr; // Optional

			VkResult result = vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &pipelineLayout);
			VULKAN_ERROR_CHECK(result, "vkCreatePipelineLayout");

			// Graphics pipeline

			VkGraphicsPipelineCreateInfo pipelineInfo = {};
			pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
			pipelineInfo.stageCount = 2;
			pipelineInfo.pStages = shaderStages;
			pipelineInfo.pVertexInputState = &vertexInputInfo;
			pipelineInfo.pInputAssemblyState = &inputAssemblyInfo;
			pipelineInfo.pViewportState = &viewportStateInfo;
			pipelineInfo.pRasterizationState = &rasterizerInfo;
			pipelineInfo.pMultisampleState = &multisamplingInfo;
			pipelineInfo.pDepthStencilState = nullptr;
			pipelineInfo.pColorBlendState = &colorBlendingInfo;
			pipelineInfo.pDynamicState = nullptr;
			pipelineInfo.layout = pipelineLayout;
			pipelineInfo.renderPass = renderPass;
			pipelineInfo.subpass = 0;

			// In order to create pipeline derivatives
			// pipelineInfo.flags = VK_PIPELINE_CREATE_DERIVATIVE_BIT;
			pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
			pipelineInfo.basePipelineIndex = -1;              // Optional

			result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline);
			VULKAN_ERROR_CHECK(result, "vkCreateGraphicsPipelines");

			// Destroy

			vkDestroyShaderModule(device, fragShaderModule, nullptr);
			vkDestroyShaderModule(device, vertShaderModule, nullptr);
		}

		VkShaderModule createShaderModule(const char *code, u32 byteCount)
		{
			VkShaderModuleCreateInfo createInfo = {};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = byteCount;
			createInfo.pCode = (u32*)code; // TODO(jesus): Check that code is u32 aligned

			VkShaderModule shaderModule;
			VkResult result = vkCreateShaderModule(device, &createInfo, nullptr, &shaderModule);
			VULKAN_ERROR_CHECK(result, "vkCreateShaderModule");

			return shaderModule;
		}

		void createFramebuffers()
		{
			for (u32 i = 0; i < swapChainImagesCount; ++i)
			{
				VkImageView attachments[] = { swapChainImageViews[i] };

				VkFramebufferCreateInfo framebufferInfo = {};
				framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
				framebufferInfo.renderPass = renderPass;
				framebufferInfo.attachmentCount = 1;
				framebufferInfo.pAttachments = attachments;
				framebufferInfo.width = swapChainExtent.width;
				framebufferInfo.height = swapChainExtent.height;
				framebufferInfo.layers = 1;

				VkResult result = vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapChainFramebuffers[i]);
				VULKAN_ERROR_CHECK(result, "vkCreateFramebuffer");
			}
		}

		void createCommandPool()
		{
			QueueFamilyIndices queueFamilyIndices = findQueueFamilies(physicalDevice);

			VkCommandPoolCreateInfo poolInfo = {};
			poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
			poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
			poolInfo.flags = 0;

			VkResult res = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
			VULKAN_ERROR_CHECK(res, "vkCreateCommandPool");

		}

		void createCommandBuffers()
		{
			VkCommandBufferAllocateInfo allocInfo = {};
			allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
			allocInfo.commandPool = commandPool;
			allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
			allocInfo.commandBufferCount = (u32)ArrayCount(commandBuffers);

			VkResult res = vkAllocateCommandBuffers(device, &allocInfo, commandBuffers);
			VULKAN_ERROR_CHECK(res, "vkAllocateCommandBuffer");

			for (u32 i = 0; i < swapChainImagesCount; ++i)
			{
				VkCommandBufferBeginInfo beginInfo = {};
				beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
				beginInfo.flags = 0;                  // Optional
				beginInfo.pInheritanceInfo = nullptr; // Optional
				VkResult res = vkBeginCommandBuffer(commandBuffers[i], &beginInfo);
				VULKAN_ERROR_CHECK(res, "vkBeginCommandBuffer");

				VkRenderPassBeginInfo renderPassInfo = {};
				renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
				renderPassInfo.renderPass = renderPass;
				renderPassInfo.framebuffer = swapChainFramebuffers[i];
				renderPassInfo.renderArea.offset = {0, 0};
				renderPassInfo.renderArea.extent = swapChainExtent;

				VkClearValue clearColor = {0.0f, 0.0f, 0.0f, 1.0f};
				renderPassInfo.clearValueCount = 1;
				renderPassInfo.pClearValues = &clearColor;

				vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

				vkCmdBindPipeline(commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

				vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);

				vkCmdEndRenderPass(commandBuffers[i]);

				res = vkEndCommandBuffer(commandBuffers[i]);
				VULKAN_ERROR_CHECK(res, "vkEndCommandBuffer");
			}
		}

		void createSemaphores()
		{
			VkSemaphoreCreateInfo semaphoreInfo = {};
			semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

			VULKAN_ERROR_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore), "vkCreateSemaphore");
			VULKAN_ERROR_CHECK(vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore), "vkCreateSemaphore");
		}

		void drawFrame()
		{
			// Acquire image from swap chain
			u32 imageIndex;
			vkAcquireNextImageKHR(device, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

			// Execute the command buffer with that image as attachment in the framebuffer
			VkSubmitInfo submitInfo = {};
			submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

			VkSemaphore waitSemaphores[] = {imageAvailableSemaphore};
			VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
			submitInfo.waitSemaphoreCount = 1;
			submitInfo.pWaitSemaphores = waitSemaphores;
			submitInfo.pWaitDstStageMask = waitStages;
			submitInfo.commandBufferCount = 1;
			submitInfo.pCommandBuffers = &commandBuffers[imageIndex];

			VkSemaphore signalSemaphores[] = {renderFinishedSemaphore};
			submitInfo.signalSemaphoreCount = 1;
			submitInfo.pSignalSemaphores = signalSemaphores;

			VkResult res = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
			VULKAN_ERROR_CHECK(res, "vkQueueSubmit");

			// Return the image to the swap chain for presentation
			VkPresentInfoKHR presentInfo = {};
			presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
			presentInfo.waitSemaphoreCount = 1;
			presentInfo.pWaitSemaphores = signalSemaphores;

			VkSwapchainKHR swapChains[] = {swapChain};
			presentInfo.swapchainCount = 1;
			presentInfo.pSwapchains = swapChains;
			presentInfo.pImageIndices = &imageIndex;

			presentInfo.pResults = nullptr; // Optional

			vkQueuePresentKHR(presentQueue, &presentInfo);

			// TODO(jesus): This is very rudimentary...
			// remove in favour of several frames in flight
			vkQueueWaitIdle(presentQueue);
		}
};

int main()
{
	HelloTriangleApplication app;

	app.run();

	system("pause");
	return EXIT_SUCCESS;
}

