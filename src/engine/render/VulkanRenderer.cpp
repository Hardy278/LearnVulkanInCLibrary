#include "VulkanRenderer.hpp"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <set>
#include <stdexcept>
#include <vector>

namespace engine::render {

VulkanRenderer::VulkanRenderer(SDL_Window *window) : m_window(window) {
    initVulkan();
}

VulkanRenderer::~VulkanRenderer() = default;

void VulkanRenderer::initVulkan() {
    createInstance();         //  创建 Vulkan 实例
    setupDebugMessenger();    //  设置调试消息
    createSurface();          //  创建 Vulkan 表面
    pickPhysicalDevice();     //  选择物理设备
    createLogicalDevice();    //  创建逻辑设备
    createSwapChain();        //  创建交换链
    createImageViews();       //  创建交换链图像视图
    createRenderPass();       //  创建渲染通道
    createGraphicsPipeline(); //  创建图形管线
    createFramebuffers();     //  创建帧缓冲区
    createCommandPool();      //  创建命令池
    createCommandBuffers();   //  创建命令缓冲区
    createSyncObjects();      //  创建同步对象
    m_initialized = true;     //  设置初始化标志
}

void VulkanRenderer::render() {
    drawFrame(); //  绘制一帧
}

void VulkanRenderer::cleanup() {
    vkDeviceWaitIdle(m_device); //  等待设备空闲
    if (m_initialized) {
        cleanupSwapChain();
        vkDestroyPipeline(m_device, m_graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
        vkDestroyRenderPass(m_device, m_renderPass, nullptr);
        for (size_t i = 0; i < m_commandBuffers.size(); i++) {
            vkDestroySemaphore(m_device, m_renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(m_device, m_imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(m_device, m_inFlightFences[i], nullptr);
        }
        vkDestroyCommandPool(m_device, m_commandPool, nullptr);
        vkDestroyDevice(m_device, nullptr);
        if (ENABLE_VALIDATION_LAYER) {
            DestroyDebugUtilsMessengerEXT(m_instance, m_debugMessenger, nullptr);
        }
        vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
        vkDestroyInstance(m_instance, nullptr);
        m_initialized = false; //  设置初始化标志
        spdlog::trace("VulkanRenderer::cleanup()::Vulkan已销毁");
    } else {
        spdlog::warn("VulkanRenderer::cleanup()::Vulkan未初始化或者已销毁");
    }
}

#pragma region Instance and Validation Layers and Surface
void VulkanRenderer::createInstance() {
    if (ENABLE_VALIDATION_LAYER && !checkValidationLayerSupport()) {
        throw std::runtime_error("VulkanRenderer::createInstance()::验证层不支持");
    }

    VkApplicationInfo appInfo{};
    appInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO; //  指定结构体类型
    appInfo.pApplicationName   = "Hello Triangle";                   //  设置应用程序名称
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);           //  设置应用程序版本 为1.0.0
    appInfo.pEngineName        = "No Engine";                        //  引擎名称设置为"No Engine"
    appInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);           //  引擎版本设置为1.0.0
    appInfo.apiVersion         = VK_API_VERSION_1_0;                 //  Vulkan API版本设置为1.0

    VkInstanceCreateInfo createInfo{};
    createInfo.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR; //  设置可移植性枚举标志（防止 macOS 上的问题）
    createInfo.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; //  指定结构体类型
    createInfo.pApplicationInfo = &appInfo;                               //  设置应用程序信息

    auto requiredExtensions            = getRequiredExtensions();                          //  获取所需的Vulkan扩展
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(requiredExtensions.size()); //  设置所需的Vulkan扩展数量
    createInfo.ppEnabledExtensionNames = requiredExtensions.data();                        //  设置所需的Vulkan扩展

    //  指定验证层，如果启用验证层，则添加验证层
    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if constexpr (ENABLE_VALIDATION_LAYER) {
        populateDebugMessengerCreateInfo(debugCreateInfo);
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size());         //  设置验证层数量
        createInfo.ppEnabledLayerNames = validationLayers.data();                                //  设置验证层
        createInfo.pNext               = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo; //  设置验证层信息
        spdlog::info("VulkanRenderer::createInstance()::启用验证层");
    } else {
        createInfo.enabledLayerCount = 0;       //  设置验证层数量为0
        createInfo.pNext             = nullptr; //  不启用验证层
        spdlog::info("VulkanRenderer::createInstance()::未启用验证层");
    }

    // 检查 Vulkan 支持的扩展，并输出
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> extensions(extensionCount);
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());
    spdlog::info("VulkanRenderer::createInstance()::Vulkan 支持的扩展数量: {}, 扩展列表:", extensionCount);
    for (const auto &extension : extensions) {
        spdlog::info("  {}", std::string_view(extension.extensionName));
    }

