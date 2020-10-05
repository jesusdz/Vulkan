#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "platform.h"


#define LOG(format, ...) \
{ \
char Buffer[256]; \
wsprintf(Buffer, format"\n", __VA_ARGS__); \
OutputDebugStringA(Buffer); \
}

#define INVALID_INDEX U32_MAX

#define MAX_FRAMES_IN_FLIGHT 2

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 768

bool GlobalRunning = true;

bool GlobalResize = false;

// Vulkan stuff
VkInstance       VulkanInstance;
VkSurfaceKHR     VulkanSurface;
VkPhysicalDevice VulkanPhysicalDevice;
VkDevice         VulkanDevice = VK_NULL_HANDLE;
uint32_t         VulkanGraphicsQueueFamily;
uint32_t         VulkanPresentQueueFamily;
VkQueue          VulkanGraphicsQueue;
VkQueue          VulkanPresentQueue;
VkSwapchainKHR   VulkanSwapchain;
VkExtent2D       VulkanSwapchainExtent;
VkFormat         VulkanSwapchainImageFormat;
uint32_t         VulkanSwapchainImageCount;
VkImage          VulkanSwapchainImages[4];
VkImageView      VulkanSwapchainImageViews[4];
VkRenderPass     VulkanRenderPass;
VkPipelineLayout VulkanPipelineLayout;
VkPipeline       VulkanGraphicsPipeline;
VkFramebuffer    VulkanSwapchainFramebuffers[4];
VkCommandPool    VulkanCommandPool = VK_NULL_HANDLE;
VkCommandBuffer  VulkanCommandBuffers[4];
VkSemaphore      VulkanImageAvailableSemaphore[MAX_FRAMES_IN_FLIGHT];
VkSemaphore      VulkanRenderFinishedSemaphore[MAX_FRAMES_IN_FLIGHT];
VkFence          VulkanInFlightFences[MAX_FRAMES_IN_FLIGHT];
VkFence          VulkanInFlightImages[4];
VkBuffer         VulkanVertexBuffer;
VkDeviceMemory   VulkanVertexBufferMemory = VK_NULL_HANDLE;
VkBuffer         VulkanIndexBuffer;
VkDeviceMemory   VulkanIndexBufferMemory = VK_NULL_HANDLE;


struct vec2
{
    float x, y;
};

struct vec3
{
    float x, y, z;
};

struct vertex
{
    vec2 pos;
    vec3 color;
};

const vertex Vertices[] = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{ 0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{ 0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f,  0.5f}, {1.0f, 1.0f, 1.0f}}
};

const uint16_t Indices[] = {
    0, 1, 2, 2, 3, 0
};

struct app_data
{
    // Vulkan info
    uint32_t                  VkExtensionPropertyCount;
    VkExtensionProperties*    VkExtensionProperties;
    uint32_t                  VkLayerPropertyCount;
    VkLayerProperties*        VkLayerProperties;
    VkPhysicalDevice          VkPhysicalDevice;
    // TODO(jdiaz): Other queue indices go here
};

struct arena
{
    u32 Size;
    u32 Head;
    u8* Buffer;
};

inline arena MakeArena(u8* Buffer, u32 Size)
{
    arena Arena = { Size, 0, Buffer };
    return Arena;
}

u8* PushSize_(arena *Arena, u32 Size)
{
    Assert(Arena->Head + Size <= Arena->Size);
    u8* HeadPtr = Arena->Buffer + Arena->Head;
    Arena->Head += Size;
    return HeadPtr;
}

#define PushStruct(Arena, type)       (type*)PushSize_(Arena,         sizeof(type))
#define PushArray(Arena, type, Count) (type*)PushSize_(Arena, (Count)*sizeof(type))

enum { PlatformError_Fatal, PlatformError_Warning };

internal b32 StringsAreEqual(const char * A, const char * B)
{
    while (*A && *A == *B)
    {
        A++;
        B++;
    }
    
    return (*A == *B);
}

internal void Win32ErrorMessage(HWND Window, int Type, const char * Message)
{
    char *Caption = "Application error";
    
    UINT MBoxType = MB_OK;
    
    if (Type == PlatformError_Fatal)
    {
        MBoxType |= MB_ICONSTOP;
    }
    else
    {
        MBoxType |= MB_ICONWARNING;
    }
    
    MessageBoxExA(Window, Message, Caption, MBoxType, 0);
    
    if (Type == PlatformError_Fatal)
    {
        ExitProcess(1);
    }
}

struct win32_read_file_result
{
    u8* Bytes;
    u32 ByteCount;
};

internal void Win32DebugFreeMemory(u8* Bytes)
{
    VirtualFree(Bytes, 0, MEM_RELEASE);
}

internal win32_read_file_result Win32DebugReadFile(const char *FilePath)
{
    win32_read_file_result Res = {};
    
    DWORD HandlePermissions = GENERIC_READ;
    DWORD HandleCreation    = OPEN_EXISTING;
    
    HANDLE File = CreateFileA(FilePath, HandlePermissions, FILE_SHARE_READ, 0, HandleCreation, 0, 0);
    if (File != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER FileSize;
        if (GetFileSizeEx(File, &FileSize))
        {
            u8* Bytes = (u8*)VirtualAlloc(0, FileSize.QuadPart + 1, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
            
            if (Bytes)
            {
                DWORD ReadByteCount;
                if (ReadFile(File, Bytes, FileSize.QuadPart, &ReadByteCount, NULL))
                {
                    Bytes[ FileSize.QuadPart ] = 0;
                    Res.Bytes     = Bytes;
                    Res.ByteCount = FileSize.QuadPart;
                }
                else
                {
                    Win32DebugFreeMemory(Bytes);
                }
            }
        }
        
        CloseHandle(File);
    }
    
    return Res;
}


// Vulkan stuff ///////////////////////////////////////////////////////////////////////////////

struct vulkan_create_buffer_result
{
    VkBuffer       Buffer;
    VkDeviceMemory Memory;
};

internal uint32_t VulkanFindMemoryType(uint32_t TypeFilter, VkMemoryPropertyFlags Properties)
{
    VkPhysicalDeviceMemoryProperties MemProperties;
    vkGetPhysicalDeviceMemoryProperties(VulkanPhysicalDevice, &MemProperties);
    
    for (uint32_t i = 0; i < MemProperties.memoryTypeCount; ++i)
    {
        if (TypeFilter & (1 << i) && (MemProperties.memoryTypes[i].propertyFlags & Properties) == Properties)
        {
            return i;
        }
    }
    
    Assert(false && "Failed to find suitable memory type");
    return 0;
}

internal vulkan_create_buffer_result VulkanCreateBuffer(HWND Window, VkDeviceSize Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Properties)
{
    vulkan_create_buffer_result Res = {};
    
    // create buffer
    VkBufferCreateInfo VertexBufferCreateInfo = {};
    VertexBufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    VertexBufferCreateInfo.size        = Size;
    VertexBufferCreateInfo.usage       = Usage;
    VertexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(VulkanDevice, &VertexBufferCreateInfo, NULL, &Res.Buffer) != VK_SUCCESS) {
        Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to create vertex buffer");
    }
    
    // alloc buffer memory
    VkMemoryRequirements MemRequirements;
    vkGetBufferMemoryRequirements(VulkanDevice, Res.Buffer, &MemRequirements);
    
    VkMemoryAllocateInfo MemAllocInfo = {};
    MemAllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAllocInfo.allocationSize  = MemRequirements.size;
    MemAllocInfo.memoryTypeIndex = VulkanFindMemoryType(MemRequirements.memoryTypeBits, Properties);
    
    if (vkAllocateMemory(VulkanDevice, &MemAllocInfo, NULL, &Res.Memory) != VK_SUCCESS) {
        Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to allocate vertex buffer memory");
    }
    
    vkBindBufferMemory(VulkanDevice, Res.Buffer, Res.Memory, 0);
    
    return Res;
}

