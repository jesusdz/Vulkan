#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <windows.h>

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include "platform.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define LOG(format, ...) \
{ \
char Buffer[256]; \
wsprintf(Buffer, format"\n", __VA_ARGS__); \
OutputDebugStringA(Buffer); \
}

#define INVALID_INDEX U32_MAX

#define DEFAULT_WINDOW_WIDTH 1024
#define DEFAULT_WINDOW_HEIGHT 768

#define MAX_SWAPCHAIN_IMAGES 4
#define MAX_FRAMES_IN_FLIGHT 2


// Types //////////////////////////////////////////////////////////////////////////////////////////

struct vec2
{
    f32 x, y;
};

struct vec3
{
    f32 x, y, z;
};

struct mat3
{
    f32 data[3][3];
};

struct mat4
{
    f32 data[4][4];
};

struct vertex
{
    vec3 pos;
    vec3 color;
    vec2 texCoord;
};

struct uniform_buffer_object
{
    mat4 model;
    mat4 view;
    mat4 proj;
};

struct arena
{
    u32 Size;
    u32 Head;
    u8* Buffer;
};

struct scratch_block
{
    arena Arena;
    
    scratch_block();
    scratch_block(u64 Size);
    ~scratch_block();
    
    operator arena*() { return &Arena; }
};

struct scratch_memory
{
    u32 Size;
    u32 Head;
    u8* Buffer;
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

struct application
{
    HWND           Window;
    HINSTANCE      Instance;
    b32            Running;
    b32            Resize;
    scratch_memory ScratchMemory;
};

struct win32_read_file_result
{
    u8* Bytes;
    u32 ByteCount;
};

struct vulkan_create_buffer_result
{
    VkBuffer       Buffer;
    VkDeviceMemory Memory;
};

struct vulkan_create_image_result
{
    VkImage        Image;
    VkDeviceMemory Memory;
};


// Globals ////////////////////////////////////////////////////////////////////////////////////////

// Vulkan stuff
VkInstance            VulkanInstance;
VkSurfaceKHR          VulkanSurface;
VkPhysicalDevice      VulkanPhysicalDevice;
VkDevice              VulkanDevice;
uint32_t              VulkanGraphicsQueueFamily;
uint32_t              VulkanPresentQueueFamily;
VkQueue               VulkanGraphicsQueue;
VkQueue               VulkanPresentQueue;
VkSwapchainKHR        VulkanSwapchain;
VkExtent2D            VulkanSwapchainExtent;
VkFormat              VulkanSwapchainImageFormat;
uint32_t              VulkanSwapchainImageCount;
VkImage               VulkanSwapchainImages[MAX_SWAPCHAIN_IMAGES];
VkImageView           VulkanSwapchainImageViews[MAX_SWAPCHAIN_IMAGES];
VkImage               VulkanColorImage;
VkDeviceMemory        VulkanColorImageMemory;
VkImageView           VulkanColorImageView;
VkImage               VulkanDepthImage;
VkDeviceMemory        VulkanDepthImageMemory;
VkImageView           VulkanDepthImageView;
VkFormat              VulkanDepthFormat;
b32                   VulkanDepthHasStencil;
VkRenderPass          VulkanRenderPass;
VkDescriptorSetLayout VulkanDescriptorSetLayout;
VkPipelineLayout      VulkanPipelineLayout;
VkPipeline            VulkanGraphicsPipeline;
VkFramebuffer         VulkanSwapchainFramebuffers[MAX_SWAPCHAIN_IMAGES];
VkCommandPool         VulkanCommandPool;
VkCommandBuffer       VulkanCommandBuffers[MAX_SWAPCHAIN_IMAGES];
VkSemaphore           VulkanImageAvailableSemaphore[MAX_FRAMES_IN_FLIGHT];
VkSemaphore           VulkanRenderFinishedSemaphore[MAX_FRAMES_IN_FLIGHT];
VkFence               VulkanInFlightFences[MAX_FRAMES_IN_FLIGHT];
VkFence               VulkanInFlightImages[MAX_SWAPCHAIN_IMAGES];
VkBuffer              VulkanVertexBuffer;
VkDeviceMemory        VulkanVertexBufferMemory;
VkBuffer              VulkanIndexBuffer;
VkDeviceMemory        VulkanIndexBufferMemory;
VkBuffer              VulkanUniformBuffers[MAX_SWAPCHAIN_IMAGES];
VkDeviceMemory        VulkanUniformBuffersMemory[MAX_SWAPCHAIN_IMAGES];
VkDescriptorPool      VulkanDescriptorPool;
VkDescriptorSet       VulkanDescriptorSets[MAX_SWAPCHAIN_IMAGES];
VkImage               VulkanTextureImage;
VkDeviceMemory        VulkanTextureImageMemory;
VkImageView           VulkanTextureImageView;
VkSampler             VulkanTextureSampler;
uint32_t              VulkanMipLevels;
VkSampleCountFlagBits VulkanMSAASampleCount;

internal application App;

const vertex Vertices[] = {
    {{-0.5f, -0.5f, 0.0f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, 0.0f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, 0.0f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}},
    
    {{-0.5f, -0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}, {0.0f, 0.0f}},
    {{ 0.5f, -0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}, {1.0f, 0.0f}},
    {{ 0.5f,  0.5f, -0.5f}, {0.0f, 0.0f, 1.0f}, {1.0f, 1.0f}},
    {{-0.5f,  0.5f, -0.5f}, {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}
};

const uint16_t Indices[] = {
    0, 1, 2, 2, 3, 0,
    4, 5, 6, 6, 7, 4
};


// Functions //////////////////////////////////////////////////////////////////////////////////////

u8* CommitScratchMemoryBlock(u64 Size)
{
    Assert(App.ScratchMemory.Head + Size <= App.ScratchMemory.Size);
    LPVOID BaseAddress = (LPVOID) (App.ScratchMemory.Buffer + App.ScratchMemory.Head);
    u8* Res = (u8*)VirtualAlloc(BaseAddress, Size, MEM_COMMIT, PAGE_READWRITE);
    App.ScratchMemory.Head += Size;
    return Res;
}

void DecommitScratchMemoryBlock(u8* Base, u64 Size)
{
    Assert(Base >= App.ScratchMemory.Buffer && Base + Size <= App.ScratchMemory.Buffer + App.ScratchMemory.Size);
    BOOL Success = VirtualFree(Base, Size, MEM_DECOMMIT);
    Assert(Success);
}

scratch_block::scratch_block()
{
    Arena.Size   = MB(1);
    Arena.Head   = 0;
    Arena.Buffer = CommitScratchMemoryBlock(Arena.Size);
}

scratch_block::scratch_block(u64 Size)
{
    Arena.Size   = Size;
    Arena.Head   = 0;
    Arena.Buffer = CommitScratchMemoryBlock(Arena.Size);
}

scratch_block::~scratch_block()
{
    DecommitScratchMemoryBlock(Arena.Buffer, Arena.Size);
}

inline arena MakeArena(u8* Buffer, u32 Size)
{
    arena Arena = { Size, 0, Buffer };
    return Arena;
}

inline u8* PushSize_(arena *Arena, u32 Size)
{
    Assert(Arena->Head + Size <= Arena->Size);
    u8* HeadPtr = Arena->Buffer + Arena->Head;
    Arena->Head += Size;
    return HeadPtr;
}

#define PushStruct(Arena, type)       (type*)PushSize_(Arena,         sizeof(type))
#define PushArray(Arena, type, Count) (type*)PushSize_(Arena, (Count)*sizeof(type))


internal b32 StringsAreEqual(const char * A, const char * B)
{
    while (*A && *A == *B)
    {
        A++;
        B++;
    }
    
    return (*A == *B);
}

#include <math.h>
internal f32 Floor(f32 Value)
{
    f32 Res = ::floorf(Value);
    return Res;
}

internal f32 Log2(f32 Value)
{
    f32 Res = ::logf(Value) / ::logf(2.0f);
    return Res;
}

internal f32 Sinf(f32 Rad)
{
    f32 Res = ::sinf(Rad);
    return Res;
}

internal f32 Cosf(f32 Rad)
{
    f32 Res = ::cosf(Rad);
    return Res;
}

internal f32 Tanf(f32 Rad)
{
    f32 Res = ::tanf(Rad);
    return Res;
}

internal f32 Sqrtf(f32 a)
{
    f32 Res = ::sqrtf(a);
    return Res;
}

internal f32 Radians(f32 Degrees)
{
    f32 Res = PI * Degrees / 180.0f;
    return Res;
}

internal vec3 V3()
{
    vec3 Res = {0.0f, 0.0f, 0.0f};
    return Res;
}

internal vec3 V3(f32 x)
{
    vec3 Res = {x, x, x};
    return Res;
}

internal vec3 V3(f32 x, f32 y, f32 z)
{
    vec3 Res = {x, y, z};
    return Res;
}

internal f32 Dot(const vec3 &a, const vec3 &b)
{
    f32 Res = a.x * b.x + a.y * b.y + a.z * b.z;
    return Res;
}

internal vec3 Cross(const vec3 &a, const vec3 &b)
{
    vec3 Res = {
        a.y * b.z - b.y * a.z,
        -(a.x * b.z - b.x * a.z),
        a.x * b.y - b.x * a.y
    };
    return Res;
}

internal vec3 operator- (const vec3 &a, const vec3 &b)
{
    vec3 Res = {a.x-b.x, a.y-b.y, a.z-b.z};
    return Res;
}

internal vec3 operator- (const vec3 &a)
{
    vec3 Res = {-a.x, -a.y, -a.z};
    return Res;
}

internal vec3 operator/ (const vec3 &a, f32 b)
{
    vec3 Res = {a.x/b, a.y/b, a.z/b};
    return Res;
}

internal vec3 operator* (const vec3 &a, f32 b)
{
    vec3 Res = {a.x*b, a.y*b, a.z*b};
    return Res;
}

internal f32 Length(const vec3 &a)
{
    f32 Res = Sqrtf(Dot(a,a));
    return Res;
}

internal vec3 Normalize(const vec3 &a)
{
    f32 InvLength = 1.0f / Length(a);
    vec3 Res = a * InvLength;
    return Res;
}

internal mat4 Identity()
{
    mat4 Res = {
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 1.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return Res;
}

internal mat4 Rotation(f32 Angle, const vec3 &Axis)
{
    f32 Cos = Cosf(Angle);
    f32 Ico = 1.0f - Cos;
    f32 Sin = Sinf(Angle);
    mat4 Res = {
        Cos + Axis.x*Axis.x*Ico, Axis.y*Axis.x*Ico + Axis.z*Sin, Axis.z*Axis.x*Ico - Axis.y*Sin, 0.0f,
        Axis.x*Axis.y*Ico - Axis.z*Sin, Cos + Axis.y*Axis.y*Ico, Axis.z*Axis.y*Ico + Axis.x*Sin, 0.0f,
        Axis.x*Axis.z*Ico + Axis.y*Sin, Axis.y*Axis.z*Ico - Axis.x*Sin, Cos + Axis.z*Axis.z*Ico, 0.0f,
        0.0f, 0.0f, 0.0f, 1.0f
    };
    return Res;
}

internal mat4 LookAt(const vec3 &Eye, const vec3 &Target, const vec3 &VUV)
{
    vec3 Z = Normalize(Eye - Target);
    vec3 X = Normalize(Cross(VUV, Z));
    vec3 Y = Cross(Z, X);
    vec3 W = - V3(Dot(X, Eye), Dot(Y, Eye), Dot(Z, Eye));
    mat4 Res     = {
        X.x, Y.x, Z.x, 0.0f,
        X.y, Y.y, Z.y, 0.0f,
        X.z, Y.z, Z.z, 0.0f,
        W.x, W.y, W.z, 1.0f
    };
    return Res;
}

// NOTE(jdiaz): Take into account that in Vulkan the projected Z range differs from OpenGL...
internal mat4 Perspective(f32 FovY, f32 Aspect, f32 Near, f32 Far)
{
    f32 t = Near * Tanf( FovY * 0.5f );
    f32 r = Aspect * t;
    mat4 Res = {
        Near / r, 0.0f, 0.0f, 0.0f,
        0.0f, Near / t, 0.0f, 0.0f,
        0.0f, 0.0f, -(Far+Near)/(Far-Near), -1.0f,
        0.0f, 0.0f, -2.0f*Far*Near/(Far-Near), 0.0f
    };
    return Res;
}

enum { PlatformError_Fatal, PlatformError_Warning };

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
    
    // TODO(jesus): Make this happen only if requested
    DebugBreak();
    
    if (Type == PlatformError_Fatal)
    {
        ExitProcess(1);
    }
}

#define ExitWithError(msg) Win32ErrorMessage(App.Window, PlatformError_Fatal, msg)

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

internal VkOffset3D VulkanOffset3D(int32_t X, int32_t Y, int32_t Z)
{
    VkOffset3D Res = { X, Y, Z };
    return Res;
}

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

internal vulkan_create_buffer_result VulkanCreateBuffer(VkDeviceSize Size, VkBufferUsageFlags Usage, VkMemoryPropertyFlags Properties)
{
    vulkan_create_buffer_result Res = {};
    
    // create buffer
    VkBufferCreateInfo VertexBufferCreateInfo = {};
    VertexBufferCreateInfo.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    VertexBufferCreateInfo.size        = Size;
    VertexBufferCreateInfo.usage       = Usage;
    VertexBufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(VulkanDevice, &VertexBufferCreateInfo, NULL, &Res.Buffer) != VK_SUCCESS) {
        ExitWithError("Failed to create vertex buffer");
    }
    
    // alloc buffer memory
    VkMemoryRequirements MemRequirements;
    vkGetBufferMemoryRequirements(VulkanDevice, Res.Buffer, &MemRequirements);
    
    VkMemoryAllocateInfo MemAllocInfo = {};
    MemAllocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    MemAllocInfo.allocationSize  = MemRequirements.size;
    MemAllocInfo.memoryTypeIndex = VulkanFindMemoryType(MemRequirements.memoryTypeBits, Properties);
    
    if (vkAllocateMemory(VulkanDevice, &MemAllocInfo, NULL, &Res.Memory) != VK_SUCCESS) {
        ExitWithError("Failed to allocate vertex buffer memory");
    }
    
    vkBindBufferMemory(VulkanDevice, Res.Buffer, Res.Memory, 0);
    
    return Res;
}

internal VkCommandBuffer VulkanBeginSingleTimeCommands()
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
    