    if (vkCreateInstance(&createInfo, nullptr, &m_instance) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createInstance()::创建Vulkan实例失败");
    }
}

VKAPI_ATTR VkBool32 VKAPI_CALL VulkanRenderer::debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT /*messageType*/,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
    void * /*pUserData*/) {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        spdlog::error("VulkanRenderer::debugCallback()::验证层报告错误: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        spdlog::warn("VulkanRenderer::debugCallback()::验证层报告警告: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        spdlog::info("VulkanRenderer::debugCallback()::验证层报告信息: {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        spdlog::debug("VulkanRenderer::debugCallback()::验证层报告调试信息: {}", pCallbackData->pMessage);
    } else {
        spdlog::error("VulkanRenderer::debugCallback()::验证层报告未知信息: {}", pCallbackData->pMessage);
    }
    return false;
}

VkResult VulkanRenderer::CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
    } else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

void VulkanRenderer::DestroyDebugUtilsMessengerEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debugMessenger,
    const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        func(instance, debugMessenger, pAllocator);
    }
}

void VulkanRenderer::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
    createInfo                 = {}; // 添加这行以确保结构体零初始化
    createInfo.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                 VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                             VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    createInfo.pfnUserCallback = debugCallback;
    createInfo.pUserData       = nullptr; // Optional
}

void VulkanRenderer::setupDebugMessenger() {
    if (!ENABLE_VALIDATION_LAYER) return;
    VkDebugUtilsMessengerCreateInfoEXT createInfo;
    populateDebugMessengerCreateInfo(createInfo); // 填充调试消息传递器创建信息
    // 创建调试消息传递器
    if (CreateDebugUtilsMessengerEXT(m_instance, &createInfo, nullptr, &m_debugMessenger) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::setupDebugMessenger()::创建调试消息传递器失败");
    }
}

std::vector<const char *> VulkanRenderer::getRequiredExtensions() {
    uint32_t SDLExtensionCount          = 0;
    const char *const *SDLExtensionsPtr = SDL_Vulkan_GetInstanceExtensions(&SDLExtensionCount);
    std::vector<const char *> extensions(SDLExtensionsPtr, SDLExtensionsPtr + SDLExtensionCount);

    if (ENABLE_VALIDATION_LAYER) {
        extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    }
    extensions.emplace_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME); //  添加可移植性枚举扩展
    return extensions;
}

bool VulkanRenderer::checkValidationLayerSupport() {
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr); // 获取可用的验证层数量
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data()); // 获取可用的验证层

    for (const char *layerName : validationLayers) {
        bool layerFound = false;
        for (const auto &layerProperties : availableLayers) { // 遍历可用的验证层
            if (strcmp(layerName, layerProperties.layerName) == 0) {
                layerFound = true;
                break;
            }
        }
        if (!layerFound) return false;
    }
    return true;
}

void VulkanRenderer::createSurface() {
    if (!SDL_Vulkan_CreateSurface(m_window, m_instance, nullptr, &m_surface)) {
        throw std::runtime_error("VulkanRenderer::createSurface()::创建窗口表面失败");
    }
}
#pragma endregion