internal void VulkanCopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size)
{
    VkCommandBufferAllocateInfo AllocInfo = {};
    AllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    AllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    AllocInfo.commandPool = VulkanCommandPool;
    AllocInfo.commandBufferCount = 1;
    
    VkCommandBuffer CommandBuffer;
    vkAllocateCommandBuffers(VulkanDevice, &AllocInfo, &CommandBuffer);
    
    VkCommandBufferBeginInfo BeginInfo = {};
    BeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    BeginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    
    vkBeginCommandBuffer(CommandBuffer, &BeginInfo);
    
    VkBufferCopy CopyRegion = {};
    CopyRegion.srcOffset = 0;
    CopyRegion.dstOffset = 0;
    CopyRegion.size      = Size;
    vkCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, 1, &CopyRegion);
    
    vkEndCommandBuffer(CommandBuffer);
    
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers    = &CommandBuffer;
    
    vkQueueSubmit(VulkanGraphicsQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(VulkanGraphicsQueue);
    
    vkFreeCommandBuffers(VulkanDevice, VulkanCommandPool, 1, &CommandBuffer);
}

internal VkShaderModule VulkanCreateShaderModule(VkDevice Device, const u8* Bytes, u32 ByteCount)
{
    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
    ShaderModuleCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = ByteCount;
    ShaderModuleCreateInfo.pCode    = (const uint32_t*)Bytes;
    
    VkShaderModule ShaderModule;
    if (vkCreateShaderModule(Device, &ShaderModuleCreateInfo, NULL, &ShaderModule) != VK_SUCCESS) {
        Assert(false); // TODO(jdiaz): Make Window globally accessible or pass it into this function in order to call Win32ErrorMessage
    }
    
    return ShaderModule;
}

