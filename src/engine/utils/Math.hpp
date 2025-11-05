#pragma once
#include <glm/glm.hpp>
#include <vulkan/vulkan.h>

#include <array>

namespace engine::utils {

struct Vertex {
    glm::vec2 pos;
    glm::vec3 color;

    static VkVertexInputBindingDescription getBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding   = 0;                           // 顶点属性绑定索引
        bindingDescription.stride    = sizeof(Vertex);              // 每个顶点的大小
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // 输入速率，表示每顶点输入一次数据
        return bindingDescription;                                  // 返回顶点输入绑定描述
    }

    static std::array<VkVertexInputAttributeDescription, 2> getAttributeDescriptions() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};
        attributeDescriptions[0].binding  = 0;                          // 顶点属性绑定索引
        attributeDescriptions[0].location = 0;                          // 顶点属性位置索引
        attributeDescriptions[0].format   = VK_FORMAT_R32G32_SFLOAT;    // 顶点属性格式
        attributeDescriptions[0].offset   = offsetof(Vertex, pos);      // 顶点属性在顶点结构体中的偏移量
        attributeDescriptions[1].binding  = 0;                          // 顶点属性绑定索引
        attributeDescriptions[1].location = 1;                          // 顶点属性位置索引
        attributeDescriptions[1].format   = VK_FORMAT_R32G32B32_SFLOAT; // 顶点属性格式
        attributeDescriptions[1].offset   = offsetof(Vertex, color);    // 顶点属性在顶点结构体中的偏移量
        return attributeDescriptions;
    }
};

} // namespace engine::utils