#pragma region Devices and Queues
void VulkanRenderer::pickPhysicalDevice() {
    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
        throw std::runtime_error("VulkanRenderer::pickPhysicalDevice()::没有找到支持 Vulkan 的物理设备");
    }
    std::vector<VkPhysicalDevice> devices(deviceCount);
    vkEnumeratePhysicalDevices(m_instance, &deviceCount, devices.data());

    for (const auto &device : devices) {
        if (isDeviceSuitable(device)) {
            m_physicalDevice = device; // 在这里我们只选择第一个合适的设备，你可以根据需要选择其他设备
            break;
        }
    }
    if (m_physicalDevice == VK_NULL_HANDLE) {
        throw std::runtime_error("VulkanRenderer::pickPhysicalDevice()::没有找到合适的物理设备");
    }
    printPhysicalDeviceProperties(m_physicalDevice);
}
QueueFamilyIndices VulkanRenderer::findQueueFamilies(const VkPhysicalDevice &device) {
    QueueFamilyIndices indices;
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;
    for (const auto &queueFamily : queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }
        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, m_surface, &presentSupport);
        if (presentSupport) {
            indices.presentFamily = i;
        }
        if (indices.isComplete()) break;
        i++;
    }
    return indices;
}
bool VulkanRenderer::isDeviceSuitable(const VkPhysicalDevice &device) {
    QueueFamilyIndices indices = findQueueFamilies(device);           // 查找队列家族
    bool extensionsSupported   = checkDeviceExtensionSupport(device); // 检查设备扩展支持
    bool swapChainAdequate     = false;
    if (extensionsSupported) {
        SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);

        swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty(); // 检查交换链支持是否足够,格式和呈现模式
    }
    return indices.isComplete() && extensionsSupported && swapChainAdequate; // 检查设备是否适合,队列家族、设备扩展和交换链支持
}
bool VulkanRenderer::checkDeviceExtensionSupport(const VkPhysicalDevice &device) {
    uint32_t extensionCount;
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);
    std::vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());
    std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
    for (const auto &extension : availableExtensions) {
        requiredExtensions.erase(extension.extensionName);
    }
    return requiredExtensions.empty();
}
SwapChainSupportDetails VulkanRenderer::querySwapChainSupport(VkPhysicalDevice device) {
    SwapChainSupportDetails details;                                                     // 获取交换链支持的能力，格式和呈现模式
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, m_surface, &details.capabilities); // 获取交换链支持的能力
    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, nullptr); // 获取支持的格式数量
    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, m_surface, &formatCount, details.formats.data()); // 获取支持的格式
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, nullptr);
    if (presentModeCount != 0) {
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, m_surface, &presentModeCount, details.presentModes.data());
    }
    return details;
}
void VulkanRenderer::printPhysicalDeviceProperties(VkPhysicalDevice &device) {
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    // 设备类型判断
    const char *deviceType;
    switch (properties.deviceType) {
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: deviceType = "集成显卡"; break;
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: deviceType = "独立显卡"; break;
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: deviceType = "虚拟GPU"; break;
    case VK_PHYSICAL_DEVICE_TYPE_CPU: deviceType = "CPU"; break;
    default: deviceType = "其他"; break;
    }
    // Vulkan API版本格式化
    uint32_t apiVersion    = properties.apiVersion;
    uint32_t driverVersion = properties.driverVersion;

    spdlog::info("VulkanRenderer::pickPhysicalDevice()::选择了物理设备:");
    spdlog::info("  设备名称: {}", properties.deviceName);
    spdlog::info("  设备类型: {}", deviceType);
    spdlog::info("  Vulkan API版本: {}.{}.{}", VK_VERSION_MAJOR(apiVersion), VK_VERSION_MINOR(apiVersion), VK_VERSION_PATCH(apiVersion));
    spdlog::info("  驱动版本: {}.{}.{}", VK_VERSION_MAJOR(driverVersion), VK_VERSION_MINOR(driverVersion), VK_VERSION_PATCH(driverVersion));
}
void VulkanRenderer::createLogicalDevice() {
    QueueFamilyIndices indices = findQueueFamilies(m_physicalDevice);

    std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
    std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(), indices.presentFamily.value()}; // 将图形队列和呈现队列的索引添加到集合中
    float queuePriority                    = 1.0f;
    for (uint32_t queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount       = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures{}; // 获取设备特性，这里我们使用默认特性

    VkDeviceCreateInfo createInfo{};
    createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
    createInfo.pQueueCreateInfos       = queueCreateInfos.data();
    createInfo.pEnabledFeatures        = &deviceFeatures;
    createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size()); // 设置启用的设备扩展数量
    createInfo.ppEnabledExtensionNames = deviceExtensions.data();                        // 设置启用的设备扩展名称
    if (ENABLE_VALIDATION_LAYER) {
        createInfo.enabledLayerCount   = static_cast<uint32_t>(validationLayers.size()); // 设置启用的验证层数量
        createInfo.ppEnabledLayerNames = validationLayers.data();                        // 设置启用的验证层名称
    } else {
        createInfo.enabledLayerCount = 0; // 如果不启用验证层，则设置为0
    }

    if (vkCreateDevice(m_physicalDevice, &createInfo, nullptr, &m_device) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createLogicalDevice()::无法创建逻辑设备");
    }

    vkGetDeviceQueue(m_device, indices.graphicsFamily.value(), 0, &m_graphicsQueue);
    vkGetDeviceQueue(m_device, indices.presentFamily.value(), 0, &m_presentQueue);
    spdlog::trace("VulkanRenderer::createLogicalDevice()::逻辑设备创建成功");
}
#pragma endregion

