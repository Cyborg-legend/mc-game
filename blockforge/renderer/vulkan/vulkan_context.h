#pragma once

#include <vulkan/vulkan.h>
#include <string>
#include <cstdint>
#include <vector>
#include <array>

// VulkanContext: holds core Vulkan objects
struct VulkanContext {
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
    VkQueue computeQueue = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT debugCallback = VK_NULL_HANDLE;

    // Swapchain
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkExtent2D swapchainExtent;
    VkFormat swapchainFormat;
    VkImage swapchainImage;
    VkDeviceMemory swapchainImageMemory;
    VkImageView swapchainImageView;

    // Render pass
    VkRenderPass renderPass = VK_NULL_HANDLE;

    // Pipeline
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;

    // Command buffers
    std::vector<VkCommandBuffer> commandBuffers;

    // Framebuffers
    std::vector<VkFramebuffer> framebuffers;

    // Image views for swapchain images
    std::vector<VkImageView> imageViews;

    // Descriptor set layout
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;

    // Descriptor pool
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;

    // Validation layers (debug)
    bool enableValidationLayers = true;

    // Device properties
    VkPhysicalDeviceProperties deviceProperties;
    VkPhysicalDeviceFeatures deviceFeatures;
};