internal void VulkanCreateSwapchain(HWND Window)
{
    // Vulkan: Swap chain
    {
        VkSurfaceFormatKHR VulkanSurfaceFormat;
        VkPresentModeKHR   VulkanPresentMode;
        
        VkSurfaceCapabilitiesKHR Capabilities;
        VkSurfaceFormatKHR       Formats[64];
        VkPresentModeKHR         PresentModes[64];
        
        // choose the best format
        uint32_t FormatCount = ArrayCount(Formats);
        VkResult Res = vkGetPhysicalDeviceSurfaceFormatsKHR(VulkanPhysicalDevice, VulkanSurface, &FormatCount, Formats);
        Assert(Res == VK_SUCCESS);
        
        VulkanSurfaceFormat = Formats[0];
        for (u32 i = 1; i < FormatCount; ++i)
        {
            if (Formats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
                Formats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                VulkanSurfaceFormat = Formats[i];
                break;
            }
        }
        VulkanSwapchainImageFormat = VulkanSurfaceFormat.format;
        
        // choose the best present mode
        uint32_t PresentModesCount = ArrayCount(PresentModes);
        Res = vkGetPhysicalDeviceSurfacePresentModesKHR(VulkanPhysicalDevice, VulkanSurface, &PresentModesCount, PresentModes);
        Assert(Res == VK_SUCCESS);
        
        VulkanPresentMode = PresentModes[0];
        for (u32 i = 1; i < PresentModesCount; ++i)
        {
            if (PresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                VulkanPresentMode = PresentModes[i];
                break;
            }
        }
        
        // get the extent
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(VulkanPhysicalDevice, VulkanSurface, &Capabilities);
        if (Capabilities.currentExtent.width != UINT32_MAX)
        {
            VulkanSwapchainExtent = Capabilities.currentExtent;
        }
        else
        {
            // NOTE(jdiaz): Win32 code
            i32 Width = DEFAULT_WINDOW_WIDTH;
            i32 Height = DEFAULT_WINDOW_HEIGHT;
            RECT WindowRect = {};
            if (GetClientRect(Window, &WindowRect)) {
                Width = WindowRect.right - WindowRect.left;
                Height = WindowRect.bottom - WindowRect.top;
            }
            
            VulkanSwapchainExtent.width = Max(Capabilities.minImageExtent.width,  Min(Capabilities.maxImageExtent.width, Width));
            VulkanSwapchainExtent.height = Max(Capabilities.minImageExtent.height, Min(Capabilities.maxImageExtent.height, Height));
        }
        
        // set the number of images
        uint32_t ImageCount = Capabilities.minImageCount + 1;
        if (Capabilities.maxImageCount > 0)
            ImageCount = Min(ImageCount, Capabilities.maxImageCount);
        Assert(ImageCount <= ArrayCount(VulkanSwapchainImages));
        
        // create swap chain
        VkSwapchainCreateInfoKHR SwapchainCreateInfo = {};
        SwapchainCreateInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        SwapchainCreateInfo.surface          = VulkanSurface;
        SwapchainCreateInfo.minImageCount    = ImageCount;
        SwapchainCreateInfo.imageFormat      = VulkanSurfaceFormat.format;
        SwapchainCreateInfo.imageColorSpace  = VulkanSurfaceFormat.colorSpace;
        SwapchainCreateInfo.imageExtent      = VulkanSwapchainExtent;
        SwapchainCreateInfo.imageArrayLayers = 1; // 1 unless stereo images
        SwapchainCreateInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        
        uint32_t QueueFamilyIndices[] = { VulkanGraphicsQueueFamily, VulkanPresentQueueFamily };
        if ( VulkanGraphicsQueueFamily != VulkanPresentQueueFamily )
        {
            SwapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
            SwapchainCreateInfo.queueFamilyIndexCount = ArrayCount(QueueFamilyIndices);
            SwapchainCreateInfo.pQueueFamilyIndices   = QueueFamilyIndices;
        }
        else
        {
            SwapchainCreateInfo.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
            SwapchainCreateInfo.queueFamilyIndexCount = 0;    // opt
            SwapchainCreateInfo.pQueueFamilyIndices   = NULL; // opt
        }
        
        SwapchainCreateInfo.preTransform   = Capabilities.currentTransform;
        SwapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        SwapchainCreateInfo.presentMode    = VulkanPresentMode;
        SwapchainCreateInfo.clipped        = VK_TRUE;
        SwapchainCreateInfo.oldSwapchain   = VK_NULL_HANDLE;
        
        if (vkCreateSwapchainKHR(VulkanDevice, &SwapchainCreateInfo, NULL, &VulkanSwapchain) != VK_SUCCESS) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to create the swap chain");
        }
        
        VulkanSwapchainImageCount = ImageCount;
        Res = vkGetSwapchainImagesKHR(VulkanDevice, VulkanSwapchain, &VulkanSwapchainImageCount, VulkanSwapchainImages);
        Assert(Res == VK_SUCCESS);
        
        // create chapchain image views
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i)
        {
            VkImageViewCreateInfo ImageViewCreateInfo = {};
            ImageViewCreateInfo.sType    = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            ImageViewCreateInfo.image    = VulkanSwapchainImages[ i ];
            ImageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            ImageViewCreateInfo.format   = VulkanSwapchainImageFormat;
            ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            ImageViewCreateInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            ImageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
            ImageViewCreateInfo.subresourceRange.levelCount     = 1;
            ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
            ImageViewCreateInfo.subresourceRange.layerCount     = 1;
            
            if (vkCreateImageView(VulkanDevice, &ImageViewCreateInfo, NULL, &VulkanSwapchainImageViews[i]) != VK_SUCCESS) {
                Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to create image views for the swap chain");
            }
        }
    }
    
    // Vulkan: Render pass
    {
        // attachment description
        
        VkAttachmentDescription ColorAttachment = {};
        ColorAttachment.format         = VulkanSwapchainImageFormat;
        ColorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;
        ColorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ColorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        ColorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ColorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        ColorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        VkAttachmentReference ColorAttachmentRef = {};
        ColorAttachmentRef.attachment = 0; // index in the pAttachments render pass global array
        ColorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkSubpassDescription Subpass = {};
        Subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS;
        Subpass.colorAttachmentCount = 1;
        Subpass.pColorAttachments    = &ColorAttachmentRef; // position in this array is the attachment location in the shader
        
        // NOTE(jdiaz): Didn't understand these flags very well...
        VkSubpassDependency SubpassDependency = {};
        SubpassDependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        SubpassDependency.dstSubpass    = 0;
        SubpassDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        SubpassDependency.srcAccessMask = 0;
        SubpassDependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        SubpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        VkRenderPassCreateInfo RenderPassCreateInfo = {};
        RenderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        RenderPassCreateInfo.attachmentCount = 1;
        RenderPassCreateInfo.pAttachments    = &ColorAttachment;
        RenderPassCreateInfo.subpassCount    = 1;
        RenderPassCreateInfo.pSubpasses      = &Subpass;
        RenderPassCreateInfo.dependencyCount = 1;
        RenderPassCreateInfo.pDependencies   = &SubpassDependency;
        
        if (vkCreateRenderPass(VulkanDevice, &RenderPassCreateInfo, NULL, &VulkanRenderPass) != VK_SUCCESS) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Render pass could not be created");
        }
    }
    
    // Vulkan: Graphics pipeline
    {
        // shader modules
        
        win32_read_file_result VertexShaderFile   = Win32DebugReadFile("vertex_shader.spv");
        Assert(VertexShaderFile.Bytes);
        
        win32_read_file_result FragmentShaderFile = Win32DebugReadFile("fragment_shader.spv");
        Assert(FragmentShaderFile.Bytes);
        
        VkShaderModule VertexShaderModule = VulkanCreateShaderModule(VulkanDevice, VertexShaderFile.Bytes, VertexShaderFile.ByteCount);
        VkShaderModule FragmentShaderModule = VulkanCreateShaderModule(VulkanDevice, FragmentShaderFile.Bytes, FragmentShaderFile.ByteCount);
        
        Win32DebugFreeMemory(VertexShaderFile.Bytes);
        Win32DebugFreeMemory(FragmentShaderFile.Bytes);
        
        VkPipelineShaderStageCreateInfo VSStageCreateInfo = {};
        VSStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        VSStageCreateInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT;
        VSStageCreateInfo.module = VertexShaderModule;
        VSStageCreateInfo.pName  = "main";
        
        VkPipelineShaderStageCreateInfo FSStageCreateInfo = {};
        FSStageCreateInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        FSStageCreateInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
        FSStageCreateInfo.module = FragmentShaderModule;
        FSStageCreateInfo.pName  = "main";
        
        VkPipelineShaderStageCreateInfo ShaderStages[] = { VSStageCreateInfo, FSStageCreateInfo };
        
        // Vertex input
        
        VkVertexInputBindingDescription VertexBindingDescription = {};
        VertexBindingDescription.binding   = 0;
        VertexBindingDescription.stride    = sizeof(vertex);
        VertexBindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // Change to INSTANCE for instancing
        
        VkVertexInputAttributeDescription VertexAttributesDescription[2] = {};
        VertexAttributesDescription[0].binding  = 0;
        VertexAttributesDescription[0].location = 0;
        VertexAttributesDescription[0].format   = VK_FORMAT_R32G32_SFLOAT;
        VertexAttributesDescription[0].offset   = OffsetOf(vertex, pos);
        VertexAttributesDescription[1].binding  = 0;
        VertexAttributesDescription[1].location = 1;
        VertexAttributesDescription[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        VertexAttributesDescription[1].offset   = OffsetOf(vertex, color);
        
        VkPipelineVertexInputStateCreateInfo VertexInputCreateInfo = {};
        VertexInputCreateInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        VertexInputCreateInfo.vertexBindingDescriptionCount   = 1;
        VertexInputCreateInfo.pVertexBindingDescriptions      = &VertexBindingDescription;
        VertexInputCreateInfo.vertexAttributeDescriptionCount = ArrayCount(VertexAttributesDescription);
        VertexInputCreateInfo.pVertexAttributeDescriptions    = VertexAttributesDescription;
        
        // Input assembly
        
        VkPipelineInputAssemblyStateCreateInfo InputAssemblyCreateInfo = {};
        InputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        InputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        InputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;
        
        // Viewports / scissors
        
        VkViewport Viewport = {};
        Viewport.x = 0.0f;
        Viewport.y = 0.0f;
        Viewport.width  = (float)VulkanSwapchainExtent.width;
        Viewport.height = (float)VulkanSwapchainExtent.height;
        Viewport.minDepth = 0.0f;
        Viewport.maxDepth = 1.0;
        
        VkRect2D Scissor = {};
        Scissor.extent = VulkanSwapchainExtent;
        
        VkPipelineViewportStateCreateInfo ViewportStateCreateInfo = {};
        ViewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        ViewportStateCreateInfo.viewportCount = 1;
        ViewportStateCreateInfo.pViewports    = &Viewport;
        ViewportStateCreateInfo.scissorCount = 1;
        ViewportStateCreateInfo.pScissors     = &Scissor;
        
        // Rasterizer
        
        VkPipelineRasterizationStateCreateInfo RasterizerCreateInfo = {};
        RasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        RasterizerCreateInfo.depthClampEnable        = VK_FALSE;
        RasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE;
        RasterizerCreateInfo.polygonMode             = VK_POLYGON_MODE_FILL;
        RasterizerCreateInfo.lineWidth               = 1.0f;
        RasterizerCreateInfo.cullMode                = VK_CULL_MODE_BACK_BIT;
        RasterizerCreateInfo.frontFace               = VK_FRONT_FACE_CLOCKWISE;
        RasterizerCreateInfo.depthBiasEnable         = VK_FALSE;
        RasterizerCreateInfo.depthBiasConstantFactor = 0.0f;
        RasterizerCreateInfo.depthBiasClamp          = 0.0f;
        RasterizerCreateInfo.depthBiasSlopeFactor    = 0.0f;
        
        // Multisampling
        
        VkPipelineMultisampleStateCreateInfo MultisampleCreateInfo = {};
        MultisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        MultisampleCreateInfo.sampleShadingEnable   = VK_FALSE;
        MultisampleCreateInfo.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
        MultisampleCreateInfo.minSampleShading      = 1.0f;
        MultisampleCreateInfo.pSampleMask           = NULL;
        MultisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
        MultisampleCreateInfo.alphaToOneEnable      = VK_FALSE;
        
        // Depth / stencil
        
        // TODO(jdiaz): Complete this when we add a depth buffer to the swapchain
        
        // Color blending
        
        VkPipelineColorBlendAttachmentState ColorBlendAttachment = {};
        ColorBlendAttachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        ColorBlendAttachment.blendEnable         = VK_FALSE;
        ColorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
        ColorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
        ColorBlendAttachment.colorBlendOp        = VK_BLEND_OP_ADD;
        ColorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        ColorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
        ColorBlendAttachment.alphaBlendOp        = VK_BLEND_OP_ADD;
        
        VkPipelineColorBlendStateCreateInfo ColorBlendingCreateInfo = {};
        ColorBlendingCreateInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        ColorBlendingCreateInfo.logicOpEnable     = VK_FALSE;
        ColorBlendingCreateInfo.logicOp           = VK_LOGIC_OP_COPY;
        ColorBlendingCreateInfo.attachmentCount   = 1;
        ColorBlendingCreateInfo.pAttachments      = &ColorBlendAttachment;
        ColorBlendingCreateInfo.blendConstants[0] = 0.0f;
        ColorBlendingCreateInfo.blendConstants[1] = 0.0f;
        ColorBlendingCreateInfo.blendConstants[2] = 0.0f;
        ColorBlendingCreateInfo.blendConstants[3] = 0.0f;
        
        // Dynamic state
        
        /*
// Dynamic states must be specified at draw time (if we need them)
        VkDynamicState DynamicStates[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_LINE_WIDTH };
        
        VkPipelineDynamicStateCreateInfo DynamicStateCreateInfo = {};
        DynamicStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        DynamicStateCreateInfo.dynamicStateCount = ArrayCount(DynamicStates);
        DynamicStateCreateInfo.pDynamicStates    = DynamicStates;
*/
        
        // Pipeline layout
        
        VkPipelineLayout PipelineLayout;
        
        VkPipelineLayoutCreateInfo PipelineLayoutCreateInfo = {};
        PipelineLayoutCreateInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        PipelineLayoutCreateInfo.setLayoutCount         = 0;
        PipelineLayoutCreateInfo.pSetLayouts            = NULL;
        PipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        PipelineLayoutCreateInfo.pPushConstantRanges    = NULL;
        
        if (vkCreatePipelineLayout(VulkanDevice, &PipelineLayoutCreateInfo, NULL, &VulkanPipelineLayout) != VK_SUCCESS) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Could not create the pipeline layout");
        }
        
        // Graphics pipeline!!!
        
        VkGraphicsPipelineCreateInfo GraphicsPipelineCreateInfo = {};
        GraphicsPipelineCreateInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        GraphicsPipelineCreateInfo.stageCount          = 2;
        GraphicsPipelineCreateInfo.pStages             = ShaderStages;
        GraphicsPipelineCreateInfo.pVertexInputState   = &VertexInputCreateInfo;
        GraphicsPipelineCreateInfo.pInputAssemblyState = &InputAssemblyCreateInfo;
        GraphicsPipelineCreateInfo.pViewportState      = &ViewportStateCreateInfo;
        GraphicsPipelineCreateInfo.pRasterizationState = &RasterizerCreateInfo;
        GraphicsPipelineCreateInfo.pMultisampleState   = &MultisampleCreateInfo;
        GraphicsPipelineCreateInfo.pDepthStencilState  = NULL;
        GraphicsPipelineCreateInfo.pColorBlendState    = &ColorBlendingCreateInfo;
        GraphicsPipelineCreateInfo.pDynamicState       = NULL;
        
        GraphicsPipelineCreateInfo.layout              = VulkanPipelineLayout;
        
        GraphicsPipelineCreateInfo.renderPass          = VulkanRenderPass;
        GraphicsPipelineCreateInfo.subpass             = 0; // graphics subpass index
        
        GraphicsPipelineCreateInfo.basePipelineHandle  = VK_NULL_HANDLE;
        GraphicsPipelineCreateInfo.basePipelineIndex   = -1;
        
        if (vkCreateGraphicsPipelines(VulkanDevice, VK_NULL_HANDLE, 1, &GraphicsPipelineCreateInfo, NULL, &VulkanGraphicsPipeline) != VK_SUCCESS) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Graphics pipeline could not be created");
        }
        
        // Free stuff
        
        vkDestroyShaderModule(VulkanDevice, FragmentShaderModule, NULL);
        vkDestroyShaderModule(VulkanDevice, VertexShaderModule, NULL);
    }
    
    // Vulkan: Framebuffers for the swapchain
    {
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i)
        {
            VkFramebufferCreateInfo FramebufferCreateInfo = {};
            FramebufferCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            FramebufferCreateInfo.renderPass      = VulkanRenderPass;
            FramebufferCreateInfo.attachmentCount = 1;
            FramebufferCreateInfo.pAttachments    = &VulkanSwapchainImageViews[ i ];
            FramebufferCreateInfo.width           = VulkanSwapchainExtent.width;
            FramebufferCreateInfo.height          = VulkanSwapchainExtent.height;
            FramebufferCreateInfo.layers          = 1;
            
            if (vkCreateFramebuffer(VulkanDevice, &FramebufferCreateInfo, NULL, &VulkanSwapchainFramebuffers[i]) != VK_SUCCESS) {
                Win32ErrorMessage(Window, PlatformError_Fatal, "Swapchain framebuffer could not be created");
            }
        }
    }
    
    // Vulkan: Command pool
    {
        // command pool
        if (VulkanCommandPool == VK_NULL_HANDLE)
        {
            VkCommandPoolCreateInfo CmdPoolCreateInfo = {};
            CmdPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            CmdPoolCreateInfo.queueFamilyIndex = VulkanGraphicsQueueFamily;
            CmdPoolCreateInfo.flags            = 0; // flags to indicate the frequency of change of commands
            
            if (vkCreateCommandPool(VulkanDevice, &CmdPoolCreateInfo, NULL, &VulkanCommandPool) != VK_SUCCESS) {
                Win32ErrorMessage(Window, PlatformError_Fatal, "Command pool could not be created");
            }
        }
    }
    
    // Vulkan: Vertex buffer
    {
        if (VulkanVertexBuffer == VK_NULL_HANDLE)
        {
            VkDeviceSize BufferSize = sizeof(Vertices);
            
            // temporary staging buffer
            vulkan_create_buffer_result Staging =
                VulkanCreateBuffer(Window, BufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
            // copy vertices into memory
            void *Data;
            vkMapMemory(VulkanDevice, Staging.Memory, 0, BufferSize, 0, &Data);
            memcpy(Data, Vertices, BufferSize);
            vkUnmapMemory(VulkanDevice, Staging.Memory);
            
            vulkan_create_buffer_result Vertex =
                VulkanCreateBuffer(Window, BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VulkanVertexBuffer       = Vertex.Buffer;
            VulkanVertexBufferMemory = Vertex.Memory;
            
            VulkanCopyBuffer(Staging.Buffer, Vertex.Buffer, BufferSize);
            
            vkDestroyBuffer(VulkanDevice, Staging.Buffer, NULL);
            vkFreeMemory(VulkanDevice, Staging.Memory, NULL);
            
        }
    }
    
    // Vulkan: Index buffer
    {
        if (VulkanIndexBuffer == VK_NULL_HANDLE)
        {
            VkDeviceSize BufferSize = sizeof(Indices);
            
            // temporary staging buffer
            vulkan_create_buffer_result Staging =
                VulkanCreateBuffer(Window, BufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            
            // copy indices into memory
            void *Data;
            vkMapMemory(VulkanDevice, Staging.Memory, 0, BufferSize, 0, &Data);
            memcpy(Data, Indices, BufferSize);
            vkUnmapMemory(VulkanDevice, Staging.Memory);
            
            vulkan_create_buffer_result Index =
                VulkanCreateBuffer(Window, BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
            VulkanIndexBuffer       = Index.Buffer;
            VulkanIndexBufferMemory = Index.Memory;
            
            VulkanCopyBuffer(Staging.Buffer, Index.Buffer, BufferSize);
            
            vkDestroyBuffer(VulkanDevice, Staging.Buffer, NULL);
            vkFreeMemory(VulkanDevice, Staging.Memory, NULL);
            
        }
    }
    
    // Vulkan: Command buffers
    {
        // command buffers
        VkCommandBufferAllocateInfo CommandBufferAllocInfo = {};
        CommandBufferAllocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        CommandBufferAllocInfo.commandPool        = VulkanCommandPool;
        CommandBufferAllocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        CommandBufferAllocInfo.commandBufferCount = (uint32_t) VulkanSwapchainImageCount;
        
        if (vkAllocateCommandBuffers(VulkanDevice, &CommandBufferAllocInfo, VulkanCommandBuffers) != VK_SUCCESS) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Command buffers could not be created");
        }
        
        // start command buffer recording
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i)
        {
            VkCommandBufferBeginInfo BeginInfo = {};
            BeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            BeginInfo.flags            = 0;
            BeginInfo.pInheritanceInfo = NULL; // for secondary only
            
            if (vkBeginCommandBuffer(VulkanCommandBuffers[i], &BeginInfo) != VK_SUCCESS) {
                Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to begin recording command buffer");
            }
            
            // start render pass
            VkRenderPassBeginInfo RenderPassInfo = {};
            RenderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            RenderPassInfo.renderPass        = VulkanRenderPass;
            RenderPassInfo.framebuffer       = VulkanSwapchainFramebuffers[i];
            //RenderPassInfo.renderArea.offset  = {0, 0};
            RenderPassInfo.renderArea.extent = VulkanSwapchainExtent;
            VkClearValue ClearColors[] = {{0.0f, 0.0f, 0.0f, 1.0f}};
            RenderPassInfo.clearValueCount   = 1;
            RenderPassInfo.pClearValues      = ClearColors;
            
            vkCmdBeginRenderPass(VulkanCommandBuffers[i], &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            
            // bind pipeline
            vkCmdBindPipeline(VulkanCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanGraphicsPipeline);
            
            // vertex buffer binding
            VkBuffer VertexBuffers[] = {VulkanVertexBuffer};
            VkDeviceSize Offsets[] = {0};
            vkCmdBindVertexBuffers(VulkanCommandBuffers[i], 0, ArrayCount(VertexBuffers), VertexBuffers, Offsets);
            
            vkCmdBindIndexBuffer(VulkanCommandBuffers[i], VulkanIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
            
            // draw
            //vkCmdDraw(VulkanCommandBuffers[i], ArrayCount(Vertices), 1, 0, 0);
            vkCmdDrawIndexed(VulkanCommandBuffers[i], ArrayCount(Indices), 1, 0, 0, 0);
            
            vkCmdEndRenderPass(VulkanCommandBuffers[i]);
            
            if (vkEndCommandBuffer(VulkanCommandBuffers[i]) != VK_SUCCESS) {
                Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to record command buffer");
            }
        }
    }
}

internal void VulkanCleanupSwapchain()
{
    u32 Count = VulkanSwapchainImageCount;
    
    for (u32 i = 0; i < Count; ++i) {
        vkDestroyFramebuffer(VulkanDevice, VulkanSwapchainFramebuffers[i], NULL);
    }
    
    // we free the buffers, but reuse the command pool
    vkFreeCommandBuffers(VulkanDevice, VulkanCommandPool, Count, VulkanCommandBuffers);
    
    vkDestroyPipeline(VulkanDevice, VulkanGraphicsPipeline, NULL);
    vkDestroyPipelineLayout(VulkanDevice, VulkanPipelineLayout, NULL);
    vkDestroyRenderPass(VulkanDevice, VulkanRenderPass, NULL);
    
    for (u32 i = 0; i < Count; ++i) {
        vkDestroyImageView(VulkanDevice, VulkanSwapchainImageViews[i], NULL);
    }
    
    vkDestroySwapchainKHR(VulkanDevice, VulkanSwapchain, NULL);
}

internal void VulkanInit(HINSTANCE hInstance, HWND Window, arena& Arena, app_data *Data, i32 Width, i32 Height)
{
    const char* RequiredInstanceExtensions[] = {"VK_KHR_win32_surface", "VK_KHR_surface"};
    const char* RequiredDeviceExtensions[]   = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    
    VkResult Res;
    
    // Vulkan: Retrieving all instance extensions
    {
        vkEnumerateInstanceExtensionProperties(NULL, &Data->VkExtensionPropertyCount, NULL);
        Data->VkExtensionProperties = PushArray(&Arena, VkExtensionProperties, Data->VkExtensionPropertyCount);
        LOG("Extension count: %u", Data->VkExtensionPropertyCount);
        vkEnumerateInstanceExtensionProperties(NULL, &Data->VkExtensionPropertyCount, Data->VkExtensionProperties);
        
        for (u32 i = 0; i < Data->VkExtensionPropertyCount; ++i)
        {
            LOG("\t %s", Data->VkExtensionProperties[ i ].extensionName);
        }
    }
    
    const char* ValidationLayers[] = { "VK_LAYER_KHRONOS_validation" };
#define USE_VALIDATION_LAYERS
    
#if defined(USE_VALIDATION_LAYERS)
    // Vulkan: Check validation layers
    {
        vkEnumerateInstanceLayerProperties(&Data->VkLayerPropertyCount, NULL);
        Data->VkLayerProperties = PushArray(&Arena, VkLayerProperties, Data->VkLayerPropertyCount);
        vkEnumerateInstanceLayerProperties(&Data->VkLayerPropertyCount, Data->VkLayerProperties);
        
        for (u32 i = 0; i < ArrayCount(ValidationLayers); ++i)
        {
            b32 LayerFound = false;
            
            for (u32 j = 0; j < Data->VkLayerPropertyCount; ++j)
            {
                if (StringsAreEqual(ValidationLayers[ i ], Data->VkLayerProperties[ j ].layerName))
                {
                    LayerFound = true;
                    break;
                }
            }
            
            if (!LayerFound)
            {
                Win32ErrorMessage(Window, PlatformError_Fatal, "Could not find the validation layer");
            }
        }
    }
#endif
    
    // Vulkan: instance creation
    {
        VkApplicationInfo AppInfo  = {};
        AppInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        AppInfo.pApplicationName   = "Vulkan app";
        AppInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        AppInfo.pEngineName        = "No engine";
        AppInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
        AppInfo.apiVersion         = VK_API_VERSION_1_0;
        
        VkInstanceCreateInfo CreateInfo = {};
        CreateInfo.sType                   = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        CreateInfo.pApplicationInfo        = &AppInfo;
        CreateInfo.enabledExtensionCount   = ArrayCount(RequiredInstanceExtensions);
        CreateInfo.ppEnabledExtensionNames = RequiredInstanceExtensions;
#if defined(USE_VALIDATION_LAYERS)
        CreateInfo.enabledLayerCount       = ArrayCount(ValidationLayers);
        CreateInfo.ppEnabledLayerNames     = ValidationLayers;
#else
        CreateInfo.enabledLayerCount       = 0;
#endif
        
        if ( vkCreateInstance(&CreateInfo, NULL, &VulkanInstance) != VK_SUCCESS ) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to create Vulkan instance");
        }
    }
    
    // Vulkan: Setup debug messenger
    
    // TODO(jdiaz)
    
    // Vulkan: Window surface
    {
        VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {};
        SurfaceCreateInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        SurfaceCreateInfo.hwnd      = Window;
        SurfaceCreateInfo.hinstance = hInstance;
        
        if (vkCreateWin32SurfaceKHR(VulkanInstance, &SurfaceCreateInfo, NULL, &VulkanSurface) != VK_SUCCESS) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to create a Vulkan surface");
        }
    }
    
    // Vulkan: Pick physical device
    {
        uint32_t         PhysicalDeviceCount = 64;
        VkPhysicalDevice PhysicalDevices[ 64 ];
        Res = vkEnumeratePhysicalDevices(VulkanInstance, &PhysicalDeviceCount, PhysicalDevices);
        Assert(Res == VK_SUCCESS);
        
        u32 BestScore = 0;
        for (u32 i = 0; i < PhysicalDeviceCount; ++i)
        {
            // Check queues support
            
            uint32_t PhysicalDeviceQueueFamilyCount = 8;
            VkQueueFamilyProperties PhysicalDeviceQueueFamilies[ 8 ];
            vkGetPhysicalDeviceQueueFamilyProperties(PhysicalDevices[ i ], &PhysicalDeviceQueueFamilyCount, PhysicalDeviceQueueFamilies);
            Assert(PhysicalDeviceQueueFamilyCount < 8); // 8 is more than enough, this "less than" is not an error
            
            u32 GraphicsQueueIdx = INVALID_INDEX;
            u32 PresentQueueIdx  = INVALID_INDEX;
            for (u32 j = 0; j < PhysicalDeviceQueueFamilyCount; ++j)
            {
                if (PhysicalDeviceQueueFamilies[ j ].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    GraphicsQueueIdx = j;
                
                VkBool32 PresentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR( PhysicalDevices[ i ], j, VulkanSurface, &PresentSupport);
                if (PresentSupport)
                    PresentQueueIdx = j;
            }
            
            if (GraphicsQueueIdx == INVALID_INDEX || PresentQueueIdx == INVALID_INDEX)
                continue;
            
            
            // Check extensions support
            
            uint32_t DeviceExtensionsCount = 256;
            VkExtensionProperties DeviceExtensions[ 256 ];
            vkEnumerateDeviceExtensionProperties( PhysicalDevices[ i ], NULL, &DeviceExtensionsCount, NULL);
            Res = vkEnumerateDeviceExtensionProperties( PhysicalDevices[ i ], NULL, &DeviceExtensionsCount, DeviceExtensions);
            Assert(Res == VK_SUCCESS);
            
            b32 AllRequiredExtensionsFound = true;
            for (u32 req_idx = 0; req_idx < ArrayCount(RequiredDeviceExtensions); ++req_idx)
            {
                bool RequiredExtensionFound = false;
                for (u32 ext_idx = 0; ext_idx < DeviceExtensionsCount; ++ext_idx)
                {
                    if (StringsAreEqual(RequiredDeviceExtensions[req_idx], DeviceExtensions[ext_idx].extensionName))
                    {
                        RequiredExtensionFound = true;
                        break;
                    }
                }
                if (!RequiredExtensionFound)
                {
                    AllRequiredExtensionsFound = false;
                    break;
                }
                
            }
            if (!AllRequiredExtensionsFound)
                continue;
            
            
            // Check swapchain support
            
            uint32_t FormatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(PhysicalDevices[ i ], VulkanSurface, &FormatCount, NULL);
            
            uint32_t PresentModesCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(PhysicalDevices[ i ], VulkanSurface, &PresentModesCount, NULL);
            
            b32 IsValidSwapChain = FormatCount != 0 && PresentModesCount != 0;
            if (!IsValidSwapChain)
                continue;
            
            
            // Check device properties
            
            u32 Score = 0;
            
            VkPhysicalDeviceProperties DeviceProperties;
            vkGetPhysicalDeviceProperties( PhysicalDevices[ i ], &DeviceProperties);
            
            if (DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                Score += 1000;
            
            Score += DeviceProperties.limits.maxImageDimension2D;
            
            
            // Check device features
            
            VkPhysicalDeviceFeatures DeviceFeatures;
            vkGetPhysicalDeviceFeatures( PhysicalDevices[ i ], &DeviceFeatures);
            
            if (!DeviceFeatures.geometryShader) Score = 0;
            
            
            // Get the most suitable device found so far
            if (Score > BestScore)
            {
                BestScore = Score;
                VulkanPhysicalDevice      = PhysicalDevices[ i ];
                VulkanGraphicsQueueFamily = GraphicsQueueIdx;
                VulkanPresentQueueFamily  = PresentQueueIdx;
            }
        }
        
        if (BestScore == 0) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to find a proper Vulkan physical device");
        }
    }
    
    // Vulkan: Logical device
    {
        f32 QueuePriorities[] = { 1.0f };
        
        uint32_t QueueIndices[] = { VulkanGraphicsQueueFamily, VulkanPresentQueueFamily };
        
        VkDeviceQueueCreateInfo QueueCreateInfos[ ArrayCount(QueueIndices) ] = {};
        for (u32 i = 0; i < 2; ++i)
        {
            QueueCreateInfos[ i ].sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            QueueCreateInfos[ i ].queueFamilyIndex = QueueIndices[ i ];
            QueueCreateInfos[ i ].queueCount       = 1;
            QueueCreateInfos[ i ].pQueuePriorities = QueuePriorities;
        }
        
        VkPhysicalDeviceFeatures DeviceFeatures = {};
        // TODO(jdiaz): Fill this structure when we know the features we need
        
        VkDeviceCreateInfo DeviceCreateInfo = {};
        DeviceCreateInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        DeviceCreateInfo.pQueueCreateInfos       = QueueCreateInfos;
        DeviceCreateInfo.queueCreateInfoCount    = ArrayCount( QueueCreateInfos );
        DeviceCreateInfo.pEnabledFeatures        = &DeviceFeatures;
        DeviceCreateInfo.enabledExtensionCount   = ArrayCount(RequiredDeviceExtensions);
        DeviceCreateInfo.ppEnabledExtensionNames = RequiredDeviceExtensions;
#if defined(USE_VALIDATION_LAYERS)
        DeviceCreateInfo.enabledLayerCount       = ArrayCount(ValidationLayers);
        DeviceCreateInfo.ppEnabledLayerNames     = ValidationLayers;
#else
        DeviceCreateInfo.enabledLayerCount       = 0;
#endif
        
        if (vkCreateDevice(VulkanPhysicalDevice, &DeviceCreateInfo, NULL, &VulkanDevice) != VK_SUCCESS) {
            Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to create Vulkan logical device");
        }
        
        vkGetDeviceQueue(VulkanDevice, VulkanGraphicsQueueFamily, 0, &VulkanGraphicsQueue);
        
        vkGetDeviceQueue(VulkanDevice, VulkanPresentQueueFamily,  0, &VulkanPresentQueue);
    }
    
    VulkanCreateSwapchain(Window);
    
    // Vulkan: Semaphore creation
    {
        VkSemaphoreCreateInfo SemaphoreCreateInfo = {};
        SemaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        
        VkFenceCreateInfo FenceCreateInfo = {};
        FenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        FenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        
        for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            if (vkCreateSemaphore(VulkanDevice, &SemaphoreCreateInfo, NULL, &VulkanImageAvailableSemaphore[i]) != VK_SUCCESS
                || vkCreateSemaphore(VulkanDevice, &SemaphoreCreateInfo, NULL, &VulkanRenderFinishedSemaphore[i]) != VK_SUCCESS
                || vkCreateFence(VulkanDevice, &FenceCreateInfo, NULL, &VulkanInFlightFences[i]) != VK_SUCCESS) {
                Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to create semaphores");
            }
        }
        
        for (u32 i = 0; i < 4; ++i)
        {
            VulkanInFlightImages[i] = VK_NULL_HANDLE;
        }
    }
}

internal void VulkanCleanup()
{
    // wait the logical device to finish operations
    vkDeviceWaitIdle(VulkanDevice);
    
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(VulkanDevice, VulkanRenderFinishedSemaphore[i], NULL);
        vkDestroySemaphore(VulkanDevice, VulkanImageAvailableSemaphore[i], NULL);
        vkDestroyFence(VulkanDevice, VulkanInFlightFences[i], NULL);
    }
    
    VulkanCleanupSwapchain();
    
    vkDestroyBuffer(VulkanDevice, VulkanVertexBuffer, NULL);
    vkFreeMemory(VulkanDevice, VulkanVertexBufferMemory, NULL);
    vkDestroyBuffer(VulkanDevice, VulkanIndexBuffer, NULL);
    vkFreeMemory(VulkanDevice, VulkanIndexBufferMemory, NULL);
    
    vkDestroyCommandPool(VulkanDevice, VulkanCommandPool, NULL);
    
    vkDestroyDevice(VulkanDevice, NULL);
    vkDestroySurfaceKHR(VulkanInstance, VulkanSurface, NULL);
    vkDestroyInstance(VulkanInstance, NULL);
}