#pragma region Swap Chain and Image Views
VkSurfaceFormatKHR VulkanRenderer::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
    for (const auto &availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }
    return availableFormats[0];
}
VkPresentModeKHR VulkanRenderer::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
    for (const auto &availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return availablePresentMode; // 如果支持 mailbox 模式，则优先使用
        }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
}
VkExtent2D VulkanRenderer::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
    if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
    } else {
        int width, height;
        SDL_GetWindowSizeInPixels(m_window, &width, &height);
        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)};
        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        return actualExtent;
    }
}
void VulkanRenderer::createSwapChain() {
    SwapChainSupportDetails swapChainSupport = querySwapChainSupport(m_physicalDevice);
    VkSurfaceFormatKHR surfaceFormat         = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode             = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent                        = chooseSwapExtent(swapChainSupport.capabilities);
    uint32_t imageCount                      = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount; // 如果最大图像数量不为0，则将图像数量设置为最大图像数量
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType            = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface          = m_surface;                           // 设置交换链的表面
    createInfo.minImageCount    = imageCount;                          // 设置交换链的图像数量
    createInfo.imageFormat      = surfaceFormat.format;                // 设置交换链的图像格式
    createInfo.imageColorSpace  = surfaceFormat.colorSpace;            // 设置交换链的图像颜色空间
    createInfo.imageExtent      = extent;                              // 设置交换链的图像尺寸
    createInfo.imageArrayLayers = 1;                                   // 设置交换链的图像层数
    createInfo.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // 设置交换链的图像用途

    QueueFamilyIndices indices    = findQueueFamilies(m_physicalDevice);                             // 查找队列家族索引
    uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()}; // 设置队列家族索引

    if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode      = VK_SHARING_MODE_CONCURRENT; // 如果图形队列和呈现队列不同，则使用并发模式
        createInfo.queueFamilyIndexCount = 2;                          // 设置队列家族索引数量
        createInfo.pQueueFamilyIndices   = queueFamilyIndices;         // 设置队列家族索引
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // 如果图形队列和呈现队列相同，则使用独占模式
    }
    createInfo.preTransform   = swapChainSupport.capabilities.currentTransform; // 设置交换链的图像变换
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;              // 设置交换链的图像透明度
    createInfo.presentMode    = presentMode;                                    // 设置交换链的呈现模式
    createInfo.clipped        = VK_TRUE;                                        // 设置交换链的剪裁模式
    createInfo.oldSwapchain   = VK_NULL_HANDLE;                                 // 设置旧的交换链

    if (vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &m_swapChain) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createSwapChain()::创建交换链失败");
    }

    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, nullptr);                  // 获取交换链图像数量
    m_swapChainImages.resize(imageCount);                                                  // 创建交换链图像
    vkGetSwapchainImagesKHR(m_device, m_swapChain, &imageCount, m_swapChainImages.data()); // 获取交换链图像
    spdlog::trace("VulkanRenderer::createSwapChain()::创建交换链成功, 交换链图像数量: {}", imageCount);

    m_swapChainImageFormat = surfaceFormat.format; // 设置交换链图像格式
    m_swapChainExtent      = extent;               // 设置交换链图像尺寸
}
void VulkanRenderer::createImageViews() {
    m_swapChainImageViews.resize(m_swapChainImages.size());
    for (size_t i = 0; i < m_swapChainImages.size(); i++) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image                           = m_swapChainImages[i];          // 设置图像
        createInfo.viewType                        = VK_IMAGE_VIEW_TYPE_2D;         // 设置图像视图类型
        createInfo.format                          = m_swapChainImageFormat;        // 设置图像格式
        createInfo.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY; // 设置颜色分量 r
        createInfo.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY; // 设置颜色分量 g
        createInfo.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY; // 设置颜色分量 b
        createInfo.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY; // 设置颜色分量 a
        createInfo.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;     // 设置图像视图的方面掩码
        createInfo.subresourceRange.baseMipLevel   = 0;                             // 设置图像视图的基础Mipmap级别
        createInfo.subresourceRange.levelCount     = 1;                             // 设置图像视图的Mipmap级别数量
        createInfo.subresourceRange.baseArrayLayer = 0;                             // 设置图像视图的基础数组层
        createInfo.subresourceRange.layerCount     = 1;                             // 设置图像视图的数组层数
        if (vkCreateImageView(m_device, &createInfo, nullptr, &m_swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("VulkanRenderer::createImageViews()::创建交换链图像视图失败");
        }
    }
    spdlog::trace("VulkanRenderer::createImageViews()::创建交换链图像视图成功, 交换链图像视图数量: {}", m_swapChainImageViews.size());
}
#pragma endregion