    return CommandBuffer;
}

internal void VulkanEndSingleTimeCommands(VkCommandBuffer CommandBuffer)
{
    vkEndCommandBuffer(CommandBuffer);
    
    VkSubmitInfo SubmitInfo = {};
    SubmitInfo.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    SubmitInfo.commandBufferCount = 1;
    SubmitInfo.pCommandBuffers    = &CommandBuffer;
    
    vkQueueSubmit(VulkanGraphicsQueue, 1, &SubmitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(VulkanGraphicsQueue);
    
    vkFreeCommandBuffers(VulkanDevice, VulkanCommandPool, 1, &CommandBuffer);
}

internal void VulkanCopyBuffer(VkBuffer SrcBuffer, VkBuffer DstBuffer, VkDeviceSize Size)
{
    VkCommandBuffer CommandBuffer = VulkanBeginSingleTimeCommands();
    
    VkBufferCopy CopyRegion = {};
    CopyRegion.srcOffset = 0;
    CopyRegion.dstOffset = 0;
    CopyRegion.size      = Size;
    vkCmdCopyBuffer(CommandBuffer, SrcBuffer, DstBuffer, 1, &CopyRegion);
    
    VulkanEndSingleTimeCommands(CommandBuffer);
}

internal void VulkanCopyBufferToImage(VkBuffer Buffer, VkImage Image, uint32_t Width, uint32_t Height)
{
    VkCommandBuffer CommandBuffer = VulkanBeginSingleTimeCommands();
    
    VkBufferImageCopy Region = {};
    Region.bufferOffset                    = 0;
    Region.bufferRowLength                 = 0;
    Region.bufferImageHeight               = 0;
    Region.imageSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    Region.imageSubresource.mipLevel       = 0;
    Region.imageSubresource.baseArrayLayer = 0;
    Region.imageSubresource.layerCount     = 1;
    Region.imageOffset.x                   = 0;
    Region.imageOffset.y                   = 0;
    Region.imageOffset.z                   = 0;
    Region.imageExtent.width               = Width;
    Region.imageExtent.height              = Height;
    Region.imageExtent.depth               = 1;
    
    vkCmdCopyBufferToImage(CommandBuffer, Buffer, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Region);
    
    VulkanEndSingleTimeCommands(CommandBuffer);
}

internal void VulkanTransitionImageLayout(VkImage Image, VkFormat Format, VkImageLayout OldLayout, VkImageLayout NewLayout, u32 MipLevelCount)
{
    VkCommandBuffer CommandBuffer = VulkanBeginSingleTimeCommands();
    
    VkImageAspectFlags AspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    if (NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
        AspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        if (Format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            Format == VK_FORMAT_D24_UNORM_S8_UINT) {
            AspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }
    }
    
    VkImageMemoryBarrier Barrier = {};
    Barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    Barrier.oldLayout                       = OldLayout;
    Barrier.newLayout                       = NewLayout;
    Barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    Barrier.image                           = Image;
    Barrier.subresourceRange.aspectMask     = AspectMask;
    Barrier.subresourceRange.baseMipLevel   = 0;
    Barrier.subresourceRange.levelCount     = MipLevelCount;
    Barrier.subresourceRange.baseArrayLayer = 0;
    Barrier.subresourceRange.layerCount     = 1;
    Barrier.srcAccessMask                   = 0;
    Barrier.dstAccessMask                   = 0;
    
    VkPipelineStageFlags SrcStage;
    VkPipelineStageFlags DstStage;
    
    if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED && NewLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    {
        Barrier.srcAccessMask = 0;
        Barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        DstStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    }
    else if (OldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && NewLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL )
    {
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        SrcStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        DstStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    }
    else if (OldLayout == VK_IMAGE_LAYOUT_UNDEFINED && NewLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    {
        Barrier.srcAccessMask = 0;
        Barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        SrcStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        DstStage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    }
    else
    {
        ExitWithError("Unsupported layout transition");
    }
    
    vkCmdPipelineBarrier(CommandBuffer,
                         SrcStage, DstStage,
                         0,
                         0, NULL,
                         0, NULL,
                         1, &Barrier
                         );
    
    VulkanEndSingleTimeCommands(CommandBuffer);
}

internal void VulkanGenerateMipmaps(VkImage Image, VkFormat ImageFormat, u32 Width, u32 Height, u32 MipCount)
{
    VkFormatProperties FormatProperties;
    vkGetPhysicalDeviceFormatProperties(VulkanPhysicalDevice, ImageFormat, &FormatProperties);
    if (!(FormatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
        ExitWithError("Texture image format does not support linear blitting");
    }
    
    VkCommandBuffer CommandBuffer = VulkanBeginSingleTimeCommands();
    
    VkImageMemoryBarrier Barrier = {};
    Barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    Barrier.image                           = Image;
    Barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    Barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
    Barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
    Barrier.subresourceRange.baseArrayLayer = 0;
    Barrier.subresourceRange.layerCount     = 1;
    Barrier.subresourceRange.levelCount     = 1;
    
    for (u32 i = 1; i < MipCount; ++i)
    {
        Barrier.subresourceRange.baseMipLevel = i - 1;
        Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        Barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        Barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        
        vkCmdPipelineBarrier(CommandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
                             0, NULL,
                             0, NULL,
                             1, &Barrier);
        
        VkImageBlit Blit = {};
        Blit.srcOffsets[0]                 = VulkanOffset3D( 0, 0, 0 );
        Blit.srcOffsets[1]                 = VulkanOffset3D( Width, Height, 1 );
        Blit.srcSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        Blit.srcSubresource.mipLevel       = i - 1;
        Blit.srcSubresource.baseArrayLayer = 0;
        Blit.srcSubresource.layerCount     = 1;
        Blit.dstOffsets[0]                 = VulkanOffset3D( 0, 0, 0 );
        Blit.dstOffsets[1]                 = VulkanOffset3D( Width>1 ? Width/2 : 1, Height>1 ? Height/2 : 1, 1 );
        Blit.dstSubresource.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
        Blit.dstSubresource.mipLevel       = i;
        Blit.dstSubresource.baseArrayLayer = 0;
        Blit.dstSubresource.layerCount     = 1;
        
        vkCmdBlitImage(CommandBuffer, Image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, Image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &Blit, VK_FILTER_LINEAR);
        
        Barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        Barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        Barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        Barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
        vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                             0, NULL,
                             0, NULL,
                             1, &Barrier);
        
        if (Width > 1) Width = Width/2;
        if (Height > 1) Height = Height/2;
    }
    
    Barrier.subresourceRange.baseMipLevel = MipCount - 1;
    Barrier.oldLayout                     = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    Barrier.newLayout                     = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    Barrier.srcAccessMask                 = VK_ACCESS_TRANSFER_WRITE_BIT;
    Barrier.dstAccessMask                 = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(CommandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
                         0, NULL,
                         0, NULL,
                         1, &Barrier);
    
    VulkanEndSingleTimeCommands(CommandBuffer);
}

internal VkShaderModule VulkanCreateShaderModule(VkDevice Device, const u8* Bytes, u32 ByteCount)
{
    VkShaderModuleCreateInfo ShaderModuleCreateInfo = {};
    ShaderModuleCreateInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    ShaderModuleCreateInfo.codeSize = ByteCount;
    ShaderModuleCreateInfo.pCode    = (const uint32_t*)Bytes;
    
    VkShaderModule ShaderModule;
    if (vkCreateShaderModule(Device, &ShaderModuleCreateInfo, NULL, &ShaderModule) != VK_SUCCESS) {
        ExitWithError("Failed to create shader module");
    }
    
    return ShaderModule;
}

internal vulkan_create_image_result VulkanCreateImage(u32 Width, u32 Height, u32 MipLevelCount, VkSampleCountFlagBits SampleCount, VkFormat Format, VkImageTiling Tiling, VkImageUsageFlags UsageFlags, VkMemoryPropertyFlags MemoryFlags)
{
    // create image
    VkImageCreateInfo ImageCreateInfo = {};
    ImageCreateInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    ImageCreateInfo.imageType     = VK_IMAGE_TYPE_2D;
    ImageCreateInfo.extent.width  = Width;
    ImageCreateInfo.extent.height = Height;
    ImageCreateInfo.extent.depth  = 1;
    ImageCreateInfo.mipLevels     = MipLevelCount;
    ImageCreateInfo.arrayLayers   = 1;
    ImageCreateInfo.format        = Format;
    ImageCreateInfo.tiling        = Tiling; // implementation defined
    ImageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ImageCreateInfo.usage         = UsageFlags;
    ImageCreateInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
    ImageCreateInfo.samples       = SampleCount; // only for multisampled attachments
    ImageCreateInfo.flags         = 0;
    
    vulkan_create_image_result Res;
    if (vkCreateImage(VulkanDevice, &ImageCreateInfo, NULL, &Res.Image) != VK_SUCCESS) {
        ExitWithError("Failed to create image");
    }
    
    // create image memory
    VkMemoryRequirements MemRequirements;
    vkGetImageMemoryRequirements(VulkanDevice, Res.Image, &MemRequirements);
    
    VkMemoryAllocateInfo AllocInfo = {};
    AllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    AllocInfo.allocationSize = MemRequirements.size;
    AllocInfo.memoryTypeIndex = VulkanFindMemoryType(MemRequirements.memoryTypeBits, MemoryFlags);
    
    if (vkAllocateMemory(VulkanDevice, &AllocInfo, NULL, &Res.Memory) != VK_SUCCESS) {
        ExitWithError("Failed to allocate image memory");
    }
    
    vkBindImageMemory(VulkanDevice, Res.Image, Res.Memory, 0);
    
    return Res;
}

internal VkImageView VulkanCreateImageView(VkImage Image, VkFormat Format, VkImageAspectFlags AspectFlags, u32 MipLevelCount)
{
    VkImageViewCreateInfo ImageViewCreateInfo = {};
    ImageViewCreateInfo.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    ImageViewCreateInfo.image        = Image;
    ImageViewCreateInfo.viewType     = VK_IMAGE_VIEW_TYPE_2D;
    ImageViewCreateInfo.format       = Format;
    ImageViewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    ImageViewCreateInfo.subresourceRange.aspectMask     = AspectFlags;
    ImageViewCreateInfo.subresourceRange.baseMipLevel   = 0;
    ImageViewCreateInfo.subresourceRange.levelCount     = MipLevelCount;
    ImageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    ImageViewCreateInfo.subresourceRange.layerCount     = 1;
    
    VkImageView ImageView;
    if (vkCreateImageView(VulkanDevice, &ImageViewCreateInfo, NULL, &ImageView) != VK_SUCCESS) {
        ExitWithError("Failed to create texture image view");
    }
    
    return ImageView;
}

internal void VulkanCreateSwapchain()
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
            i32 Width  = DEFAULT_WINDOW_WIDTH;
            i32 Height = DEFAULT_WINDOW_HEIGHT;
            RECT WindowRect = {};
            if (GetClientRect(App.Window, &WindowRect)) {
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
            ExitWithError("Failed to create the swap chain");
        }
        
        VulkanSwapchainImageCount = ImageCount;
        Res = vkGetSwapchainImagesKHR(VulkanDevice, VulkanSwapchain, &VulkanSwapchainImageCount, VulkanSwapchainImages);
        Assert(Res == VK_SUCCESS);
        
        // create chapchain image views
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i)
        {
            VulkanSwapchainImageViews[i] = VulkanCreateImageView(VulkanSwapchainImages[i], VulkanSwapchainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }
    }
    
    // Vulkan: Render pass
    {
        // attachment description
        
        VkAttachmentDescription ColorAttachment = {};
        ColorAttachment.format         = VulkanSwapchainImageFormat;
        ColorAttachment.samples        = VulkanMSAASampleCount;
        ColorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        ColorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        ColorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ColorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        ColorAttachment.finalLayout    = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkAttachmentReference ColorAttachmentRef = {};
        ColorAttachmentRef.attachment = 0; // index in the pAttachments render pass global array
        ColorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkAttachmentDescription DepthAttachment = {};
        DepthAttachment.format         = VulkanDepthFormat;
        DepthAttachment.samples        = VulkanMSAASampleCount;
        DepthAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
        DepthAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_DONT_CARE; // because we won't use it after render
        DepthAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        DepthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        DepthAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        DepthAttachment.finalLayout    = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        VkAttachmentReference DepthAttachmentRef = {};
        DepthAttachmentRef.attachment = 1; // index in the pAttachments render pass global array
        DepthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        VkAttachmentDescription ColorAttachmentResolve = {};
        ColorAttachmentResolve.format         = VulkanSwapchainImageFormat;
        ColorAttachmentResolve.samples        = VK_SAMPLE_COUNT_1_BIT;
        ColorAttachmentResolve.loadOp         = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachmentResolve.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
        ColorAttachmentResolve.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        ColorAttachmentResolve.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        ColorAttachmentResolve.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
        ColorAttachmentResolve.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        
        VkAttachmentReference ColorAttachmentResolveRef = {};
        ColorAttachmentResolveRef.attachment = 2; // index in the pAttachments render pass global array
        ColorAttachmentResolveRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkSubpassDescription Subpass    = {};
        Subpass.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
        Subpass.colorAttachmentCount    = 1;
        Subpass.pColorAttachments       = &ColorAttachmentRef; // position in this array is the attachment location in the shader
        Subpass.pResolveAttachments     = &ColorAttachmentResolveRef;
        Subpass.pDepthStencilAttachment = &DepthAttachmentRef;
        
        // NOTE(jdiaz): Didn't understand these flags very well...
        VkSubpassDependency SubpassDependency = {};
        SubpassDependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
        SubpassDependency.dstSubpass    = 0;
        SubpassDependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        SubpassDependency.srcAccessMask = 0;
        SubpassDependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        SubpassDependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        
        VkAttachmentDescription Attachments[] = {ColorAttachment, DepthAttachment, ColorAttachmentResolve};
        
        VkRenderPassCreateInfo RenderPassCreateInfo = {};
        RenderPassCreateInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        RenderPassCreateInfo.attachmentCount = ArrayCount(Attachments);
        RenderPassCreateInfo.pAttachments    = Attachments;
        RenderPassCreateInfo.subpassCount    = 1;
        RenderPassCreateInfo.pSubpasses      = &Subpass;
        RenderPassCreateInfo.dependencyCount = 1;
        RenderPassCreateInfo.pDependencies   = &SubpassDependency;
        
        if (vkCreateRenderPass(VulkanDevice, &RenderPassCreateInfo, NULL, &VulkanRenderPass) != VK_SUCCESS) {
            ExitWithError("Render pass could not be created");
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
        
        VkVertexInputAttributeDescription VertexAttributesDescription[3] = {};
        VertexAttributesDescription[0].binding  = 0;
        VertexAttributesDescription[0].location = 0;
        VertexAttributesDescription[0].format   = VK_FORMAT_R32G32B32_SFLOAT;
        VertexAttributesDescription[0].offset   = OffsetOf(vertex, pos);
        VertexAttributesDescription[1].binding  = 0;
        VertexAttributesDescription[1].location = 1;
        VertexAttributesDescription[1].format   = VK_FORMAT_R32G32B32_SFLOAT;
        VertexAttributesDescription[1].offset   = OffsetOf(vertex, color);
        VertexAttributesDescription[2].binding  = 0;
        VertexAttributesDescription[2].location = 2;
        VertexAttributesDescription[2].format   = VK_FORMAT_R32G32_SFLOAT;
        VertexAttributesDescription[2].offset   = OffsetOf(vertex, texCoord);
        
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
        Viewport.width  = (f32)VulkanSwapchainExtent.width;
        Viewport.height = (f32)VulkanSwapchainExtent.height;
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
        RasterizerCreateInfo.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        RasterizerCreateInfo.depthBiasEnable         = VK_FALSE;
        RasterizerCreateInfo.depthBiasConstantFactor = 0.0f;
        RasterizerCreateInfo.depthBiasClamp          = 0.0f;
        RasterizerCreateInfo.depthBiasSlopeFactor    = 0.0f;
        
        // Multisampling
        
        VkPipelineMultisampleStateCreateInfo MultisampleCreateInfo = {};
        MultisampleCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        MultisampleCreateInfo.sampleShadingEnable   = VK_FALSE; // VK_TRUE also applies MSAA to shading (if supported)
        MultisampleCreateInfo.rasterizationSamples  = VulkanMSAASampleCount;
        MultisampleCreateInfo.minSampleShading      = 1.0f;     // If sampleShadingEnabled, close to 1.0 means smoother
        MultisampleCreateInfo.pSampleMask           = NULL;
        MultisampleCreateInfo.alphaToCoverageEnable = VK_FALSE;
        MultisampleCreateInfo.alphaToOneEnable      = VK_FALSE;
        
        // Depth / stencil
        
        VkPipelineDepthStencilStateCreateInfo DepthStencilCreateInfo = {};
        DepthStencilCreateInfo.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        DepthStencilCreateInfo.depthTestEnable       = VK_TRUE;
        DepthStencilCreateInfo.depthWriteEnable      = VK_TRUE;
        DepthStencilCreateInfo.depthCompareOp        = VK_COMPARE_OP_LESS;
        DepthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;
        DepthStencilCreateInfo.minDepthBounds        = 0.0f;
        DepthStencilCreateInfo.maxDepthBounds        = 1.0f;
        DepthStencilCreateInfo.stencilTestEnable     = VK_FALSE;
        //DepthStencilCreateInfo.front;
        //DepthStencilCreateInfo.back;
        
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
        PipelineLayoutCreateInfo.setLayoutCount         = 1;
        PipelineLayoutCreateInfo.pSetLayouts            = &VulkanDescriptorSetLayout;
        PipelineLayoutCreateInfo.pushConstantRangeCount = 0;
        PipelineLayoutCreateInfo.pPushConstantRanges    = NULL;
        
        if (vkCreatePipelineLayout(VulkanDevice, &PipelineLayoutCreateInfo, NULL, &VulkanPipelineLayout) != VK_SUCCESS) {
            ExitWithError("Could not create the pipeline layout");
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
        GraphicsPipelineCreateInfo.pDepthStencilState  = &DepthStencilCreateInfo;
        GraphicsPipelineCreateInfo.pColorBlendState    = &ColorBlendingCreateInfo;
        GraphicsPipelineCreateInfo.pDynamicState       = NULL;
        
        GraphicsPipelineCreateInfo.layout              = VulkanPipelineLayout;
        
        GraphicsPipelineCreateInfo.renderPass          = VulkanRenderPass;
        GraphicsPipelineCreateInfo.subpass             = 0; // graphics subpass index
        
        GraphicsPipelineCreateInfo.basePipelineHandle  = VK_NULL_HANDLE;
        GraphicsPipelineCreateInfo.basePipelineIndex   = -1;
        
        if (vkCreateGraphicsPipelines(VulkanDevice, VK_NULL_HANDLE, 1, &GraphicsPipelineCreateInfo, NULL, &VulkanGraphicsPipeline) != VK_SUCCESS) {
            ExitWithError("Graphics pipeline could not be created");
        }
        
        // Free stuff
        
        vkDestroyShaderModule(VulkanDevice, FragmentShaderModule, NULL);
        vkDestroyShaderModule(VulkanDevice, VertexShaderModule, NULL);
    }
    
    // Vulkan: Color buffer
    {
        VkFormat ColorFormat = VulkanSwapchainImageFormat;
        
        vulkan_create_image_result Color = VulkanCreateImage(VulkanSwapchainExtent.width,
                                                             VulkanSwapchainExtent.height,
                                                             1,
                                                             VulkanMSAASampleCount,
                                                             ColorFormat,
                                                             VK_IMAGE_TILING_OPTIMAL,
                                                             VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
                                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanColorImage       = Color.Image;
        VulkanColorImageMemory = Color.Memory;
        VulkanColorImageView   = VulkanCreateImageView(VulkanColorImage, ColorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }
    
    // Vulkan: Depth buffer
    {
        vulkan_create_image_result Depth = VulkanCreateImage(VulkanSwapchainExtent.width,
                                                             VulkanSwapchainExtent.height,
                                                             1,
                                                             VulkanMSAASampleCount,
                                                             VulkanDepthFormat,
                                                             VK_IMAGE_TILING_OPTIMAL,
                                                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                                                             VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanDepthImage       = Depth.Image;
        VulkanDepthImageMemory = Depth.Memory;
        VulkanDepthImageView   = VulkanCreateImageView(VulkanDepthImage, VulkanDepthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    }
    
    // Vulkan: Framebuffers for the swapchain
    {
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i)
        {
            VkImageView Attachments[] = { VulkanColorImageView, VulkanDepthImageView, VulkanSwapchainImageViews[i] };
            
            VkFramebufferCreateInfo FramebufferCreateInfo = {};
            FramebufferCreateInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            FramebufferCreateInfo.renderPass      = VulkanRenderPass;
            FramebufferCreateInfo.attachmentCount = ArrayCount(Attachments);
            FramebufferCreateInfo.pAttachments    = Attachments;
            FramebufferCreateInfo.width           = VulkanSwapchainExtent.width;
            FramebufferCreateInfo.height          = VulkanSwapchainExtent.height;
            FramebufferCreateInfo.layers          = 1;
            
            if (vkCreateFramebuffer(VulkanDevice, &FramebufferCreateInfo, NULL, &VulkanSwapchainFramebuffers[i]) != VK_SUCCESS) {
                ExitWithError("Swapchain framebuffer could not be created");
            }
        }
    }
    
    // Vulkan: Uniform buffers
    {
        VkDeviceSize BufferSize = sizeof(uniform_buffer_object);
        
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i)
        {
            vulkan_create_buffer_result Uniform = 
                VulkanCreateBuffer(BufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            VulkanUniformBuffers[i] = Uniform.Buffer;
            VulkanUniformBuffersMemory[i] = Uniform.Memory;
        }
    }
    
    // Vulkan: Descriptor pool / Descriptor set
    {
        // descriptor pool
        VkDescriptorPoolSize DescriptorPoolSize[2] = {};
        DescriptorPoolSize[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        DescriptorPoolSize[0].descriptorCount = VulkanSwapchainImageCount;
        DescriptorPoolSize[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        DescriptorPoolSize[1].descriptorCount = VulkanSwapchainImageCount;
        
        VkDescriptorPoolCreateInfo DescriptorPoolCreateInfo = {};
        DescriptorPoolCreateInfo.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        DescriptorPoolCreateInfo.poolSizeCount = ArrayCount(DescriptorPoolSize);
        DescriptorPoolCreateInfo.pPoolSizes    = DescriptorPoolSize;
        DescriptorPoolCreateInfo.maxSets       = VulkanSwapchainImageCount;
        
        if (vkCreateDescriptorPool(VulkanDevice, &DescriptorPoolCreateInfo, NULL, &VulkanDescriptorPool) != VK_SUCCESS)
        {
            ExitWithError("Failed to create descriptor pool");
        }
        
        // descriptor sets
        VkDescriptorSetLayout DescriptorSetLayouts[MAX_SWAPCHAIN_IMAGES];
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i) {
            DescriptorSetLayouts[i] = VulkanDescriptorSetLayout;
        }
        
        VkDescriptorSetAllocateInfo DescriptorSetAllocInfo = {};
        DescriptorSetAllocInfo.sType              = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        DescriptorSetAllocInfo.descriptorPool     = VulkanDescriptorPool;
        DescriptorSetAllocInfo.descriptorSetCount = VulkanSwapchainImageCount;
        DescriptorSetAllocInfo.pSetLayouts        = DescriptorSetLayouts;
        
        if (vkAllocateDescriptorSets(VulkanDevice, &DescriptorSetAllocInfo, VulkanDescriptorSets) != VK_SUCCESS) {
            ExitWithError("Failed to allocate descriptor sets");
        }
        
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i)
        {
            VkDescriptorBufferInfo BufferInfo = {};
            BufferInfo.buffer = VulkanUniformBuffers[i];
            BufferInfo.offset = 0;
            BufferInfo.range  = sizeof(uniform_buffer_object);
            
            VkDescriptorImageInfo ImageInfo = {};
            ImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            ImageInfo.imageView   = VulkanTextureImageView;
            ImageInfo.sampler     = VulkanTextureSampler;
            
            VkWriteDescriptorSet DescriptorWrite[2] = {};
            
            DescriptorWrite[0].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            DescriptorWrite[0].dstSet          = VulkanDescriptorSets[i];
            DescriptorWrite[0].dstBinding      = 0;
            DescriptorWrite[0].dstArrayElement = 0;
            DescriptorWrite[0].descriptorType  = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            DescriptorWrite[0].descriptorCount = 1;
            DescriptorWrite[0].pBufferInfo     = &BufferInfo;
            DescriptorWrite[0].pImageInfo      = NULL;
            DescriptorWrite[0].pTexelBufferView= NULL;
            
            DescriptorWrite[1].sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            DescriptorWrite[1].dstSet          = VulkanDescriptorSets[i];
            DescriptorWrite[1].dstBinding      = 1;
            DescriptorWrite[1].dstArrayElement = 0;
            DescriptorWrite[1].descriptorType  = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            DescriptorWrite[1].descriptorCount = 1;
            DescriptorWrite[1].pBufferInfo     = NULL;
            DescriptorWrite[1].pImageInfo      = &ImageInfo;
            DescriptorWrite[1].pTexelBufferView= NULL;
            
            vkUpdateDescriptorSets(VulkanDevice, ArrayCount(DescriptorWrite), DescriptorWrite, 0, NULL);
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
            ExitWithError("Command buffers could not be created");
        }
        
        // start command buffer recording
        for (u32 i = 0; i < VulkanSwapchainImageCount; ++i)
        {
            VkCommandBufferBeginInfo BeginInfo = {};
            BeginInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            BeginInfo.flags            = 0;
            BeginInfo.pInheritanceInfo = NULL; // for secondary only
            
            if (vkBeginCommandBuffer(VulkanCommandBuffers[i], &BeginInfo) != VK_SUCCESS) {
                ExitWithError("Failed to begin recording command buffer");
            }
            
            VkClearValue ClearValues[] = {{0.0f, 0.0f, 0.0f, 1.0f}, {1.0f, 0}};
            
            // start render pass
            VkRenderPassBeginInfo RenderPassInfo = {};
            RenderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            RenderPassInfo.renderPass        = VulkanRenderPass;
            RenderPassInfo.framebuffer       = VulkanSwapchainFramebuffers[i];
            //RenderPassInfo.renderArea.offset  = {0, 0};
            RenderPassInfo.renderArea.extent = VulkanSwapchainExtent;
            RenderPassInfo.clearValueCount   = ArrayCount(ClearValues);
            RenderPassInfo.pClearValues      = ClearValues;
            
            vkCmdBeginRenderPass(VulkanCommandBuffers[i], &RenderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            
            // bind pipeline
            vkCmdBindPipeline(VulkanCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanGraphicsPipeline);
            
            // vertex buffer binding
            VkBuffer VertexBuffers[] = {VulkanVertexBuffer};
            VkDeviceSize Offsets[] = {0};
            vkCmdBindVertexBuffers(VulkanCommandBuffers[i], 0, ArrayCount(VertexBuffers), VertexBuffers, Offsets);
            
            vkCmdBindIndexBuffer(VulkanCommandBuffers[i], VulkanIndexBuffer, 0, VK_INDEX_TYPE_UINT16);
            
            // bind descriptor sets
            vkCmdBindDescriptorSets(VulkanCommandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, VulkanPipelineLayout, 0, 1, &VulkanDescriptorSets[i], 0, NULL);
            
            // draw
            vkCmdDrawIndexed(VulkanCommandBuffers[i], ArrayCount(Indices), 1, 0, 0, 0);
            
            vkCmdEndRenderPass(VulkanCommandBuffers[i]);
            
            if (vkEndCommandBuffer(VulkanCommandBuffers[i]) != VK_SUCCESS) {
                ExitWithError("Failed to record command buffer");
            }
        }
    }
}

internal void VulkanCleanupSwapchain()
{
    vkDestroyImageView(VulkanDevice, VulkanDepthImageView, NULL);
    vkDestroyImage(VulkanDevice, VulkanDepthImage, NULL);
    vkFreeMemory(VulkanDevice, VulkanDepthImageMemory, NULL);
    
    vkDestroyImageView(VulkanDevice, VulkanColorImageView, NULL);
    vkDestroyImage(VulkanDevice, VulkanColorImage, NULL);
    vkFreeMemory(VulkanDevice, VulkanColorImageMemory, NULL);
    
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
    
    for (u32 i = 0; i < Count; ++i) {
        vkDestroyBuffer(VulkanDevice, VulkanUniformBuffers[i], NULL);
        vkFreeMemory(VulkanDevice, VulkanUniformBuffersMemory[i], NULL);
    }
    
    //vkFreeDescriptorSets(VulkanDevice, VulkanDescriptorPool, Count, VulkanDescriptorSets);
    
    vkDestroyDescriptorPool(VulkanDevice, VulkanDescriptorPool, NULL);
}

internal void VulkanInit(arena& Arena, app_data *Data, i32 Width, i32 Height)
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
                ExitWithError("Could not find the validation layer");
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
            ExitWithError("Failed to create Vulkan instance");
        }
    }
    
    // Vulkan: Setup debug messenger
    
    // TODO(jdiaz)
    
    // Vulkan: Window surface
    {
        VkWin32SurfaceCreateInfoKHR SurfaceCreateInfo = {};
        SurfaceCreateInfo.sType     = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
        SurfaceCreateInfo.hwnd      = App.Window;
        SurfaceCreateInfo.hinstance = App.Instance;
        
        if (vkCreateWin32SurfaceKHR(VulkanInstance, &SurfaceCreateInfo, NULL, &VulkanSurface) != VK_SUCCESS) {
            ExitWithError("Failed to create a Vulkan surface");
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
            VkPhysicalDevice CurrPhysicalDevice = PhysicalDevices[ i ];
            
            // Check queues support
            
            uint32_t PhysicalDeviceQueueFamilyCount = 8;
            VkQueueFamilyProperties PhysicalDeviceQueueFamilies[ 8 ];
            vkGetPhysicalDeviceQueueFamilyProperties(CurrPhysicalDevice, &PhysicalDeviceQueueFamilyCount, PhysicalDeviceQueueFamilies);
            Assert(PhysicalDeviceQueueFamilyCount < 8); // 8 is more than enough, this "less than" is not an error
            
            u32 GraphicsQueueIdx = INVALID_INDEX;
            u32 PresentQueueIdx  = INVALID_INDEX;
            for (u32 j = 0; j < PhysicalDeviceQueueFamilyCount; ++j)
            {
                if (PhysicalDeviceQueueFamilies[ j ].queueFlags & VK_QUEUE_GRAPHICS_BIT)
                    GraphicsQueueIdx = j;
                
                VkBool32 PresentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(CurrPhysicalDevice, j, VulkanSurface, &PresentSupport);
                if (PresentSupport)
                    PresentQueueIdx = j;
            }
            
            if (GraphicsQueueIdx == INVALID_INDEX || PresentQueueIdx == INVALID_INDEX)
                continue;
            
            
            // Check extensions support
            
            uint32_t DeviceExtensionsCount = 256;
            VkExtensionProperties DeviceExtensions[ 256 ];
            vkEnumerateDeviceExtensionProperties(CurrPhysicalDevice, NULL, &DeviceExtensionsCount, NULL);
            Res = vkEnumerateDeviceExtensionProperties(CurrPhysicalDevice, NULL, &DeviceExtensionsCount, DeviceExtensions);
            Assert(Res == VK_SUCCESS);
            
            b32 AllRequiredExtensionsFound = true;
            for (u32 req_idx = 0; req_idx < ArrayCount(RequiredDeviceExtensions); ++req_idx)
            {
                b32 RequiredExtensionFound = false;
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
            vkGetPhysicalDeviceSurfaceFormatsKHR(CurrPhysicalDevice, VulkanSurface, &FormatCount, NULL);
            
            uint32_t PresentModesCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(CurrPhysicalDevice, VulkanSurface, &PresentModesCount, NULL);
            
            b32 IsValidSwapChain = FormatCount != 0 && PresentModesCount != 0;
            if (!IsValidSwapChain)
                continue;
            
            // Check depth buffer availability
            
            VkImageTiling        Tiling         = VK_IMAGE_TILING_OPTIMAL; // or LINEAR
            VkFormatFeatureFlags Features       = VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
            VkFormat             DepthFormats[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
            
            b32 AvailableDepthFormatFound = false;
            VkFormat DepthFormat;
            for (u32 j = 0; j < ArrayCount(DepthFormats); ++j)
            {
                VkFormatProperties Props;
                vkGetPhysicalDeviceFormatProperties(CurrPhysicalDevice, DepthFormats[j], &Props);
                
                if (Tiling == VK_IMAGE_TILING_LINEAR && (Props.linearTilingFeatures & Features) == Features)
                {
                    DepthFormat = DepthFormats[j];
                    AvailableDepthFormatFound = true;
                }
                else if (Tiling == VK_IMAGE_TILING_OPTIMAL && (Props.optimalTilingFeatures & Features) == Features)
                {
                    DepthFormat = DepthFormats[j];
                    AvailableDepthFormatFound = true;
                }
            }
            
            if (!AvailableDepthFormatFound)
                continue;
            
            b32 DepthHasStencil = (DepthFormat == VK_FORMAT_D24_UNORM_S8_UINT || DepthFormat == VK_FORMAT_D32_SFLOAT_S8_UINT);
            
            
            
            // Check device properties
            
            u32 Score = 0;
            
            VkPhysicalDeviceProperties DeviceProperties;
            vkGetPhysicalDeviceProperties(CurrPhysicalDevice, &DeviceProperties);
            
            if (DeviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU)
                Score += 1000;
            
            Score += DeviceProperties.limits.maxImageDimension2D;
            
            VkSampleCountFlagBits SampleCountFlag;
            VkSampleCountFlags    SampleCountFlags = DeviceProperties.limits.framebufferColorSampleCounts & DeviceProperties.limits.framebufferDepthSampleCounts;
            if      (SampleCountFlags & VK_SAMPLE_COUNT_64_BIT) SampleCountFlag = VK_SAMPLE_COUNT_64_BIT;
            else if (SampleCountFlags & VK_SAMPLE_COUNT_32_BIT) SampleCountFlag = VK_SAMPLE_COUNT_32_BIT;
            else if (SampleCountFlags & VK_SAMPLE_COUNT_16_BIT) SampleCountFlag = VK_SAMPLE_COUNT_16_BIT;
            else if (SampleCountFlags & VK_SAMPLE_COUNT_8_BIT)  SampleCountFlag = VK_SAMPLE_COUNT_8_BIT;
            else if (SampleCountFlags & VK_SAMPLE_COUNT_4_BIT)  SampleCountFlag = VK_SAMPLE_COUNT_4_BIT;
            else if (SampleCountFlags & VK_SAMPLE_COUNT_2_BIT)  SampleCountFlag = VK_SAMPLE_COUNT_2_BIT;
            else                                                SampleCountFlag = VK_SAMPLE_COUNT_1_BIT;
            
            
            // Check device features
            
            VkPhysicalDeviceFeatures DeviceFeatures;
            vkGetPhysicalDeviceFeatures(CurrPhysicalDevice, &DeviceFeatures);
            
            if (!DeviceFeatures.geometryShader)    Score = 0;
            if (!DeviceFeatures.samplerAnisotropy) Score = 0;
            
            
            // Get the most suitable device found so far
            if (Score > BestScore)
            {
                BestScore = Score;
                VulkanPhysicalDevice      = CurrPhysicalDevice;
                VulkanGraphicsQueueFamily = GraphicsQueueIdx;
                VulkanPresentQueueFamily  = PresentQueueIdx;
                VulkanMSAASampleCount     = SampleCountFlag;
                VulkanDepthFormat         = DepthFormat;
                VulkanDepthHasStencil     = DepthHasStencil;
            }
        }
        
        if (BestScore == 0) {
            ExitWithError("Failed to find a proper Vulkan physical device");
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
        DeviceFeatures.samplerAnisotropy = VK_TRUE;
        //DeviceFeatures.sampleRateShading = VK_TRUE; // MSAA also applied to shading (more costly)
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
            ExitWithError("Failed to create Vulkan logical device");
        }
        
        vkGetDeviceQueue(VulkanDevice, VulkanGraphicsQueueFamily, 0, &VulkanGraphicsQueue);
        
        vkGetDeviceQueue(VulkanDevice, VulkanPresentQueueFamily,  0, &VulkanPresentQueue);
    }
    
    // Vulkan: Command pool
    {
        // command pool
        VkCommandPoolCreateInfo CmdPoolCreateInfo = {};
        CmdPoolCreateInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        CmdPoolCreateInfo.queueFamilyIndex = VulkanGraphicsQueueFamily;
        CmdPoolCreateInfo.flags            = 0; // flags to indicate the frequency of change of commands
        
        if (vkCreateCommandPool(VulkanDevice, &CmdPoolCreateInfo, NULL, &VulkanCommandPool) != VK_SUCCESS) {
            ExitWithError("Command pool could not be created");
        }
    }
    
    // Vulkan: Texture image
    {
        // read image
        int TexWidth, TexHeight, TexChannels;
        stbi_uc* Pixels = stbi_load("texture.jpg", &TexWidth, &TexHeight, &TexChannels, STBI_rgb_alpha);
        if (!Pixels) {
            ExitWithError("Failed to load texture.jpg");
        }
        
        VulkanMipLevels = (uint32_t)Floor(Log2(Max(TexWidth, TexHeight))) + 1u;
        
        // staging buffer
        VkDeviceSize ImageSize = TexWidth * TexHeight * 4;
        vulkan_create_buffer_result Staging = 
            VulkanCreateBuffer(ImageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        void* Data;
        vkMapMemory(VulkanDevice, Staging.Memory, 0, ImageSize, 0, &Data);
        memcpy(Data, Pixels, ImageSize);
        vkUnmapMemory(VulkanDevice, Staging.Memory);
        
        stbi_image_free(Pixels);
        
        vulkan_create_image_result Res = VulkanCreateImage(TexWidth, TexHeight, VulkanMipLevels,
                                                           VK_SAMPLE_COUNT_1_BIT,
                                                           VK_FORMAT_R8G8B8A8_SRGB,
                                                           VK_IMAGE_TILING_OPTIMAL,
                                                           VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                                           VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanTextureImage = Res.Image;
        VulkanTextureImageMemory = Res.Memory;
        
        VulkanTransitionImageLayout(VulkanTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VulkanMipLevels);
        VulkanCopyBufferToImage(Staging.Buffer, VulkanTextureImage, TexWidth, TexHeight);
        
        // OLD: Transitions all mip levels in the image to read_only_optimal
        //VulkanTransitionImageLayout(VulkanTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VulkanMipLevels);
        
        // NEW: Generate all mipmap levels one by one, making layout transitions more granular
        VulkanGenerateMipmaps(VulkanTextureImage, VK_FORMAT_R8G8B8A8_SRGB,  TexWidth, TexHeight, VulkanMipLevels);
        
        vkDestroyBuffer(VulkanDevice, Staging.Buffer, NULL);
        vkFreeMemory(VulkanDevice, Staging.Memory, NULL);
    }
    
    // Vulkan: Texture image view
    {
        VulkanTextureImageView = VulkanCreateImageView(VulkanTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, VulkanMipLevels);
    }
    
    // Vulkan: Texture sampler
    {
        VkSamplerCreateInfo SamplerCreateInfo = {};
        SamplerCreateInfo.sType                   = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        SamplerCreateInfo.magFilter               = VK_FILTER_LINEAR;
        SamplerCreateInfo.minFilter               = VK_FILTER_LINEAR;
        SamplerCreateInfo.addressModeU            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerCreateInfo.addressModeV            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerCreateInfo.addressModeW            = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        SamplerCreateInfo.anisotropyEnable        = VK_TRUE;
        SamplerCreateInfo.maxAnisotropy           = 16.0f;
        SamplerCreateInfo.borderColor             = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        SamplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
        SamplerCreateInfo.compareEnable           = VK_FALSE;             // PCF for shadow maps
        SamplerCreateInfo.compareOp               = VK_COMPARE_OP_ALWAYS; // PCF for shadow maps
        SamplerCreateInfo.mipmapMode              = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        SamplerCreateInfo.mipLodBias              = 0.0f;
        SamplerCreateInfo.minLod                  = 0.0f;
        SamplerCreateInfo.maxLod                  = (f32)VulkanMipLevels;
        
        if (vkCreateSampler(VulkanDevice, &SamplerCreateInfo, NULL, &VulkanTextureSampler) != VK_SUCCESS) {
            ExitWithError("Failed to create texture sampler");
        }
    }
    
    // Vulkan: Vertex buffer
    {
        VkDeviceSize BufferSize = sizeof(Vertices);
        
        // temporary staging buffer
        vulkan_create_buffer_result Staging =
            VulkanCreateBuffer(BufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        // copy vertices into memory
        void *Data;
        vkMapMemory(VulkanDevice, Staging.Memory, 0, BufferSize, 0, &Data);
        memcpy(Data, Vertices, BufferSize);
        vkUnmapMemory(VulkanDevice, Staging.Memory);
        
        vulkan_create_buffer_result Vertex =
            VulkanCreateBuffer(BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanVertexBuffer       = Vertex.Buffer;
        VulkanVertexBufferMemory = Vertex.Memory;
        
        VulkanCopyBuffer(Staging.Buffer, Vertex.Buffer, BufferSize);
        
        vkDestroyBuffer(VulkanDevice, Staging.Buffer, NULL);
        vkFreeMemory(VulkanDevice, Staging.Memory, NULL);
    }
    
    // Vulkan: Index buffer
    {
        VkDeviceSize BufferSize = sizeof(Indices);
        
        // temporary staging buffer
        vulkan_create_buffer_result Staging =
            VulkanCreateBuffer(BufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        // copy indices into memory
        void *Data;
        vkMapMemory(VulkanDevice, Staging.Memory, 0, BufferSize, 0, &Data);
        memcpy(Data, Indices, BufferSize);
        vkUnmapMemory(VulkanDevice, Staging.Memory);
        
        vulkan_create_buffer_result Index =
            VulkanCreateBuffer(BufferSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        VulkanIndexBuffer       = Index.Buffer;
        VulkanIndexBufferMemory = Index.Memory;
        
        VulkanCopyBuffer(Staging.Buffer, Index.Buffer, BufferSize);
        
        vkDestroyBuffer(VulkanDevice, Staging.Buffer, NULL);
        vkFreeMemory(VulkanDevice, Staging.Memory, NULL);
    }
    
    // Vulkan: Descriptor set layout
    {
        VkDescriptorSetLayoutBinding UboLayoutBinding = {};
        UboLayoutBinding.binding            = 0;
        UboLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        UboLayoutBinding.descriptorCount    = 1; // more than 1 if it is an array
        UboLayoutBinding.stageFlags         = VK_SHADER_STAGE_VERTEX_BIT;
        UboLayoutBinding.pImmutableSamplers = NULL;
        
        VkDescriptorSetLayoutBinding SamplerLayoutBinding = {};
        SamplerLayoutBinding.binding            = 1;
        SamplerLayoutBinding.descriptorCount    = 1; // more than 1 if it is an array
        SamplerLayoutBinding.descriptorType     = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        SamplerLayoutBinding.pImmutableSamplers = NULL;
        SamplerLayoutBinding.stageFlags         = VK_SHADER_STAGE_FRAGMENT_BIT;
        
        VkDescriptorSetLayoutBinding Bindings[] = {UboLayoutBinding, SamplerLayoutBinding};
        
        VkDescriptorSetLayoutCreateInfo DescriptorSetLayoutCreateInfo = {};
        DescriptorSetLayoutCreateInfo.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        DescriptorSetLayoutCreateInfo.bindingCount = ArrayCount(Bindings);
        DescriptorSetLayoutCreateInfo.pBindings    = Bindings;
        
        if (vkCreateDescriptorSetLayout(VulkanDevice, &DescriptorSetLayoutCreateInfo, NULL, &VulkanDescriptorSetLayout) != VK_SUCCESS) {
            ExitWithError("Failed to create descriptor set layout");
        }
    }
    
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
                ExitWithError("Failed to create semaphores");
            }
        }
        
        for (u32 i = 0; i < MAX_SWAPCHAIN_IMAGES; ++i)
        {
            VulkanInFlightImages[i] = VK_NULL_HANDLE;
        }
    }
    
    VulkanCreateSwapchain();
}

internal void VulkanCleanup()
{
    // wait the logical device to finish operations
    vkDeviceWaitIdle(VulkanDevice);
    
    VulkanCleanupSwapchain();
    
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(VulkanDevice, VulkanRenderFinishedSemaphore[i], NULL);
        vkDestroySemaphore(VulkanDevice, VulkanImageAvailableSemaphore[i], NULL);
        vkDestroyFence(VulkanDevice, VulkanInFlightFences[i], NULL);
    }
    
    vkDestroySampler(VulkanDevice, VulkanTextureSampler, NULL);
    vkDestroyImageView(VulkanDevice, VulkanTextureImageView, NULL);
    vkDestroyImage(VulkanDevice, VulkanTextureImage, NULL);
    vkFreeMemory(VulkanDevice, VulkanTextureImageMemory, NULL);
    
    vkDestroyDescriptorSetLayout(VulkanDevice, VulkanDescriptorSetLayout, NULL);
    
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
        App.Resize = true;
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
            const b32 ShiftKeyIsDown = wParam & MK_SHIFT;
            const b32 CtrlKeyIsDown  = wParam & MK_CONTROL;
            const b32 LButtonIsDown  = wParam & MK_LBUTTON;
            const b32 RButtonIsDown  = wParam & MK_RBUTTON;
            const b32 MButtonIsDown  = wParam & MK_MBUTTON;
            const i32  MouseX        = LO_WORD(lParam);
            const i32  MouseY        = HI_WORD(lParam);
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
        App.Running = false;
        
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
    
    HWND Window  = CreateWindowExA(WS_EX_APPWINDOW,
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
    
    App.Resize   = false;
    App.Running  = true;
    App.Window   = Window;
    App.Instance = hInstance;
    
    if ( Window )
    {
        // System info
        SYSTEM_INFO SystemInfo;
        GetSystemInfo(&SystemInfo);
        DWORD PageSize              = SystemInfo.dwPageSize;
        DWORD AllocationGranularity = SystemInfo.dwAllocationGranularity; // much larger
        
        // Memory initialization
        
        u32 MemorySize   = MB(512);
        u8* MemoryBuffer = (u8*)VirtualAlloc(NULL, MemorySize, MEM_RESERVE|MEM_COMMIT, PAGE_READWRITE);
        Assert(MemoryBuffer);
        arena Arena      = MakeArena(MemoryBuffer, MemorySize);
        
        App.ScratchMemory.Size = GB(2);
        App.ScratchMemory.Head = 0;
        App.ScratchMemory.Buffer = (u8*)VirtualAlloc(NULL, App.ScratchMemory.Size, MEM_RESERVE, PAGE_READWRITE);
        Assert(App.ScratchMemory.Buffer);
        
        app_data* Data = PushStruct(&Arena, app_data);
        
        VulkanInit(Arena, Data, DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT);
        
        // Application loop
        u32 CurrentFrame = 0;
        
        MSG Msg = {};
        while ( App.Running )
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
                    VulkanCreateSwapchain();
                }
                else if (Res != VK_SUCCESS && Res != VK_SUBOPTIMAL_KHR)
                {
                    ExitWithError("Failed to acquire swapchain image");
                }
                
                // this is just in case the image returned by vkAquireNextImage is not consecutive
                if (VulkanInFlightImages[ImageIndex] != VK_NULL_HANDLE)
                {
                    vkWaitForFences(VulkanDevice, 1, &VulkanInFlightImages[ImageIndex], VK_TRUE, UINT64_MAX);
                }
                VulkanInFlightImages[ImageIndex] = VulkanInFlightFences[CurrentFrame];
                
                vkResetFences(VulkanDevice, 1, &VulkanInFlightFences[CurrentFrame]);
                
                // update uniform buffer
                local_persist f32 Angle = 0.0f;
                Angle += 0.01f;
                uniform_buffer_object UBO = {};
                UBO.model = Rotation(Radians(Angle), V3(0.0, 0.0, 1.0));
                UBO.view  = LookAt(V3(2.0, 2.0, 2.0), V3(0.0, 0.0, 0.0), V3(0.0, 1.0, 0.0));
                UBO.proj  = Perspective(Radians(45.0f), VulkanSwapchainExtent.width / (f32)VulkanSwapchainExtent.height, 0.1f, 10.0f);
                UBO.proj.data[1][1] *= -1.0f;
                
                void *UBOData;
                vkMapMemory(VulkanDevice, VulkanUniformBuffersMemory[ImageIndex], 0, sizeof(uniform_buffer_object), 0, &UBOData);
                memcpy(UBOData, &UBO, sizeof(uniform_buffer_object));
                vkUnmapMemory(VulkanDevice, VulkanUniformBuffersMemory[ImageIndex]);
                
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
                    ExitWithError("Failed to submit draw command buffer");
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
                
                if (Res == VK_ERROR_OUT_OF_DATE_KHR || Res == VK_SUBOPTIMAL_KHR || App.Resize)
                {
                    App.Resize = false;
                    vkDeviceWaitIdle(VulkanDevice);
                    VulkanCleanupSwapchain();
                    VulkanCreateSwapchain();
                }
                else if (Res != VK_SUCCESS)
                {
                    ExitWithError("Failed to present swapchain image");
                }
                
                CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
            }
            
            // Reset scratch memory
            App.ScratchMemory.Head = 0;
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