LRESULT CALLBACK WinProc(HWND Window,      // handle to window
                         UINT uMsg,        // message identifier
                         WPARAM wParam,    // first message parameter
                         LPARAM lParam)    // second message parameter
{
    switch (uMsg)
    {
        case WM_CREATE:
        // Initialize the window
        return 0;
        
        //case WM_PAINT:
        // Paint the window's client area
        //return 0;
        
        case WM_SETFOCUS:
        // The window has gained the focus
        OutputDebugStringA("WM_SETFOCUS\n");
        return 0;
        
        case WM_KILLFOCUS:
        // The windows is about to lose the focus
        OutputDebugStringA("WM_KILLFOCUS\n");
        return 0;
        
        case WM_SIZE:
        // Set the size and position of the window
        GlobalResize = true;
        return 0;
        
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
        case WM_MBUTTONDOWN:
        case WM_MBUTTONUP:
        case WM_MOUSEMOVE:
        // Handle mouse button events
        {
            const bool ShiftKeyIsDown = wParam & MK_SHIFT;
            const bool CtrlKeyIsDown  = wParam & MK_CONTROL;
            const bool LButtonIsDown  = wParam & MK_LBUTTON;
            const bool RButtonIsDown  = wParam & MK_RBUTTON;
            const bool MButtonIsDown  = wParam & MK_MBUTTON;
            const i32  MouseX         = LO_WORD(lParam);
            const i32  MouseY         = HI_WORD(lParam);
            /*
            char Buffer[256];
            wsprintf(Buffer, "Mouse position X:%d Y:%d\n", MouseX, MouseY);
            OutputDebugStringA(Buffer);*/
        }
        return 0;
        
        case WM_KEYDOWN:
        case WM_KEYUP:
        {
            //if (wParam == VK_ESCAPE)
            //{
            //}
            if (uMsg == WM_KEYDOWN) OutputDebugStringA("WM_KEYDOWN: ");
            else                    OutputDebugStringA("EM_KEYUP:   ");
            
            const u32 TransitionBit = IS_BIT_SET(lParam, 31);
            
            char Buffer[256];
            wsprintf(Buffer, "Transition bit %u\n", TransitionBit);
            OutputDebugStringA(Buffer);
        }
        return 0;
        
        case WM_DESTROY: 
        // Clean up window-specific data objects. 
        GlobalRunning = false;
        
        return 0; 
        
        // 
        // Process other messages. 
        // 
        
        default: 
        return DefWindowProc(Window, uMsg, wParam, lParam); 
    } 
    return 0; 
}