#pragma region Shader Modules and Pipelines
std::vector<char> VulkanRenderer::readFile(const std::string &filename) {
    std::ifstream file(filename, std::ios::ate | std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("VulkanRenderer::readFile()::打开文件失败, 文件名: " + filename);
    }
    size_t fileSize = (size_t)file.tellg();
    std::vector<char> buffer(fileSize);
    file.seekg(0);
    file.read(buffer.data(), fileSize);
    file.close();
    spdlog::info("VulkanRenderer::readFile()::读取文件成功, 文件名: {}, 文件大小: {} bytes", filename, fileSize);
    return buffer;
}
VkShaderModule VulkanRenderer::createShaderModule(const std::vector<char> &code) {
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();                                     // 设置着色器代码大小
    createInfo.pCode    = reinterpret_cast<const uint32_t *>(code.data()); // 设置着色器代码
    VkShaderModule shaderModule;
    if (vkCreateShaderModule(m_device, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createShaderModule()::创建着色器模块失败");
    }
    return shaderModule;
}
void VulkanRenderer::createGraphicsPipeline() {
    auto vertShaderCode             = readFile("assets/shaders/graphics.vert.spv");
    auto fragShaderCode             = readFile("assets/shaders/graphics.frag.spv");
    VkShaderModule vertShaderModule = createShaderModule(vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
    vertShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vertShaderStageInfo.stage  = VK_SHADER_STAGE_VERTEX_BIT; // 设置着色器阶段，这里设置为顶点着色器阶段
    vertShaderStageInfo.module = vertShaderModule;           // 设置着色器模块，这里设置为顶点着色器模块
    vertShaderStageInfo.pName  = "main";                     // 设置着色器入口函数名

    VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
    fragShaderStageInfo.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    fragShaderStageInfo.stage  = VK_SHADER_STAGE_FRAGMENT_BIT; // 设置着色器阶段，这里设置为片段着色器阶段
    fragShaderStageInfo.module = fragShaderModule;             // 设置着色器模块，这里设置为片段着色器模块
    fragShaderStageInfo.pName  = "main";                       // 设置着色器入口函数名

    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount   = 0;       // 设置顶点绑定描述数量，这里设置为0，因为我们没有顶点绑定描述
    vertexInputInfo.pVertexBindingDescriptions      = nullptr; // 设置顶点绑定描述，这里设置为nullptr，因为我们没有顶点绑定描述
    vertexInputInfo.vertexAttributeDescriptionCount = 0;       // 设置顶点属性描述数量，这里设置为0，因为我们没有顶点属性描述
    vertexInputInfo.pVertexAttributeDescriptions    = nullptr; // 设置顶点属性描述，这里设置为nullptr，因为我们没有顶点属性描述

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST; // 设置图元拓扑，这里设置为三角形列表
    inputAssembly.primitiveRestartEnable = VK_FALSE;                            // 设置是否启用图元重启，这里设置为不启用

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1; // 设置视口数量，这里设置为1，因为我们只有一个视口
    viewportState.scissorCount  = 1; // 设置剪裁矩形数量，这里设置为1，因为我们只有一个剪裁矩形

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.depthClampEnable        = VK_FALSE;                // 设置是否启用深度钳制
    rasterizer.rasterizerDiscardEnable = VK_FALSE;                // 设置是否启用光栅化丢弃
    rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;    // 设置多边形模式，这里设置为填充模式
    rasterizer.lineWidth               = 1.0f;                    // 设置线宽
    rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;   // 设置剔除模式
    rasterizer.frontFace               = VK_FRONT_FACE_CLOCKWISE; // 设置正面朝向
    rasterizer.depthBiasEnable         = VK_FALSE;                // 设置是否启用深度偏移，这里设置为不启用

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType                = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable  = VK_FALSE;              // 设置是否启用样本着色
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT; // 设置样本数量，这里设置为1

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT; // 设置颜色写入掩码
    colorBlendAttachment.blendEnable    = VK_FALSE;                                                                                                  // 设置是否启用混合

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable     = VK_FALSE;              // 设置是否启用逻辑操作
    colorBlending.logicOp           = VK_LOGIC_OP_COPY;      // 设置逻辑操作，这里设置为复制操作
    colorBlending.attachmentCount   = 1;                     // 设置颜色混合附件数量
    colorBlending.pAttachments      = &colorBlendAttachment; // 设置颜色混合附件
    colorBlending.blendConstants[0] = 0.0f;                  // 设置混合常数
    colorBlending.blendConstants[1] = 0.0f;                  // 设置混合常数
    colorBlending.blendConstants[2] = 0.0f;                  // 设置混合常数
    colorBlending.blendConstants[3] = 0.0f;                  // 设置混合常数

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT, // 设置动态视口
        VK_DYNAMIC_STATE_SCISSOR}; // 设置动态剪裁矩形
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size()); // 设置动态状态数量
    dynamicState.pDynamicStates    = dynamicStates.data();                        // 设置动态状态

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount         = 0; // 设置布局数量
    pipelineLayoutInfo.pushConstantRangeCount = 0; // 设置推送常量范围数量

    if (vkCreatePipelineLayout(m_device, &pipelineLayoutInfo, nullptr, &m_pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createShaderModule()::创建管线布局失败");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType               = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount          = 2;                // 设置着色器阶段数量
    pipelineInfo.pStages             = shaderStages;     // 设置着色器阶段
    pipelineInfo.pVertexInputState   = &vertexInputInfo; // 设置顶点输入状态
    pipelineInfo.pInputAssemblyState = &inputAssembly;   // 设置图元组装状态
    pipelineInfo.pViewportState      = &viewportState;   // 设置视口状态
    pipelineInfo.pRasterizationState = &rasterizer;      // 设置光栅化状态
    pipelineInfo.pMultisampleState   = &multisampling;   // 设置多重采样状态
    pipelineInfo.pColorBlendState    = &colorBlending;   // 设置颜色混合状态
    pipelineInfo.pDynamicState       = &dynamicState;    // 设置动态状态
    pipelineInfo.layout              = m_pipelineLayout; // 设置管线布局
    pipelineInfo.renderPass          = m_renderPass;     // 设置渲染通道
    pipelineInfo.subpass             = 0;                // 设置子通道
    pipelineInfo.basePipelineHandle  = VK_NULL_HANDLE;   // 设置基础管线句柄

    if (vkCreateGraphicsPipelines(m_device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &m_graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createGraphicsPipeline()::创建图形管线失败");
    }

    vkDestroyShaderModule(m_device, fragShaderModule, nullptr);
    vkDestroyShaderModule(m_device, vertShaderModule, nullptr);
    spdlog::trace("VulkanRenderer::createGraphicsPipeline()::创建图形管线成功");
}
#pragma endregion

#pragma region Render Pass and Framebuffers
void VulkanRenderer::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format         = m_swapChainImageFormat;           // 设置颜色附件格式，这里设置为交换链图像格式
    colorAttachment.samples        = VK_SAMPLE_COUNT_1_BIT;            // 设置颜色附件样本数，这里设置为1
    colorAttachment.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;      // 设置颜色附件加载操作，这里设置为清除操作
    colorAttachment.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;     // 设置颜色附件存储操作，这里设置为存储操作
    colorAttachment.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;  // 设置模板附件加载操作，这里设置为不关心
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // 设置模板附件存储操作，这里设置为不关心
    colorAttachment.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;        // 设置初始布局，这里设置为未定义
    colorAttachment.finalLayout    = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;  // 设置最终布局，这里设置为交换链图像源布局

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;                                        // 设置颜色附件引用索引
    colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; // 设置颜色附件引用布局，这里设置为颜色附件优化布局

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint    = VK_PIPELINE_BIND_POINT_GRAPHICS; // 设置管线绑定点，这里设置为图形管线
    subpass.colorAttachmentCount = 1;                               // 设置颜色附件数量
    subpass.pColorAttachments    = &colorAttachmentRef;             // 设置颜色附件引用

    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;                // 设置附件数量
    renderPassInfo.pAttachments    = &colorAttachment; // 设置附件
    renderPassInfo.subpassCount    = 1;                // 设置子通道数量
    renderPassInfo.pSubpasses      = &subpass;         // 设置子通道

    if (vkCreateRenderPass(m_device, &renderPassInfo, nullptr, &m_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createRenderPass()::创建渲染通道失败");
    }
    spdlog::trace("VulkanRenderer::createRenderPass()::创建渲染通道成功");
}

void VulkanRenderer::createFramebuffers() {
    m_swapChainFramebuffers.resize(m_swapChainImageViews.size());
    for (size_t i = 0; i < m_swapChainImageViews.size(); i++) {
        VkImageView attachments[] = {m_swapChainImageViews[i]};
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass      = m_renderPass;             // 设置渲染通道
        framebufferInfo.attachmentCount = 1;                        // 设置附件数量
        framebufferInfo.pAttachments    = attachments;              // 设置附件
        framebufferInfo.width           = m_swapChainExtent.width;  // 设置帧缓冲宽度
        framebufferInfo.height          = m_swapChainExtent.height; // 设置帧缓冲高度
        framebufferInfo.layers          = 1;                        // 设置帧缓冲层数量
        if (vkCreateFramebuffer(m_device, &framebufferInfo, nullptr, &m_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("VulkanRenderer::createFramebuffers()::创建帧缓冲失败");
        }
    }
    spdlog::trace("VulkanRenderer::createFramebuffers()::创建帧缓冲成功，数量：{}", m_swapChainFramebuffers.size());
}
#pragma endregion

#pragma region Command Buffers and Synchronization
void VulkanRenderer::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(m_physicalDevice);
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; // 设置命令池标志，这里设置为重置命令缓冲标志
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();       // 设置命令池队列族索引，这里设置为图形队列族索引
    if (vkCreateCommandPool(m_device, &poolInfo, nullptr, &m_commandPool) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createCommandPool()::创建命令池失败");
    }
    spdlog::trace("VulkanRenderer::createCommandPool()::创建命令池成功");
}
void VulkanRenderer::createCommandBuffers() {
    m_commandBuffers.resize(m_swapChainFramebuffers.size());
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool        = m_commandPool;                     // 设置命令池
    allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;   // 设置命令缓冲级别，这里设置为一级命令缓冲
    allocInfo.commandBufferCount = 1;                                 // 设置命令缓冲数量
    allocInfo.commandBufferCount = (uint32_t)m_commandBuffers.size(); // 设置命令缓冲数量
    if (vkAllocateCommandBuffers(m_device, &allocInfo, m_commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::createCommandBuffers()::创建命令缓冲失败");
    }
    spdlog::trace("VulkanRenderer::createCommandBuffers()::创建命令缓冲成功，数量：{}", m_commandBuffers.size());
}
void VulkanRenderer::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::recordCommandBuffer()::开始记录命令缓冲失败");
    }
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType             = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass        = m_renderPass;                        // 设置渲染通道
    renderPassInfo.framebuffer       = m_swapChainFramebuffers[imageIndex]; // 设置帧缓冲
    renderPassInfo.renderArea.offset = {0, 0};                              // 设置渲染区域偏移
    renderPassInfo.renderArea.extent = m_swapChainExtent;                   // 设置渲染区域大小
    VkClearValue clearColor          = {{{0.0f, 0.0f, 0.0f, 1.0f}}};        // 设置清除值
    renderPassInfo.clearValueCount   = 1;                                   // 设置清除值数量
    renderPassInfo.pClearValues      = &clearColor;                         // 设置清除值
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, m_graphicsPipeline);

        // 目标三角形的宽高比
        float targetAspectRatio = 4.0f / 3.0f; // 例如 4.0f / 3.0f
        // 计算目标宽度和高度
        float width  = static_cast<float>(m_swapChainExtent.width);
        float height = static_cast<float>(m_swapChainExtent.height);
        // 根据目标宽高比调整视口
        if (width / height > targetAspectRatio) {
            width = height * targetAspectRatio; // 窗口更宽，按高度缩放
        } else {
            height = width / targetAspectRatio; // 窗口更高，按宽度缩放
        }
        // 计算视口的 x 和 y 偏移量以使三角形居中
        float x = (static_cast<float>(m_swapChainExtent.width) - width) / 2.0f;
        float y = (static_cast<float>(m_swapChainExtent.height) - height) / 2.0f;

        VkViewport viewport{};
        viewport.x        = x;      // 设置视口x坐标
        viewport.y        = y;      // 设置视口y坐标
        viewport.width    = width;  // 设置视口宽度
        viewport.height   = height; // 设置视口高度
        viewport.minDepth = 0.0f;   // 设置视口最小深度
        viewport.maxDepth = 1.0f;   // 设置视口最大深度
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        VkRect2D scissor{};
        scissor.offset = {0, 0};            // 设置剪裁区域偏移
        scissor.extent = m_swapChainExtent; // 设置剪裁区域大小
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
        vkCmdDraw(commandBuffer, 3, 1, 0, 0); // 绘制三角形
    }
    vkCmdEndRenderPass(commandBuffer);
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::recordCommandBuffer()::结束记录命令缓冲失败");
    }
}
void VulkanRenderer::createSyncObjects() {
    m_imageAvailableSemaphores.resize(m_commandBuffers.size());
    m_renderFinishedSemaphores.resize(m_commandBuffers.size());
    m_inFlightFences.resize(m_commandBuffers.size());
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // 设置Fence标志，这里设置为已信号标志

    for (size_t i = 0; i < m_commandBuffers.size(); i++) {
        if (vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(m_device, &semaphoreInfo, nullptr, &m_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(m_device, &fenceInfo, nullptr, &m_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("VulkanRenderer::createSyncObjects()::创建同步对象失败");
        }
    }
    spdlog::trace("VulkanRenderer::createSyncObjects()::创建同步对象成功，数量：{}", m_commandBuffers.size());
}
#pragma endregion

#pragma region Render and Recreate Swap Chain
void VulkanRenderer::drawFrame() {
    vkWaitForFences(m_device, 1, &m_inFlightFences[m_currentFrame], VK_TRUE, UINT64_MAX); // 等待Fence信号
    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(m_device, m_swapChain, UINT64_MAX, m_imageAvailableSemaphores[m_currentFrame], VK_NULL_HANDLE, &imageIndex); // 获取下一个图像
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain();
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("VulkanRenderer::drawFrame()::获取下一个交换链图像失败");
    }
    vkResetFences(m_device, 1, &m_inFlightFences[m_currentFrame]);                              // 重置Fence信号
    vkResetCommandBuffer(m_commandBuffers[m_currentFrame], /*VkCommandBufferResetFlagBits*/ 0); // 重置命令缓冲
    // 在记录命令缓冲之前，我们需要确保我们正在渲染的图像已经准备好，并且没有其他操作正在使用它
    recordCommandBuffer(m_commandBuffers[m_currentFrame], imageIndex); // 记录命令缓冲，并绘制三角形，渲染到帧缓冲
    // 在这个时候，我们已经有了一个渲染好的图像，并且已经准备好了命令缓冲，现在我们可以提交命令缓冲并呈现图像了
    VkSubmitInfo submitInfo{};
    submitInfo.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    VkSemaphore waitSemaphores[]      = {m_imageAvailableSemaphores[m_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount     = 1;                                 // 设置等待信号量的数量
    submitInfo.pWaitSemaphores        = waitSemaphores;                    // 设置等待信号量
    submitInfo.pWaitDstStageMask      = waitStages;                        // 设置等待阶段
    submitInfo.commandBufferCount     = 1;                                 // 设置命令缓冲数量
    submitInfo.pCommandBuffers        = &m_commandBuffers[m_currentFrame]; // 设置命令缓冲
    VkSemaphore signalSemaphores[]    = {m_renderFinishedSemaphores[m_currentFrame]};
    submitInfo.signalSemaphoreCount   = 1;                // 设置信号量的数量
    submitInfo.pSignalSemaphores      = signalSemaphores; // 设置信号量
    if (vkQueueSubmit(m_graphicsQueue, 1, &submitInfo, m_inFlightFences[m_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::drawFrame()::提交命令缓冲失败");
    }
    // 提交命令缓冲后，我们可以使用present函数将图像呈现到屏幕上，提交到队列后，图像将呈现到屏幕上
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;                // 设置等待信号量的数量
    presentInfo.pWaitSemaphores    = signalSemaphores; // 设置等待信号量
    VkSwapchainKHR swapChains[]    = {m_swapChain};    // 设置交换链，这里只有一个交换链
    presentInfo.swapchainCount     = 1;                // 设置交换链数量
    presentInfo.pSwapchains        = swapChains;       // 设置交换链
    presentInfo.pImageIndices      = &imageIndex;      // 设置图像索引

    result = vkQueuePresentKHR(m_presentQueue, &presentInfo); // 提交到队列
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || m_framebufferResized) {
        m_framebufferResized = false; // 如果交换链需要重新创建，则将标志设置为false
        recreateSwapChain();
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("VulkanRenderer::drawFrame()::呈现交换链图像失败");
    }
    // 现在图像已经呈现到屏幕上了，我们可以开始下一帧了
    m_currentFrame = (m_currentFrame + 1) % m_commandBuffers.size();
}
void VulkanRenderer::cleanupSwapChain() {
    for (auto framebuffer : m_swapChainFramebuffers) {
        vkDestroyFramebuffer(m_device, framebuffer, nullptr);
    }
    for (auto imageView : m_swapChainImageViews) {
        vkDestroyImageView(m_device, imageView, nullptr);
    }
    vkDestroySwapchainKHR(m_device, m_swapChain, nullptr);
}
void VulkanRenderer::recreateSwapChain() {
    vkDeviceWaitIdle(m_device);
    cleanupSwapChain();

    createSwapChain();
    createImageViews();
    createFramebuffers();
}
#pragma endregion

} // namespace engine::render