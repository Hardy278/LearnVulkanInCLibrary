#pragma once
#include <vulkan/vulkan.h>

#include <optional>
#include <string>
#include <vector>

struct SDL_Window;

namespace engine::render {

#pragma region Constants
const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};   // 验证层扩展
const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME}; // 设备扩展

#ifdef NDEBUG
const bool ENABLE_VALIDATION_LAYER = false;
#else
const bool ENABLE_VALIDATION_LAYER = true;
#endif
#pragma endregion

/**
 * @struct QueueFamilyIndices
 * @brief 存储队列族索引的容器
 *
 * 该结构体用于存储物理设备的队列族索引，包括图形队列和显示队列。
 * 通过isComplete()函数可以检查队列族是否完整。
 */
struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() && presentFamily.has_value();
    }
};

/**
 * @struct SwapChainSupportDetails
 * @brief 存储交换链支持信息的容器
 */
struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;
};

class VulkanRenderer final {
public:
    VulkanRenderer(SDL_Window *window);
    ~VulkanRenderer();

    VulkanRenderer(const VulkanRenderer &)            = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;
    VulkanRenderer(VulkanRenderer &&)                 = delete;
    VulkanRenderer &operator=(VulkanRenderer &&)      = delete;

    void initVulkan(); // 初始化Vulkan
    void render();     // 更新渲染
    void cleanup();    // 清理Vulkan

    void setFramebufferResized(bool resized) { m_framebufferResized = resized; }

private:
#pragma region Menber Variables
    bool m_initialized = false; // 是否初始化
    SDL_Window *m_window;       // SDL windows 窗口句柄

    VkInstance m_instance;                     // Vulkan 实例句柄
    VkDebugUtilsMessengerEXT m_debugMessenger; // Debug 消息句柄
    VkSurfaceKHR m_surface;                    // Vulkan 窗口句柄

    VkPhysicalDevice m_physicalDevice = VK_NULL_HANDLE; // 物理设备句柄
    VkDevice m_device;                                  // 逻辑设备句柄

    VkQueue m_graphicsQueue; // 图形队列句柄
    VkQueue m_presentQueue;  // 显示队列句柄

    VkSwapchainKHR m_swapChain;                         // 交换链句柄
    std::vector<VkImage> m_swapChainImages;             // 交换链图像句柄
    VkFormat m_swapChainImageFormat;                    // 交换链图像格式
    VkExtent2D m_swapChainExtent;                       // 交换链图像尺寸
    std::vector<VkImageView> m_swapChainImageViews;     // 交换链图像视图句柄
    std::vector<VkFramebuffer> m_swapChainFramebuffers; // 交换链帧缓冲区句柄

    VkRenderPass m_renderPass;         // 渲染通道句柄
    VkPipelineLayout m_pipelineLayout; // 管道布局
    VkPipeline m_graphicsPipeline;     // 渲染管道

    VkCommandPool m_commandPool;                   // 命令池
    std::vector<VkCommandBuffer> m_commandBuffers; // 命令缓冲区

    std::vector<VkSemaphore> m_imageAvailableSemaphores; // 图像可用信号量
    std::vector<VkSemaphore> m_renderFinishedSemaphores; // 渲染完成信号量
    std::vector<VkFence> m_inFlightFences;               // 在飞行中的帧缓冲区
    uint32_t m_currentFrame = 0;                         // 当前帧

    bool m_framebufferResized = false; // 是否调整了窗口大小
#pragma endregion

#pragma region Instance and Validation Layers and Surface
    void createInstance();
    static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData);
    VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
        const VkAllocationCallbacks *pAllocator,
        VkDebugUtilsMessengerEXT *pDebugMessenger);
    void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks *pAllocator);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
    void setupDebugMessenger();
    std::vector<const char *> getRequiredExtensions();
    bool checkValidationLayerSupport();
    void createSurface();
#pragma endregion

#pragma region Devices and Queues
    void pickPhysicalDevice();
    QueueFamilyIndices findQueueFamilies(const VkPhysicalDevice &device);
    bool isDeviceSuitable(const VkPhysicalDevice &device);
    bool checkDeviceExtensionSupport(const VkPhysicalDevice &device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    void printPhysicalDeviceProperties(VkPhysicalDevice &device);
    void createLogicalDevice();
#pragma endregion

#pragma region Swap Chain and Image Views
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
    VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);
    void createSwapChain();
    void createImageViews();
#pragma endregion

#pragma region Shader Modules and Pipelines
    static std::vector<char> readFile(const std::string &filename);
    VkShaderModule createShaderModule(const std::vector<char> &code);
    void createGraphicsPipeline();
#pragma endregion

#pragma region Render Pass and Framebuffers
    void createRenderPass();
    void createFramebuffers();
#pragma endregion

#pragma region Command Buffers and Synchronization
    void createCommandPool();
    void createCommandBuffers();
    void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void createSyncObjects();
#pragma endregion

#pragma region Render and Recreate Swap Chain
    void drawFrame();
    void cleanupSwapChain();
    void recreateSwapChain();
#pragma endregion
};
} // namespace engine::render