int WinMain(
            HINSTANCE hInstance,
            HINSTANCE hPrevInstance,
            LPSTR     lpCmdLine,
            int       nShowCmd
            )
{
    // Window creation
    
    WNDCLASSA WindowClass     = {};
    WindowClass.style         = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
    WindowClass.lpfnWndProc   = WinProc;
    //WindowClass.cbClsExtra    = ;
    //WindowClass.cbWndExtra    = ;
    WindowClass.hInstance     = hInstance;
    WindowClass.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    WindowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    WindowClass.lpszClassName = "Platform template";
    
    ATOM Atom   = RegisterClassA( &WindowClass );
    
    DWORD WindowStyle  = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
    i32   WindowWidth  = DEFAULT_WINDOW_WIDTH;
    i32   WindowHeight = DEFAULT_WINDOW_HEIGHT;
    RECT  WindowRect   = { 0, 0, WindowWidth, WindowHeight };
    if ( AdjustWindowRect( &WindowRect, WindowStyle, FALSE ) ) {
        WindowWidth  = WindowRect.right - WindowRect.left;
        WindowHeight = WindowRect.bottom - WindowRect.top;
    }
    
    HWND Window = CreateWindowExA(WS_EX_APPWINDOW,
                                  "Platform template",
                                  "Platform template",
                                  WindowStyle,
                                  CW_USEDEFAULT, CW_USEDEFAULT,
                                  WindowWidth, WindowHeight,
                                  NULL,
                                  NULL,
                                  hInstance,
                                  NULL
                                  );
    
    if ( Window )
    {
        // Memory initialization
        
        u32 MemorySize   = MB(512);
        u8* MemoryBuffer = (u8*)VirtualAlloc(0, MemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        Assert(MemoryBuffer);
        arena Arena      = MakeArena(MemoryBuffer, MemorySize);
        
        app_data* Data = PushStruct(&Arena, app_data);
        
        VulkanInit(hInstance, Window, Arena, Data, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
        
        // Application loop
        u32 CurrentFrame = 0;
        
        MSG Msg = {};
        while ( GlobalRunning )
        {
            while ( PeekMessageA(&Msg,
                                 Window,
                                 0, 0,
                                 PM_REMOVE) )
            {
                TranslateMessage( &Msg );
                DispatchMessage ( &Msg );
            }
            
            // Avoid rendering minimized windows
            RECT WindowRect;
            if (GetClientRect(Window, &WindowRect)) {
                if (WindowRect.right - WindowRect.left == 0 || WindowRect.bottom - WindowRect.top == 0) {
                    continue;
                }
            }
            else continue;
            
            // Vulkan: Drawing
            {
                // Wait for this frame fence
                vkWaitForFences(VulkanDevice, 1, &VulkanInFlightFences[CurrentFrame], VK_TRUE, UINT64_MAX);
                
                // aquire an image from the swapchain
                uint32_t ImageIndex;
                VkResult Res = vkAcquireNextImageKHR(VulkanDevice, VulkanSwapchain, UINT64_MAX, VulkanImageAvailableSemaphore[CurrentFrame], VK_NULL_HANDLE, &ImageIndex);
                
                // check if the swapchain needs to be recreated
                if (Res == VK_ERROR_OUT_OF_DATE_KHR)
                {
                    vkDeviceWaitIdle(VulkanDevice);
                    VulkanCleanupSwapchain();
                    VulkanCreateSwapchain(Window);
                }
                else if (Res != VK_SUCCESS && Res != VK_SUBOPTIMAL_KHR)
                {
                    Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to acquire swapchain image");
                }
                
                // this is just in case the image returned by vkAquireNextImage is not consecutive
                if (VulkanInFlightImages[ImageIndex] != VK_NULL_HANDLE)
                {
                    vkWaitForFences(VulkanDevice, 1, &VulkanInFlightImages[ImageIndex], VK_TRUE, UINT64_MAX);
                }
                VulkanInFlightImages[ImageIndex] = VulkanInFlightFences[CurrentFrame];
                
                vkResetFences(VulkanDevice, 1, &VulkanInFlightFences[CurrentFrame]);
                
                // submitting the command buffer
                VkSemaphore          WaitSemaphores[]   = {VulkanImageAvailableSemaphore[CurrentFrame]};
                VkSemaphore          SignalSemaphores[] = {VulkanRenderFinishedSemaphore[CurrentFrame]};
                VkPipelineStageFlags WaitStages[]       = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
                
                VkSubmitInfo SubmitInfo = {};
                SubmitInfo.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
                SubmitInfo.waitSemaphoreCount   = 1;
                SubmitInfo.pWaitSemaphores      = WaitSemaphores;
                SubmitInfo.pWaitDstStageMask    = WaitStages;
                SubmitInfo.commandBufferCount   = 1;
                SubmitInfo.pCommandBuffers      = &VulkanCommandBuffers[ImageIndex];
                SubmitInfo.signalSemaphoreCount = 1;
                SubmitInfo.pSignalSemaphores    = SignalSemaphores;
                
                if (vkQueueSubmit(VulkanGraphicsQueue, 1, &SubmitInfo, 
                                  VulkanInFlightFences[CurrentFrame]) != VK_SUCCESS) {
                    Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to submit draw command buffer");
                }
                
                VkSwapchainKHR Swapchains[] = {VulkanSwapchain};
                
                // presentation
                VkPresentInfoKHR PresentInfo = {};
                PresentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
                PresentInfo.waitSemaphoreCount = 1;
                PresentInfo.pWaitSemaphores    = SignalSemaphores;
                PresentInfo.swapchainCount     = 1;
                PresentInfo.pSwapchains        = Swapchains;
                PresentInfo.pImageIndices      = &ImageIndex;
                PresentInfo.pResults           = NULL;
                
                Res = vkQueuePresentKHR(VulkanPresentQueue, &PresentInfo);
                
                if (Res == VK_ERROR_OUT_OF_DATE_KHR || Res == VK_SUBOPTIMAL_KHR || GlobalResize)
                {
                    GlobalResize = false;
                    vkDeviceWaitIdle(VulkanDevice);
                    VulkanCleanupSwapchain();
                    VulkanCreateSwapchain(Window);
                }
                else if (Res != VK_SUCCESS)
                {
                    Win32ErrorMessage(Window, PlatformError_Fatal, "Failed to present swapchain image");
                }
                
                // NOTE(jdiaz): Not needed because we are using fences
                //vkQueueWaitIdle(VulkanPresentQueue);
                
                CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
            }
        }
        
        VulkanCleanup();
    }
    else
    {
        LOG("Could not create the main window");
    }
    
    DestroyWindow(Window);
    
    UnregisterClassA(WindowClass.lpszClassName, WindowClass.hInstance);
    
    return 0;
}