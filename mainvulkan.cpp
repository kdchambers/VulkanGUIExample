#include "mainvulkan.h"

static size_t currentFrame = 0;
static bool framebufferResized = false;

static double updatedFrameWidthRatio = 1.0;
static double updatedFrameHeightRatio = 1.0;

static uint16_t currentWindowHeight = vconfig::INITIAL_WINDOW_HEIGHT;
static uint16_t currentWindowWidth = vconfig::INITIAL_WINDOW_WIDTH;

const uint8_t VERTICES_PER_SQUARE = 4;
const uint8_t INDICES_PER_SQUARE = 6;

void recreateSwapChain(VulkanApplication& app)
{
    int width = 0, height = 0;

    while(width == 0 || height == 0)
    {
        glfwGetFramebufferSize(app.window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(app.device);

    cleanupSwapChain(app);

    createSwapChain(app.physicalDevice, app.device, app.surface, app.swapChain, app.swapChainImages, app.swapChainImageFormat, app.swapChainExtent, app.window);

    // Create Image View BEGIN
    app.swapChainImageViews.resize(app.swapChainImages.size());

    for (size_t i = 0; i < app.swapChainImages.size(); i++)
    {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = app.swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = app.swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(app.device, &createInfo, nullptr, &app.swapChainImageViews[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views!");
        }
    }

    // Create Image View END

    // TODO: Everything seems in order. Please disperse.
    bool hacky_bool_fix_later = true;

    updatedFrameWidthRatio = static_cast<double>(width) / currentWindowWidth;
    updatedFrameHeightRatio = static_cast<double>(height) / currentWindowHeight;

    for(VulkanApplicationPipeline& pipeline : app.pipelines)
    {
        vkDestroyDescriptorSetLayout(app.device, pipeline.descriptorSetLayout, nullptr);

        GenericGraphicsPipelineTargets pipelineSetup
        {
            & pipeline.graphicsPipeline,
            & pipeline.pipelineLayout,
            & pipeline.renderPass,
            & pipeline.descriptorSetLayout,
            & pipeline.swapChainFramebuffers
        };

        if(! updateGenericGraphicsPipeline(app.device, app.swapChainExtent, app.swapChainImageViews, static_cast<uint8_t>(app.swapChainImages.size()), pipelineSetup, pipeline.setupCache)) {
            throw std::runtime_error("Failed to update pipeline");
        }

        hacky_bool_fix_later = false;

        uint32_t numVertices = pipeline.numVertices;
        uint8_t * currentVertex = pipeline.pipelineMappedMemory + pipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)].offset;
        uint32_t vertexStride = pipeline.vertexStride;

        // TODO: There's a better way to do this..
        for(uint32_t i = 0; i < numVertices; i++)
        {
            reinterpret_cast<glm::vec2 *>(currentVertex)->x += 1;
            reinterpret_cast<glm::vec2 *>(currentVertex)->x /= 2;

            reinterpret_cast<glm::vec2 *>(currentVertex)->x /= static_cast<float>(updatedFrameWidthRatio);

            reinterpret_cast<glm::vec2 *>(currentVertex)->x *= 2;
            reinterpret_cast<glm::vec2 *>(currentVertex)->x -= 1;

            reinterpret_cast<glm::vec2 *>(currentVertex)->y += 1;
            reinterpret_cast<glm::vec2 *>(currentVertex)->y /= 2;

            reinterpret_cast<glm::vec2 *>(currentVertex)->y /= static_cast<float>(updatedFrameHeightRatio);

            reinterpret_cast<glm::vec2 *>(currentVertex)->y *= 2;
            reinterpret_cast<glm::vec2 *>(currentVertex)->y -= 1;

            currentVertex += vertexStride;
        }
    }

    assert(width > 0);
    assert(height > 0);

    currentWindowWidth = static_cast<uint16_t>(width);
    currentWindowHeight = static_cast<uint16_t>(height);

    // Create Description Pool Begin

    std::array<VkDescriptorPoolSize, 1> descriptorPoolSizes = {};

    descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSizes[0].descriptorCount = static_cast<uint32_t>(app.swapChainImages.size());

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
    poolInfo.pPoolSizes = descriptorPoolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(app.swapChainImages.size());

    if (vkCreateDescriptorPool(app.device, &poolInfo, nullptr, &app.descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    // Create Description Pool END

    for(VulkanApplicationPipeline& pipeline : app.pipelines)
    {
        if(pipeline.descriptorSetLayout != nullptr)
        {
            std::vector<VkDescriptorSetLayout> layouts(app.swapChainImages.size(), pipeline.descriptorSetLayout);

            VkDescriptorSetAllocateInfo descriptorAllocInfo = {};
            descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
            descriptorAllocInfo.descriptorPool = app.descriptorPool;
            descriptorAllocInfo.descriptorSetCount = static_cast<uint32_t>(app.swapChainImages.size());
            descriptorAllocInfo.pSetLayouts = layouts.data();

            pipeline.descriptorSets.resize(app.swapChainImages.size());

            if (vkAllocateDescriptorSets(app.device, &descriptorAllocInfo, pipeline.descriptorSets.data()) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate descriptor sets!");
            }

            // TODO: This isn't generic..
            for (size_t i = 0; i < app.swapChainImages.size(); i++) {

                VkDescriptorImageInfo imageInfo = {};

                imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                imageInfo.imageView = pipeline.textureImageView;
                imageInfo.sampler = pipeline.textureSampler;

                assert(pipeline.textureImageView && "Image view -> null");
                assert(pipeline.textureSampler && "Sampler -> null");

                std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};

                descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                descriptorWrites[0].dstSet = pipeline.descriptorSets[i];
                descriptorWrites[0].dstBinding = 0;
                descriptorWrites[0].dstArrayElement = 0;
                descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                descriptorWrites[0].descriptorCount = 1;
                descriptorWrites[0].pImageInfo = &imageInfo;

                vkUpdateDescriptorSets(app.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
            }
        }
    }

    app.commandBuffers.resize(app.swapChainImages.size());

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.commandPool = app.commandPool;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;

    assert(app.commandBuffers.size() <= UINT32_MAX);

    commandBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(app.commandBuffers.size());

    if (vkAllocateCommandBuffers(app.device, &commandBufferAllocInfo, app.commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    VkClearValue clearColor = { /* .color = */  {  /* .float32 = */  { 1.0f, 1.0f, 1.0f, 1.0f } } };

    for (size_t i = 0; i < app.commandBuffers.size(); i++)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(app.commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        for(size_t layerIndex = 0; layerIndex < PipelineType::SIZE; layerIndex++)
        {
            VulkanApplicationPipeline& pipeline = app.pipelines[ app.pipelineDrawOrder[layerIndex] ];

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = pipeline.renderPass;
            renderPassInfo.framebuffer = pipeline.swapChainFramebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = app.swapChainExtent;

            bool requiresInitialClear = (layerIndex == 0);

            renderPassInfo.clearValueCount = (requiresInitialClear) ? 1 : 0;
            renderPassInfo.pClearValues = (requiresInitialClear) ? &clearColor : nullptr;

            vkCmdBeginRenderPass(app.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(app.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);

                VkBuffer vertexBuffers[] = {pipeline.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(app.commandBuffers[i], 0, 1, vertexBuffers, offsets);

                vkCmdBindIndexBuffer(app.commandBuffers[i], pipeline.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

                if(pipeline.pipelineLayout != nullptr && pipeline.descriptorSets.size() != 0) {
                    vkCmdBindDescriptorSets(app.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSets[i], 0, nullptr);
                }

                vkCmdDrawIndexed(app.commandBuffers[i], pipeline.numIndices, 1, 0, 0, 0);

            vkCmdEndRenderPass(app.commandBuffers[i]);
        }

        if (vkEndCommandBuffer(app.commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}

int main()
{
    VulkanApplication app = setupApplication();
    loadInitialMeshData(app, 0);

    mainLoop(app);
    cleanup(app);

    puts("Clean termination");

    return 1;
}

void drawFrame(VulkanApplication& app)
{
    vkWaitForFences(app.device, 1, & app.inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

    uint32_t imageIndex;
    VkResult result = vkAcquireNextImageKHR(app.device, app.swapChain, UINT64_MAX, app.imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        recreateSwapChain(app);
        return;
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        throw std::runtime_error("failed to acquire swap chain image!");
    }

    VkSubmitInfo submitInfo = {};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

    VkSemaphore waitSemaphores[] = { app.imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;

    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &app.commandBuffers[imageIndex];

    VkSemaphore signalSemaphores[] = { app.renderFinishedSemaphores[currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    vkResetFences(app.device, 1, & app.inFlightFences[currentFrame]);

    if (vkQueueSubmit(app.graphicsQueue, 1, &submitInfo, app.inFlightFences[currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    VkPresentInfoKHR presentInfo = {};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {app.swapChain};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;

    presentInfo.pImageIndices = &imageIndex;

    result = vkQueuePresentKHR(app.presentQueue, &presentInfo);

    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || framebufferResized)
    {
        framebufferResized = false;
        recreateSwapChain(app);
    } else if (result != VK_SUCCESS) {
        throw std::runtime_error("failed to present swap chain image!");
    }

    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void mainLoop(VulkanApplication& app)
{
    using namespace std::chrono_literals;

    uint32_t framesPerSec = 0;

    constexpr uint16_t targetFPS = 50;
    constexpr std::chrono::duration<long, std::milli> msPerFrame(1000 / targetFPS);

    std::chrono::milliseconds sinceLastFPSPrint = 0ms;

    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long, std::milli>>  frameStart;
    std::chrono::time_point<std::chrono::steady_clock, std::chrono::duration<long, std::milli>>  frameEnd;

    std::chrono::duration<long, std::milli> frameLength;

    while(!glfwWindowShouldClose(app.window))
    {
        frameStart = std::chrono::time_point_cast<std::chrono::duration<long, std::milli>>( std::chrono::steady_clock::now() );

        glfwPollEvents();
        //glfwWaitEventsTimeout(0.0005);

        if(sinceLastFPSPrint >= 1000ms)
        {
            printf("FPS: %d\n", framesPerSec);
            framesPerSec = 0;
            sinceLastFPSPrint = 0ms;
        }

        loopLogic(app, msPerFrame);

        drawFrame(app);
        framesPerSec++;

        frameEnd = std::chrono::time_point_cast<std::chrono::duration<long, std::milli>>( std::chrono::steady_clock::now() );
        sinceLastFPSPrint += msPerFrame;

        frameLength = frameEnd - frameStart;

        if(frameLength >= msPerFrame) {
            printf("No sleep this frame :(\n");
            continue;
        }

        std::this_thread::sleep_for(msPerFrame - frameLength);
    }

    vkDeviceWaitIdle(app.device);
}

void createVertexBuffer(    const VkDevice device,
                            const VkPhysicalDevice physicalDevice,
                            const VkQueue graphicsQueue,
                            const VkCommandPool commandPool,
                            uint8_t * vertexData,
                            const uint32_t vertexDataSize,
                            VkBuffer &outVertexBuffer,
                            VkDeviceMemory& outVertexBufferMemory,
                            void ** outMappedMemory )
{

    if(vertexDataSize == 0) {
        return;
    }

    if(outVertexBuffer != nullptr)
    {
        puts("Warning: Existing Vertex Buffer being deallocated");
        vkDestroyBuffer(device, outVertexBuffer, nullptr);
        vkFreeMemory(device, outVertexBufferMemory, nullptr);
    }

    VkDeviceSize bufferSize = vertexDataSize;

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(   device,
                    physicalDevice,
                    bufferSize,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingBuffer,
                    stagingBufferMemory);

    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, outMappedMemory);
        memcpy(*outMappedMemory, vertexData, bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);


    createBuffer(   device,
                    physicalDevice,
                    bufferSize,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    outVertexBuffer,
                    outVertexBufferMemory);

    copyBuffer( device,
                commandPool,
                graphicsQueue,
                stagingBuffer,
                outVertexBuffer,
                bufferSize);

    vkMapMemory(device, outVertexBufferMemory, 0, bufferSize, 0, outMappedMemory);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

void createIndexBuffer( const VkDevice device,
                        const VkPhysicalDevice physicalDevice,
                        const VkQueue graphicsQueue,
                        const VkCommandPool commandPool,
                        const std::vector<uint16_t>& indices,
                        VkBuffer &outIndicesBuffer,
                        VkDeviceMemory& outIndicesBufferMemory)
{
    VkDeviceSize bufferSize = sizeof(indices[0]) * indices.size();

    if(bufferSize == 0) {
        return;
    }

    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;

    createBuffer(   device,
                    physicalDevice,
                    bufferSize,
                    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    stagingBuffer,
                    stagingBufferMemory);

    void * mappedIndicesData;

    vkMapMemory(device, stagingBufferMemory, 0, bufferSize, 0, &mappedIndicesData);
        memcpy(mappedIndicesData, indices.data(), bufferSize);
    vkUnmapMemory(device, stagingBufferMemory);

    createBuffer(   device,
                    physicalDevice,
                    bufferSize,
                    VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    outIndicesBuffer,
                    outIndicesBufferMemory );

    copyBuffer( device,
                commandPool,
                graphicsQueue,
                stagingBuffer,
                outIndicesBuffer,
                bufferSize);

    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
}

static void framebufferResizeCallback(GLFWwindow* window, int width, int height)
{
    // Disable parameters not used warnings
    (void)window;
    (void)width;
    (void)height;

    framebufferResized = true;
}

void onTimeUpdate(VulkanApplication& app, uint32_t delta)
{
    //    VulkanApplicationPipeline& texturesPipeline = app.pipelines[PipelineType::Texture];
    VulkanApplicationPipeline& primativeShapesPipeline = app.pipelines[PipelineType::PrimativeShapes];

    updateAddVertexPositions(   reinterpret_cast<glm::vec2 *>(primativeShapesPipeline.pipelineMappedMemory),
                                primativeShapesPipeline.numVertices,
                                primativeShapesPipeline.vertexStride,
                                static_cast<float>(delta) / 50.0f,
                                0 );
}

// TODO: Remove
void onTimeUpdateExperimental(VulkanApplication& app, uint32_t delta)
{
    for(uint16_t i = 0; i < app.entitySystem.exampleTimeUpdateListSize; i++) {
        RelativeDataLocation& verticesTarget = app.entitySystem.verticesComponent[app.entitySystem.exampleTimeUpdateList[i]];
        updateAddVertexPositions(reinterpret_cast<glm::vec2*>(app.entitySystem.verticesComponentBasePtr + verticesTarget.offsetBytes), verticesTarget.spanElements, verticesTarget.strideBytes, 0.001f, 0.001f);
    }
}

void doPerFrameOperations(VulkanApplication& app)
{
    for(uint16_t i = 0; i < app.numPerFrameOperations; i++) {
        handleOperation(app, app.perFrameOperationIndices[i]);
    }
}

void loopLogic(VulkanApplication& app, std::chrono::milliseconds delta)
{
    doPerFrameOperations(app);

//    updateAddVertexPositions(reinterpret_cast<glm::vec2*>(app.mappedVerticesMemory), 24, sizeof(Vertex), 0.001f, 0.001f);

    VkClearValue clearColor = { /* .color = */  {  /* .float32 = */  { 1.0f, 1.0f, 1.0f, 1.0f } } };

    vkDeviceWaitIdle(app.device);

    for (size_t i = 0; i < app.commandBuffers.size(); i++)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(app.commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        for(size_t layerIndex = 0; layerIndex < PipelineType::SIZE; layerIndex++)
        {
            VulkanApplicationPipeline& pipeline = app.pipelines[ app.pipelineDrawOrder[layerIndex] ];

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = pipeline.renderPass;
            renderPassInfo.framebuffer = pipeline.swapChainFramebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = app.swapChainExtent;

            bool requiresInitialClear = (layerIndex == 0);

            renderPassInfo.clearValueCount = (requiresInitialClear) ? 1 : 0;
            renderPassInfo.pClearValues = (requiresInitialClear) ? &clearColor : nullptr;

            vkCmdBeginRenderPass(app.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(app.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);

                VkBuffer vertexBuffers[] = {pipeline.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(app.commandBuffers[i], 0, 1, vertexBuffers, offsets);

                vkCmdBindIndexBuffer(app.commandBuffers[i], pipeline.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

                if(pipeline.pipelineLayout != nullptr && pipeline.descriptorSets.size() != 0) {
                    vkCmdBindDescriptorSets(app.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSets[i], 0, nullptr);
                }

                vkCmdDrawIndexed(app.commandBuffers[i], pipeline.numIndices, 1, 0, 0, 0);

            vkCmdEndRenderPass(app.commandBuffers[i]);
        }

        if (vkEndCommandBuffer(app.commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}

int16_t doublePercentageToInt16(double value) {
    return static_cast<int16_t>(value * 10000.0);
}

double int16PercentageToDouble(int16_t value) {
    return value / 10000.0;
}

float int16PercentageToFloat(int16_t value) {
    return value / 10000.0f;
}

struct PageSetupState
{
    uint16_t currentVerticesIndex;
    uint16_t currentIndicesIndex;
    uint8_t currentMouseBoundsIndex;
};

void button(VulkanApplication& app, glm::vec3 color, std::string& text, NormalizedPoint tlPoint)
{
    NormFloat16 height;
    height.set(0.07);

    NormFloat16 width;
    width.set(0.2);

    VulkanApplicationPipeline& texturesPipeline = app.pipelines[PipelineType::Texture];
    VulkanApplicationPipeline& primativeShapesPipeline = app.pipelines[PipelineType::PrimativeShapes];

    std::vector<BasicVertex> simpleShapesVertices = {
        {{tlPoint.x.get() + width.get(), tlPoint.y.get()},                  {color.r, color.g, color.b}},   // Top right
        {{tlPoint.x.get() + width.get(), tlPoint.x.get() + height.get()},   {color.r, color.g, color.b}},   // Bottom right
        {{tlPoint.x.get(), tlPoint.x.get() + height.get()},                 {color.r, color.g, color.b}},   // Bottom left
        {{tlPoint.x.get(), tlPoint.y.get()},                                {color.r, color.g, color.b}}    // Top left
    };

    memcpy(primativeShapesPipeline.writeVertices(app.mappedVerticesMemory, VERTICES_PER_SQUARE, sizeof(BasicVertex), offsetof(BasicVertex, pos)),
           simpleShapesVertices.data(), simpleShapesVertices.size() * sizeof(BasicVertex));

    std::vector<uint16_t> drawIndices = {
        0, 1, 2, 2, 3, 0
    };

    memcpy(primativeShapesPipeline.writeIndices(app.mappedIndicesMemory, INDICES_PER_SQUARE), drawIndices.data(), drawIndices.size() * sizeof(uint16_t));

//    NormalizedPoint point;
//    point.x.set(0.0);
//    point.y.set(0.0);

    drawText(app, tlPoint, text);

//    uint16_t requiredIndices = static_cast<uint16_t>(text.size() * INDICES_PER_SQUARE);
//    uint16_t requiredVertices = static_cast<uint16_t>(text.size() * VERTICES_PER_SQUARE);

//    glm::vec2 * startTexCoordPos = texturesPipeline.getFreeVertices(app.mappedVerticesMemory, sizeof(Vertex), offsetof(Vertex, texCoord));
//    uint16_t verticesStartIndex = static_cast<uint16_t>(texturesPipeline.numVertices);

//    generateTextMeshes(texturesPipeline.writeIndices(app.mappedIndicesMemory, requiredIndices),
//                       texturesPipeline.writeVertices(app.mappedVerticesMemory, requiredVertices, sizeof(Vertex), offsetof(Vertex, pos)),
//                       sizeof(Vertex),
//                       verticesStartIndex,
//                       app.fontBitmap,
//                       startTexCoordPos,
//                       texturesPipeline.vertexStride,
//                       text, 500, 280);

//    app.entitySystem.verticesComponent[app.entitySystem.nextEntity] = { vconfig::PIPELINE_MEMORY_SIZE / 2, 4, primativeShapesPipeline.vertexStride };
//    app.entitySystem.numberVerticesComponents++;
//    app.entitySystem.nextEntity++;

//    assert(texturesPipeline.numIndices == requiredIndices);

     // TODO: Audit and refactor
//    app.entitySystem.verticesComponent[app.entitySystem.nextEntity] =
//    {
//        vconfig::PIPELINE_MEMORY_SIZE / 2,
//        static_cast<uint16_t>(4),
//        sizeof(BasicVertex)
//    };

//    app.entitySystem.numberVerticesComponents++;
//    app.entitySystem.nextEntity++;
}

Point unnormalizePoint(NormalizedPoint point, uint16_t widthPixels, uint16_t heightPixels)
{
    double x = point.x.get();
    x += 1.0;
    x /= 2;
    point.x.set(x);

    double y = point.y.get();
    y += 1.0;
    y /= 2;
    point.y.set(y);

    return Point {
        static_cast<uint16_t>(point.x.get() * static_cast<double>(widthPixels)),
        static_cast<uint16_t>(point.y.get() * static_cast<double>(heightPixels))
    };
}

uint32_t drawText(VulkanApplication& app, NormalizedPoint point, std::string& text)
{
    uint16_t requiredVertices = static_cast<uint16_t>(text.size()) * VERTICES_PER_SQUARE;
    uint16_t requiredIndices = static_cast<uint16_t>(text.size()) * INDICES_PER_SQUARE;

    assert(text.size() == 8);

    VulkanApplicationPipeline& texturesPipeline = app.pipelines[PipelineType::Texture];

    glm::vec2 * startTexCoordPos = texturesPipeline.getFreeVertices(app.mappedVerticesMemory, sizeof(Vertex), offsetof(Vertex, texCoord));
    uint16_t verticesStartIndex = static_cast<uint16_t>(texturesPipeline.numVertices);

    Point pointPixels = unnormalizePoint(point, 800, 600);

    generateTextMeshes(texturesPipeline.writeIndices(app.mappedIndicesMemory, requiredIndices),
                       texturesPipeline.writeVertices(app.mappedVerticesMemory, requiredVertices, sizeof(Vertex), offsetof(Vertex, pos)),
                       sizeof(Vertex),
                       verticesStartIndex,
                       app.fontBitmap,
                       startTexCoordPos,
                       texturesPipeline.vertexStride,
                       text, pointPixels.x, pointPixels.y);

    app.entitySystem.verticesComponent[app.entitySystem.nextEntity] = { 0, requiredVertices, texturesPipeline.vertexStride };
    app.entitySystem.numberVerticesComponents++;
    app.entitySystem.nextEntity++;

    return app.entitySystem.nextEntity - 1;
}

void loadInitialMeshData(VulkanApplication& app, uint32_t delta)
{
    int width = 0, height = 0;

    while(width == 0 || height == 0)
    {
        glfwGetFramebufferSize(app.window, &width, &height);
        glfwWaitEvents();
    }

    updatedFrameWidthRatio = static_cast<double>(width) / currentWindowWidth;
    updatedFrameHeightRatio = static_cast<double>(height) / currentWindowHeight;

    VulkanApplicationPipeline& texturesPipeline = app.pipelines[PipelineType::Texture];
    VulkanApplicationPipeline& primativeShapesPipeline = app.pipelines[PipelineType::PrimativeShapes];

    texturesPipeline.vertexStride = sizeof(Vertex);

    NormalizedPoint point;
    point.x.set( 0.0 );
    point.y.set( 0.0 );

    std::string buttonText = "click me";

    button(app, {0.0, 1.0, 0.0}, buttonText, point);

    std::string otherText = "How are you doing today? I hope you are doing well!";

    uint16_t requiredVertices = static_cast<uint16_t>(otherText.size()) * VERTICES_PER_SQUARE;
    uint16_t requiredIndices = static_cast<uint16_t>(otherText.size()) * INDICES_PER_SQUARE;

    glm::vec2 * startTexCoordPos = texturesPipeline.getFreeVertices(app.mappedVerticesMemory, sizeof(Vertex), offsetof(Vertex, texCoord));

    uint16_t verticesStartIndex = static_cast<uint16_t>(texturesPipeline.numVertices);

    generateTextMeshes(texturesPipeline.writeIndices(app.mappedIndicesMemory, requiredIndices),
                       texturesPipeline.writeVertices(app.mappedVerticesMemory, requiredVertices, sizeof(Vertex), offsetof(Vertex, pos)),
                       sizeof(Vertex),
                       verticesStartIndex,
                       app.fontBitmap,
                       startTexCoordPos,
                       texturesPipeline.vertexStride,
                       otherText, 150, 25);

    app.entitySystem.verticesComponent[app.entitySystem.nextEntity] = { 0, requiredVertices, texturesPipeline.vertexStride };
    app.entitySystem.numberVerticesComponents++;
    app.entitySystem.nextEntity++;

//    assert(texturesPipeline.numVertices == requiredVertices);

//    texturesPipeline.uiComponentsMap[0] = {
//        UIType::TEXT,
//        GraphicsUpdateDependency::STATIC,
//        0,
//        0,
//        requiredVertices,
//        requiredIndices
//    };

//    assert(texturesPipeline.numIndices == requiredIndices);

    std::string moreText = "New text would be pretty nice actually..";

    uint16_t moreRequiredIndices = static_cast<uint16_t>(moreText.size() * INDICES_PER_SQUARE);
    uint16_t moreRequiredVertices = static_cast<uint16_t>(moreText.size() * VERTICES_PER_SQUARE);

    glm::vec2 * startTexCoordPos2 = texturesPipeline.getFreeVertices(app.mappedVerticesMemory, sizeof(Vertex), offsetof(Vertex, texCoord));

//    assert(texturesPipeline.numIndices == requiredIndices);

//    assert(app.mappedIndicesMemory + texturesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::INDICES_BUFFER)].offset + (requiredIndices * 2) ==
//            reinterpret_cast<uint8_t *>( texturesPipeline.getFreeIndices(app.mappedIndicesMemory)) );

    verticesStartIndex = static_cast<uint16_t>(texturesPipeline.numVertices);

    generateTextMeshes( texturesPipeline.writeIndices(app.mappedIndicesMemory, moreRequiredIndices),
                        texturesPipeline.writeVertices(app.mappedVerticesMemory, moreRequiredVertices, sizeof(Vertex), offsetof(Vertex, pos)),
                        sizeof(Vertex),
                        verticesStartIndex,
                        app.fontBitmap,
                        startTexCoordPos2,
                        texturesPipeline.vertexStride,
                        moreText, 150, 250);

//    assert(texturesPipeline.numIndices == (moreText.size() * INDICES_PER_SQUARE) + (static_cast<uint16_t>(otherText.size()) * INDICES_PER_SQUARE));

//    assert(app.entitySystem.verticesComponent[app.entitySystem.nextEntity - 1].spanElements == requiredVertices);

    app.entitySystem.verticesComponent[app.entitySystem.nextEntity] =
    {
        app.entitySystem.verticesComponent[app.entitySystem.nextEntity - 1].spanElements,
        moreRequiredVertices,
        texturesPipeline.vertexStride
    };

    app.entitySystem.numberVerticesComponents++;
    app.entitySystem.nextEntity++;

//    texturesPipeline.uiComponentsMap[1] = {
//        UIType::TEXT,
//        GraphicsUpdateDependency::STATIC,
//        requiredVertices,
//        requiredIndices,
//        static_cast<uint16_t>(moreText.size() * VERTICES_PER_SQUARE),
//        static_cast<uint16_t>(moreText.size() * INDICES_PER_SQUARE)
//    };

    // Second pipeline

    std::vector<BasicVertex> simpleShapesVertices = {
        {{-0.5f, -1.0f}, {1.0f, 0.0f, 0.0f}},
        {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{-1.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
        {{-1.0f, -1.0f}, {1.0f, 0.0f, 0.0f}}
    };

//    primativeShapesPipeline.uiComponentsMap[0] = {
//        UIType::SHAPE,
//        GraphicsUpdateDependency::ALL,
//        0,
//        0,
//        4,
//        6
//    };

//    app.entitySystem.verticesComponent[app.entitySystem.nextEntity] =
//    {
//        primativeShapesPipeline.numVertices,
//        static_cast<uint16_t>(simpleShapesVertices.size()),
//        sizeof(BasicVertex)
//    };

//    app.entitySystem.numberVerticesComponents++;
//    app.entitySystem.nextEntity++;

//    assert(primativeShapesPipeline.numVertices == 0);

    static_assert(offsetof(BasicVertex, pos) == 0);

//    assert(primativeShapesPipeline.getFreeVertices(app.mappedVerticesMemory, sizeof(BasicVertex), offsetof(BasicVertex, pos)) ==
//           reinterpret_cast<glm::vec2*>( app.mappedVerticesMemory + primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)].offset ));

    memcpy(primativeShapesPipeline.writeVertices(app.mappedVerticesMemory, VERTICES_PER_SQUARE * 1, sizeof(BasicVertex), offsetof(BasicVertex, pos)),
           simpleShapesVertices.data(), simpleShapesVertices.size() * sizeof(BasicVertex));

//    assert(primativeShapesPipeline.getFreeVertices(app.mappedVerticesMemory, sizeof(BasicVertex), offsetof(BasicVertex, pos)) ==
//           reinterpret_cast<glm::vec2*>( app.mappedVerticesMemory + primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)].offset + (4 * sizeof(BasicVertex)) ));

//    assert(primativeShapesPipeline.numVertices == 4);

//    uint16_t nextVertexIndex = primativeShapesPipeline.numVertices;

//    std::vector<uint16_t> drawIndices = {
//        0, 1, 2, 2, 3, 0
//    };

    std::vector<uint16_t> drawIndices = {
        4, 5, 6, 6, 7, 4
    };

//    assert(primativeShapesPipeline.numIndices == 0);

//    assert(primativeShapesPipeline.getFreeIndices(app.mappedIndicesMemory) ==
//           reinterpret_cast<uint16_t*>(app.mappedIndicesMemory + primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::INDICES_BUFFER)].offset));

    memcpy(primativeShapesPipeline.writeIndices(app.mappedIndicesMemory, INDICES_PER_SQUARE * 1), drawIndices.data(), drawIndices.size() * sizeof(uint16_t));

//    assert(primativeShapesPipeline.numIndices == 6);

    app.entitySystem.verticesComponent[app.entitySystem.nextEntity] =
    {
        vconfig::PIPELINE_MEMORY_SIZE / 2,
        static_cast<uint16_t>(simpleShapesVertices.size()),
        sizeof(BasicVertex)
    };

    app.entitySystem.numberVerticesComponents++;
    app.entitySystem.nextEntity++;

    app.entitySystem.exampleTimeUpdateList[0] = app.entitySystem.nextEntity - 1;
    app.entitySystem.exampleTimeUpdateListSize++;

    RelativeMoveOperation relativeMove = { static_cast<Entity16>(app.entitySystem.nextEntity - 1), {30}, {30} };
    Operation8Union op8;
    op8.relativeMove = relativeMove;

    app.insertOp8(OPERATION_CODE_RELATIVE_MOVE, OPERATION_FLAGS_PER_FRAME, op8);

    assert((app.opAt(0).flags & OPERATION_FLAGS_SIZE_8) == OPERATION_FLAGS_SIZE_8);
    assert(app.opAt(0).opCode == OPERATION_CODE_RELATIVE_MOVE);

    op8.mouseBoundsMove = { /* Bounds Index */ 0, {30}, {30} };
    app.insertOp8(OPERATION_CODE_APPLY_MOVE_TO_BOUNDS, OPERATION_FLAGS_PER_FRAME, op8);

//    assert((app.opAt(1).flags & OPERATION_FLAGS_SIZE_8) == OPERATION_FLAGS_SIZE_8);
//    assert(app.opAt(1).opCode == OPERATION_CODE_APPLY_MOVE_TO_BOUNDS);

    Operation4Union op4;
    op4.operationIndex = 0;

    app.insertOp4(OPERATION_CODE_DEACTIVATE_OP, 0, op4);

    op4.operationIndex = 1;
    app.insertOp4(OPERATION_CODE_DEACTIVATE_OP, 0, op4);

//    assert((app.opAt(2).flags & OPERATION_FLAGS_SIZE_4) == OPERATION_FLAGS_SIZE_4);
//    assert(app.opAt(2).opCode == OPERATION_CODE_DEACTIVATE_OP);

    app.onMouseEventOpBindings.numAreas = 0;
    app.onMouseEventOpBindings.numBounds = 1;

    app.onMouseEventOpBindings.bounds[0] =
    {
        { /* Point*/ { { NORMFLOAT_MIN }, { NORMFLOAT_MIN } }, { 5000 }, { 5000 } },
        MOUSE_BOUNDS_FLAGS_2_ON_HOVER_ENTER | MOUSE_BOUNDS_FLAGS_2_ON_HOVER_EXIT,
        0,
        1,
        2,
        3,
        UINT8_MAX,
        0
    };

//    assert(primativeShapesPipeline.numVertices == static_cast<uint32_t>(simpleShapesVertices.size()));
//    assert(primativeShapesPipeline.numIndices == static_cast<uint32_t>(drawIndices.size()));

//    primativeShapesPipeline.numVertices = static_cast<uint32_t>(simpleShapesVertices.size());
//    primativeShapesPipeline.numIndices = static_cast<uint32_t>(drawIndices.size());
    primativeShapesPipeline.vertexStride = sizeof(BasicVertex);

//    NormalizedPoint point2;
//    point.x.set( 0.3 );
//    point.y.set( 0.3 );

////    std::string buttonText = "click me";

//    button(app, {0.0, 1.0, 0.0}, buttonText, point2);

    VkClearValue clearColor = { /* .color = */  {  /* .float32 = */  { 1.0f, 1.0f, 1.0f, 1.0f } } };

    vkDeviceWaitIdle(app.device);

    for (size_t i = 0; i < app.commandBuffers.size(); i++)
    {
        VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

        if (vkBeginCommandBuffer(app.commandBuffers[i], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        for(size_t layerIndex = 0; layerIndex < PipelineType::SIZE; layerIndex++)
        {
            VulkanApplicationPipeline& pipeline = app.pipelines[ app.pipelineDrawOrder[layerIndex] ];

            VkRenderPassBeginInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = pipeline.renderPass;
            renderPassInfo.framebuffer = pipeline.swapChainFramebuffers[i];
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = app.swapChainExtent;

            bool requiresInitialClear = (layerIndex == 0);

            renderPassInfo.clearValueCount = (requiresInitialClear) ? 1 : 0;
            renderPassInfo.pClearValues = (requiresInitialClear) ? &clearColor : nullptr;

            vkCmdBeginRenderPass(app.commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

                vkCmdBindPipeline(app.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.graphicsPipeline);

                VkBuffer vertexBuffers[] = {pipeline.vertexBuffer};
                VkDeviceSize offsets[] = {0};
                vkCmdBindVertexBuffers(app.commandBuffers[i], 0, 1, vertexBuffers, offsets);

                vkCmdBindIndexBuffer(app.commandBuffers[i], pipeline.indexBuffer, 0, VK_INDEX_TYPE_UINT16);

                if(pipeline.pipelineLayout != nullptr && pipeline.descriptorSets.size() != 0) {
                    vkCmdBindDescriptorSets(app.commandBuffers[i], VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline.pipelineLayout, 0, 1, &pipeline.descriptorSets[i], 0, nullptr);
                }

                assert(pipeline.numIndices >= 6);

                vkCmdDrawIndexed(app.commandBuffers[i], pipeline.numIndices, 1, 0, 0, 0);

            vkCmdEndRenderPass(app.commandBuffers[i]);
        }

        if (vkEndCommandBuffer(app.commandBuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }
}

uint16_t unnormalize(double percentage, double max)
{
    percentage += 1.0;
    percentage /= 2;

    return static_cast<uint16_t>(max * percentage);
}

static void handleOperation4(VulkanApplication& app, Operation4& operation)
{
    switch(operation.opCode)
    {
        case OPERATION_CODE_DEACTIVATE_OP:
        {
            for(uint16_t i = 0; i < app.numPerFrameOperations; i++) {
                if(app.perFrameOperationIndices[i] == operation.opData.operationIndex) {
                    // TODO: Hack, do proper removal of array element after testing
//                    printf("Removing active index -> %u\n", i);
                    removeArrayIndex(app.perFrameOperationIndices, app.numPerFrameOperations, i);
                    app.numPerFrameOperations--;
                    app.opAt(operation.opData.operationIndex).flags ^= OPERATION_FLAGS_ACTIVE;
                }
            }

            break;
        }
    }
}

static void handleOperation8(VulkanApplication& app, Operation8& operation)
{
    switch(operation.opCode)
    {
        case OPERATION_CODE_RELATIVE_MOVE: {
//            printf("move op\n");
            RelativeMoveOperation& relativeMove = operation.opData.relativeMove;
            RelativeDataLocation& verticesTarget = app.entitySystem.verticesComponent[relativeMove.targetEntity];

//            assert(verticesTarget.spanElements > 10);
//            assert(verticesTarget.strideBytes == sizeof(Vertex));
//            assert(verticesTarget.offsetBytes == 0);

            updateAddVertexPositions(   reinterpret_cast<glm::vec2*>(app.entitySystem.verticesComponentBasePtr + verticesTarget.offsetBytes),
                                        verticesTarget.spanElements,
                                        verticesTarget.strideBytes,
                                        relativeMove.addX.get(), relativeMove.addY.get());

//            printf("X -> %f\n", relativeMove.addX.get());
//            printf("Y -> %f\n", relativeMove.addY.get());

            break;
        }
        case OPERATION_CODE_ABSOLUTE_MOVE:
            assert(false && "OPERATION_CODE_ABSOLUTE_MOVE not implemented");
            break;
        case OPERATION_CODE_APPLY_MOVE_TO_BOUNDS:
        {

            const SimpleMouseBoundsMoveOperation& boundsMove = operation.opData.mouseBoundsMove;
            NormalizedRect& targetBounds = app.onMouseEventOpBindings.bounds[boundsMove.boundsIndex].boundsArea;
//            assert(boundsMove.addX == 10);
//            assert(boundsMove.addY == 10);
            targetBounds.topLeftPoint.x.addTo(boundsMove.addX);   // TODO: You need to decide whether the main unit is going to be pixels or screen percentages within the loop
            targetBounds.topLeftPoint.y.addTo(boundsMove.addY);

//            printf("Bounds X -> %f\n", targetBounds.topLeftPoint.x.get());
//            printf("Bounds Y -> %f\n", targetBounds.topLeftPoint.y.get());
            break;
        }
        default:
            assert(false && "invalid operation opCode [OPCODE_OFFSET_SIZE_8]");
    }
}

static void handleOperation(VulkanApplication& app, uint16_t operationIndex)
{
    Operation2& operation = app.opAt(operationIndex);

    if((operation.flags & OPERATION_FLAGS_PER_FRAME) && !(operation.flags & OPERATION_FLAGS_ACTIVE))
    {
        app.perFrameOperationIndices[ app.numPerFrameOperations ] = operationIndex;
        app.numPerFrameOperations++;
        operation.flags |= OPERATION_FLAGS_ACTIVE;
        return;
    }

    switch(operation.flags & 0b11000000)
    {
        case OPERATION_FLAGS_SIZE_2: { // TODO: Probably 'deprecate' Operation2

            break;
        }
        case OPERATION_FLAGS_SIZE_4: {
            Operation4& operation4 = *reinterpret_cast<Operation4*>(&operation);
            handleOperation4( app, operation4 );
            break;
        }
        case OPERATION_FLAGS_SIZE_8: {
            Operation8& operation8 = *reinterpret_cast<Operation8*>(&operation);
            handleOperation8(app, operation8);

            break;
        }
        case OPERATION_FLAGS_SIZE_16: {
            Operation16& operation16 = *reinterpret_cast<Operation16*>(&operation);

            break;
        }
        default: {
            printf("Invalid operation size\n");
        }
    }
}

bool isActiveBoundsIndex(VulkanApplication& app, uint16_t boundsIndex)
{
    for(uint16_t i = 0; i < app.numActiveBounds; i++)
    {
        if(app.activeBoundsIndices[i] == boundsIndex) {
            return true;
        }
    }

    return false;
}

bool removeArrayIndex(uint16_t *array, uint16_t arraySize, uint16_t arrayIndex)
{
    if(arrayIndex == arraySize - 1) {
        return true;
    }

    for(uint16_t i = arraySize - 1; i > arrayIndex; i--) {
        array[i - 1] = array[i];
    }

    return true;
}

bool removeArrayValue(uint16_t *array, uint16_t arraySize, uint16_t arrayValue)
{

    uint16_t foundIndex = arraySize;

    for(uint16_t i = 0; i < arraySize; i++) {
        if(array[i] == arrayValue) {
            foundIndex = i;
            break;
        }
    }

    if(foundIndex == arraySize) {
        return false;
    }

    if(foundIndex == arraySize - 1) {
        return true;
    }

    for(uint16_t i = arraySize - 1; i > foundIndex; i--) {
        array[i - 1] = array[i];
    }

    return true;
}

static void onCursorPosChanged(GLFWwindow * window, double xPos, double yPos)
{
//    assert(false && "onCursorPosChanged disabled\n");

    VulkanApplication& app = *reinterpret_cast<VulkanApplication*>( glfwGetWindowUserPointer(window) );

//    uint16_t xPixelPos = static_cast<uint16_t>(xPos);
//    uint16_t yPixelPos = static_cast<uint16_t>(yPos);

    xPos *= 2;
    yPos *= 2;

    xPos -= 800;
    yPos -= 600;

    xPos /= 800;
    yPos /= 600;

//    printf("X pos -> %f\n", xPos);
//    printf("Y pos -> %f\n", yPos);

    for(uint16_t i = 0; i < app.numActiveBounds; i++)
    {
        const MouseBounds& mouseBounds = app.onMouseEventOpBindings.bounds[ app.activeBoundsIndices[i] ];
        const NormalizedRect& boundsArea = mouseBounds.boundsArea;

        if( boundsArea.topLeftPoint.x.get() > xPos ||
            normfloat16::add(boundsArea.topLeftPoint.x, boundsArea.width) < xPos ||
            boundsArea.topLeftPoint.y.get() > yPos ||
            normfloat16::add(boundsArea.topLeftPoint.y, boundsArea.height) < yPos)
        {
            printf("left active area\n");

            removeArrayIndex(app.activeBoundsIndices, 10, i);
            app.numActiveBounds--;

            // TODO: If this works, remove all the function calls and add indices to a temp array and call in a loop
            switch(mouseBounds.flags & MOUSE_BOUNDS_FLAGS_NUM_ON_HOVER_ENTER_MASK)
            {
                case MOUSE_BOUNDS_FLAGS_3_ON_HOVER_ENTER:
                    switch(mouseBounds.flags & MOUSE_BOUNDS_FLAGS_NUM_ON_HOVER_EXIT_MASK)
                    {
                        case MOUSE_BOUNDS_FLAGS_3_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.numSubAreas);
                        [[fallthrough]];
                        case MOUSE_BOUNDS_FLAGS_2_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.onMClickOperation);
                        [[fallthrough]];
                        case MOUSE_BOUNDS_FLAGS_1_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.onRClickOperation);
                    }
                    break;
                case MOUSE_BOUNDS_FLAGS_2_ON_HOVER_ENTER:
                    switch(mouseBounds.flags & MOUSE_BOUNDS_FLAGS_NUM_ON_HOVER_EXIT_MASK)
                    {
                        case MOUSE_BOUNDS_FLAGS_3_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.onMClickOperation);
                        [[fallthrough]];
                        case MOUSE_BOUNDS_FLAGS_2_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.onRClickOperation);
                        [[fallthrough]];
                        case MOUSE_BOUNDS_FLAGS_1_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.onLClickOperation);
                    }
                    break;
                case MOUSE_BOUNDS_FLAGS_1_ON_HOVER_ENTER:
                    switch(mouseBounds.flags & MOUSE_BOUNDS_FLAGS_NUM_ON_HOVER_EXIT_MASK)
                    {
                        case MOUSE_BOUNDS_FLAGS_3_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.onRClickOperation);
                        [[fallthrough]];
                        case MOUSE_BOUNDS_FLAGS_2_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.onLClickOperation);
                        [[fallthrough]];
                        case MOUSE_BOUNDS_FLAGS_1_ON_HOVER_EXIT:
                            handleOperation(app, mouseBounds.onHoverExitOperation);
                    }
                    break;
                default:
                    assert(false && "Must be an ON_HOVER_ENTER for an ON_HOVER_EXIT");
            }

            assert(app.numActiveBounds == 0);
        }
    }

    for(uint16_t i = 0; i < app.onMouseEventOpBindings.numBounds; i++)
    {
        const MouseBounds& mouseBounds = app.onMouseEventOpBindings.bounds[i];
        const NormalizedRect& boundsArea = mouseBounds.boundsArea;

//        printf("TL X %f\n", boundsArea.topLeftPoint.x.get());
//        printf("TL Y %f\n", boundsArea.topLeftPoint.y.get());
//        printf("Width %f\n", boundsArea.width.get());
//        printf("Height %f\n", boundsArea.height.get());
//        printf("TL X + Width %f\n", normfloat16::add(boundsArea.topLeftPoint.x, boundsArea.width));
//        printf("TL Y + Height %f\n", normfloat16::add(boundsArea.topLeftPoint.y, boundsArea.height));

        if((mouseBounds.flags & MOUSE_BOUNDS_FLAGS_NUM_ON_HOVER_ENTER_MASK) == MOUSE_BOUNDS_FLAGS_0_ON_HOVER_ENTER) {
            continue;
        }

        if( (!isActiveBoundsIndex(app, i)) &&
            mouseBounds.onHoverEnterOperation != UINT8_MAX &&
            xPos >= boundsArea.topLeftPoint.x.get() &&
            xPos <= normfloat16::add(boundsArea.topLeftPoint.x, boundsArea.width) &&
            yPos >= boundsArea.topLeftPoint.y.get() &&
            yPos <= normfloat16::add(boundsArea.topLeftPoint.y, boundsArea.height))
        {
            printf("Entered bounds\n");

            if((mouseBounds.flags & MOUSE_BOUNDS_FLAGS_NUM_ON_HOVER_EXIT_MASK) != MOUSE_BOUNDS_FLAGS_0_ON_HOVER_EXIT)
            {
                app.activeBoundsIndices[ app.numActiveBounds ] = i;
                app.numActiveBounds++;
            }

            if(mouseBounds.flags == 0) {
                handleOperation(app, app.onMouseEventOpBindings.bounds[i].onHoverEnterOperation);
                continue;
            }

            // TODO: this..
            if((mouseBounds.flags & MOUSE_BOUNDS_FLAGS_NUM_ON_HOVER_ENTER_MASK) == MOUSE_BOUNDS_FLAGS_2_ON_HOVER_ENTER) {
                handleOperation(app, app.onMouseEventOpBindings.bounds[i].onHoverEnterOperation);
                handleOperation(app, app.onMouseEventOpBindings.bounds[i].onHoverExitOperation);
            }

            if((mouseBounds.flags & MOUSE_BOUNDS_FLAGS_NUM_ON_HOVER_ENTER_MASK) == MOUSE_BOUNDS_FLAGS_3_ON_HOVER_ENTER) {
                handleOperation(app, app.onMouseEventOpBindings.bounds[i].onHoverEnterOperation);
                handleOperation(app, app.onMouseEventOpBindings.bounds[i].onHoverExitOperation);
                handleOperation(app, app.onMouseEventOpBindings.bounds[i].onLClickOperation);
            }

            assert(app.numActiveBounds == 1);
        }
    }
}

VulkanApplication setupApplication()
{
    setvbuf(stdout, nullptr, _IOLBF, 0);

    VulkanApplication app;

    initializeVulkan(app);

    glfwSetWindowUserPointer(app.window, reinterpret_cast<void *>(&app));
    glfwSetCursorPosCallback(app.window, onCursorPosChanged);

    glfwSetFramebufferSizeCallback(app.window, framebufferResizeCallback);

    app.pipelineDrawOrder[0] = PipelineType::PrimativeShapes;
    app.pipelineDrawOrder[1] = PipelineType::Texture;

    app.pipelines[PipelineType::Texture] = {};
    VulkanApplicationPipeline& texturesPipeline = app.pipelines[PipelineType::Texture];

{   // BEGIN `texturesPipeline` CREATION

    std::vector<VkDescriptorSetLayoutBinding> descriptorSetLayoutBindings;

    VkDescriptorSetLayoutBinding samplerLayoutBinding = {};
    samplerLayoutBinding.binding = 0;
    samplerLayoutBinding.descriptorCount = 1;
    samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    samplerLayoutBinding.pImmutableSamplers = nullptr;
    samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    descriptorSetLayoutBindings.push_back(samplerLayoutBinding);

    GenericGraphicsPipelineSetup textureGraphicsPipelineCreateInfo;

    textureGraphicsPipelineCreateInfo.vertexShaderPath = "shaders/vert.spv";
    textureGraphicsPipelineCreateInfo.fragmentShaderPath = "shaders/frag.spv";
    textureGraphicsPipelineCreateInfo.device = app.device;
    textureGraphicsPipelineCreateInfo.swapChainImageFormat = app.swapChainImageFormat;
    textureGraphicsPipelineCreateInfo.vertexBindingDescription = Vertex::getBindingDescription();
    textureGraphicsPipelineCreateInfo.vertexAttributeDescriptions = Vertex::getAttributeDescriptions();
    textureGraphicsPipelineCreateInfo.swapChainExtent = app.swapChainExtent;
    textureGraphicsPipelineCreateInfo.descriptorSetLayoutBindings = descriptorSetLayoutBindings;
    textureGraphicsPipelineCreateInfo.swapChainSize = static_cast<uint8_t>(app.swapChainImages.size());
    textureGraphicsPipelineCreateInfo.swapChainImageViews = app.swapChainImageViews;

    GenericGraphicsPipelineTargets texturesPipelineSetup
    {
        & texturesPipeline.graphicsPipeline,
        & texturesPipeline.pipelineLayout,
        & texturesPipeline.renderPass,
        & texturesPipeline.descriptorSetLayout,
        & texturesPipeline.swapChainFramebuffers
    };

    if(! createGenericGraphicsPipeline(textureGraphicsPipelineCreateInfo, texturesPipelineSetup, texturesPipeline.setupCache, false)) {
        throw std::runtime_error("Failed to create the first pipeline");
    }

    // TODO: You may want to add some assertions around memoryTypeIndex as an invalid value doesn't trigger the validation layers or crash the application. Simply UB.
    allocateMemory(app.device, vconfig::PIPELINE_MEMORY_SIZE, 0, app.verticesMemory);
    allocateMemory(app.device, vconfig::PIPELINE_MEMORY_SIZE, 0, app.indicesMemory);

    app.allocatedVerticesMemory = vconfig::PIPELINE_MEMORY_SIZE;
    app.allocatedIndicesMemory = vconfig::PIPELINE_MEMORY_SIZE;
    app.freeVerticesMemory = vconfig::PIPELINE_MEMORY_SIZE;
    app.freeIndicesMemory = vconfig::PIPELINE_MEMORY_SIZE;

    vkMapMemory(app.device, app.verticesMemory, 0, vconfig::PIPELINE_MEMORY_SIZE, 0, reinterpret_cast<void**>(&app.mappedVerticesMemory));
    vkMapMemory(app.device, app.indicesMemory, 0, vconfig::PIPELINE_MEMORY_SIZE, 0, reinterpret_cast<void**>(&app.mappedIndicesMemory));

    app.entitySystem = {};
    app.entitySystem.verticesComponentBasePtr = app.mappedVerticesMemory;

    texturesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)] = { 0, vconfig::PIPELINE_MEMORY_SIZE / 2 };
    texturesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::INDICES_BUFFER)] = { 0, vconfig::PIPELINE_MEMORY_SIZE / 2 };

}   // END `texturesPipeline` CREATION

    app.pipelines[PipelineType::PrimativeShapes] = {};
    VulkanApplicationPipeline& primativeShapesPipeline = app.pipelines[PipelineType::PrimativeShapes];

{   // BEGIN `primativeShapesPipeline` CREATION

    // There aren't any descriptorSets being used for this pipeline
    std::vector<VkDescriptorSetLayoutBinding> primativeShapesPipelineDescriptorSetLayoutBindings;

    GenericGraphicsPipelineSetup graphicsPipelineCreateInfo;

    GenericGraphicsPipelineSetup primativeShapesGraphicsPipelineCreateInfo;

    primativeShapesGraphicsPipelineCreateInfo.vertexShaderPath = "shaders/simple_vert.spv";
    primativeShapesGraphicsPipelineCreateInfo.fragmentShaderPath = "shaders/simple_frag.spv";
    primativeShapesGraphicsPipelineCreateInfo.device = app.device;
    primativeShapesGraphicsPipelineCreateInfo.swapChainImageFormat = app.swapChainImageFormat;
    primativeShapesGraphicsPipelineCreateInfo.vertexBindingDescription = BasicVertex::getBindingDescription();
    primativeShapesGraphicsPipelineCreateInfo.vertexAttributeDescriptions = BasicVertex::getAttributeDescriptions();
    primativeShapesGraphicsPipelineCreateInfo.swapChainExtent = app.swapChainExtent;
    primativeShapesGraphicsPipelineCreateInfo.descriptorSetLayoutBindings = primativeShapesPipelineDescriptorSetLayoutBindings;
    primativeShapesGraphicsPipelineCreateInfo.swapChainSize = static_cast<uint8_t>(app.swapChainImages.size());
    primativeShapesGraphicsPipelineCreateInfo.swapChainImageViews = app.swapChainImageViews;

    GenericGraphicsPipelineTargets primativeShapesPipelineSetup
    {
        & primativeShapesPipeline.graphicsPipeline,
        & primativeShapesPipeline.pipelineLayout,
        & primativeShapesPipeline.renderPass,
        & primativeShapesPipeline.descriptorSetLayout,
        & primativeShapesPipeline.swapChainFramebuffers
    };

    if(! createGenericGraphicsPipeline(primativeShapesGraphicsPipelineCreateInfo, primativeShapesPipelineSetup, primativeShapesPipeline.setupCache, true)) {
        throw std::runtime_error("Failed to create the second pipeline");
    }

    primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)] = { vconfig::PIPELINE_MEMORY_SIZE / 2, vconfig::PIPELINE_MEMORY_SIZE / 2 };
    primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::INDICES_BUFFER)] = { vconfig::PIPELINE_MEMORY_SIZE / 2, vconfig::PIPELINE_MEMORY_SIZE / 2 };

}   // END `primativeShapesPipeline` CREATION

    // Create Command Pool BEGIN

    QueueFamilyIndices queueFamilyIndices = findQueueFamilies(app.physicalDevice, app.surface);

    VkCommandPoolCreateInfo commandPoolInfo = {};
    commandPoolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    commandPoolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    commandPoolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(app.device, &commandPoolInfo, nullptr, &app.commandPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics command pool!");
    }

    // Create Command Pool END

    FT_Library ft;

    if(FT_Init_FreeType(&ft)) {
        assert(false && "Failed to setup freetype");
    }

    FT_Face face;

    if(FT_New_Face(ft, vconfig::DEFAULT_FONT_PATH, 0, &face)) {
        assert(false && "Failed to load font");
    }

    FT_Select_Charmap(face, FT_ENCODING_UNICODE);

//    assert(FT_HAS_KERNING( face ));

    FT_Set_Pixel_Sizes(face, 0, 28);

    app.fontBitmap.face = face;

    std::string unique_chars = "abcdefghijklmnopqrstuvwxyz \"ABCDEFGHIJKLMNOPQRSTUVWXYZ.<>!?";
    uint16_t cellSize = 40; // TODO: No assert for when this gets too small
    uint16_t cellsPerLine = 10;

    instanciateFontBitmap(app.fontBitmap, face, unique_chars.c_str(), cellsPerLine, cellSize );

    FT_Done_Face(face);
    FT_Done_FreeType(ft);

//    // TODO: Part of pipeline setup??
    createTextureImage( app.device,
                        app.physicalDevice,
                        app.commandPool,
                        app.graphicsQueue,
                        reinterpret_cast<uint8_t *>(app.fontBitmap.bitmap_data),
                        app.fontBitmap.texture_width,
                        app.fontBitmap.texture_height,
                        texturesPipeline.textureImage,
                        texturesPipeline.textureImageMemory);

    createImageView(app.device, texturesPipeline.textureImage, VK_FORMAT_R8G8B8A8_UNORM, texturesPipeline.textureImageView);

    // Create Texture Sampler BEGIN
    VkSamplerCreateInfo samplerInfo = {};

    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.anisotropyEnable = VK_TRUE;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = VK_FALSE;
    samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    if (vkCreateSampler(app.device, &samplerInfo, nullptr, &texturesPipeline.textureSampler) != VK_SUCCESS) {
        throw std::runtime_error("failed to create texture sampler!");
    }

    // Create Texture Sampler END

    createBufferOnMemory(   app.device,
                            texturesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)].size,
                            texturesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)].offset,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            app.verticesMemory,
                            texturesPipeline.vertexBuffer );

    createBufferOnMemory(   app.device,
                            texturesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::INDICES_BUFFER)].size,
                            texturesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::INDICES_BUFFER)].offset,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            app.indicesMemory,
                            texturesPipeline.indexBuffer );

    createBufferOnMemory(   app.device,
                            primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)].size,
                            primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::VERTEX_BUFFER)].offset,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                            app.verticesMemory,
                            primativeShapesPipeline.vertexBuffer );

    createBufferOnMemory(   app.device,
                            primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::INDICES_BUFFER)].size,
                            primativeShapesPipeline.usageMap[static_cast<uint16_t>(MemoryUsageType::INDICES_BUFFER)].offset,
                            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                            app.indicesMemory,
                            primativeShapesPipeline.indexBuffer );

    // Create Description Pool Begin
    std::array<VkDescriptorPoolSize, 1> descriptorPoolSizes = {};

    descriptorPoolSizes[0].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorPoolSizes[0].descriptorCount = static_cast<uint32_t>(app.swapChainImages.size());

    VkDescriptorPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    poolInfo.poolSizeCount = static_cast<uint32_t>(descriptorPoolSizes.size());
    poolInfo.pPoolSizes = descriptorPoolSizes.data();
    poolInfo.maxSets = static_cast<uint32_t>(app.swapChainImages.size());

    if (vkCreateDescriptorPool(app.device, &poolInfo, nullptr, &app.descriptorPool) != VK_SUCCESS) {
        throw std::runtime_error("failed to create descriptor pool!");
    }

    // Create Description Pool END

    // Create Description Sets BEGIN
    std::vector<VkDescriptorSetLayout> layouts(app.swapChainImages.size(), texturesPipeline.descriptorSetLayout);

    VkDescriptorSetAllocateInfo descriptorAllocInfo = {};
    descriptorAllocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAllocInfo.descriptorPool = app.descriptorPool;
    descriptorAllocInfo.descriptorSetCount = static_cast<uint32_t>(app.swapChainImages.size());
    descriptorAllocInfo.pSetLayouts = layouts.data();

    texturesPipeline.descriptorSets.resize(app.swapChainImages.size());

    if (vkAllocateDescriptorSets(app.device, &descriptorAllocInfo, texturesPipeline.descriptorSets.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate descriptor sets!");
    }

    for (size_t i = 0; i < app.swapChainImages.size(); i++)
    {

        VkDescriptorImageInfo imageInfo = {};

        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = texturesPipeline.textureImageView;
        imageInfo.sampler = texturesPipeline.textureSampler;

        assert(texturesPipeline.textureImageView && "Image view = null");
        assert(texturesPipeline.textureSampler && "Sampler = null");

        std::array<VkWriteDescriptorSet, 1> descriptorWrites = {};

        descriptorWrites[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrites[0].dstSet = texturesPipeline.descriptorSets[i];
        descriptorWrites[0].dstBinding = 0;
        descriptorWrites[0].dstArrayElement = 0;
        descriptorWrites[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrites[0].descriptorCount = 1;
        descriptorWrites[0].pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(app.device, static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
    }

    // Create Description Sets END

    // Create Command Buffers BEGIN

    app.commandBuffers.resize(texturesPipeline.swapChainFramebuffers.size());

    VkCommandBufferAllocateInfo commandBufferAllocInfo = {};
    commandBufferAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandBufferAllocInfo.commandPool = app.commandPool;
    commandBufferAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandBufferAllocInfo.commandBufferCount = static_cast<uint32_t>(app.commandBuffers.size());

    if (vkAllocateCommandBuffers(app.device, &commandBufferAllocInfo, app.commandBuffers.data()) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate command buffers!");
    }

    // Create Sync Objects BEGIN

    app.imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    app.renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    app.inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo = {};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(app.device, &semaphoreInfo, nullptr, &app.imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(app.device, &semaphoreInfo, nullptr, &app.renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(app.device, &fenceInfo, nullptr, &app.inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }

    // Create Sync Objects END

    return app;
}

bool updateGenericGraphicsPipeline( VkDevice device,
                                    VkExtent2D swapchainExtent,
                                    std::vector<VkImageView>& swapChainImageViews,
                                    uint8_t swapchainSize,
                                    GenericGraphicsPipelineTargets& out,
                                    PipelineSetupData &setupCache)
{

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &setupCache.attachmentReference;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &setupCache.attachmentDescription;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &setupCache.subpassDependency;

    if (vkCreateRenderPass(device, &renderPassInfo, nullptr, out.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    if(setupCache.descriptorSetLayoutBindings.size() == 0) {
        out.descriptorSetLayout = nullptr;
    } else {

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(setupCache.descriptorSetLayoutBindings.size());
        layoutInfo.pBindings = setupCache.descriptorSetLayoutBindings.data();

        if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, out.descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    // TODO: Get rid of VertexInputInfo structure as it can be reconstructed and isn't providing any value as-is
    setupCache.vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    setupCache.vertexInputInfo.vertexBindingDescriptionCount = 1;
    setupCache.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(setupCache.vertexAttributeDescriptions.size());
    setupCache.vertexInputInfo.pVertexBindingDescriptions = &setupCache.vertexBindingDescription;
    setupCache.vertexInputInfo.pVertexAttributeDescriptions = setupCache.vertexAttributeDescriptions.data();

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainExtent.width);
    viewport.height = static_cast<float>(swapchainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = swapchainExtent;

//    VkPipelineViewportStateCreateInfo viewportState = {};
    setupCache.viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    setupCache.viewportState.viewportCount = 1;
    setupCache.viewportState.pViewports = &viewport;
    setupCache.viewportState.scissorCount = 1;
    setupCache.viewportState.pScissors = &scissor;

    setupCache.pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if(setupCache.descriptorSetLayoutBindings.size() == 0)
    {
        setupCache.pipelineLayoutInfo.setLayoutCount = 0;
        setupCache.pipelineLayoutInfo.pSetLayouts = nullptr;
    } else
    {
        setupCache.pipelineLayoutInfo.setLayoutCount = 1;
        setupCache.pipelineLayoutInfo.pSetLayouts = out.descriptorSetLayout;
    }

    if (vkCreatePipelineLayout(device, &setupCache.pipelineLayoutInfo, nullptr, out.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = setupCache.shaderStages;
    pipelineInfo.pVertexInputState = &setupCache.vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &setupCache.inputAssembly;
    pipelineInfo.pViewportState = &setupCache.viewportState;
    pipelineInfo.pRasterizationState = &setupCache.rasterizer;
    pipelineInfo.pMultisampleState = &setupCache.multisampling;
    pipelineInfo.pColorBlendState = &setupCache.colorBlending;
    pipelineInfo.layout = *out.pipelineLayout;
    pipelineInfo.renderPass = *out.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, out.graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    out.swapChainFramebuffers->resize(swapchainSize);

    createGenericFrameBuffers(  *out.renderPass,
                                device,
                                swapChainImageViews,
                                *out.swapChainFramebuffers,
                                swapchainExtent );

    return true;
}

// TODO: clearFirst is a bit of a hack. Should come up with something more flexible
bool createGenericGraphicsPipeline(const GenericGraphicsPipelineSetup& params, GenericGraphicsPipelineTargets& out, PipelineSetupData &outSetup, bool clearFirst)
{

    outSetup.descriptorSetLayoutBindings = params.descriptorSetLayoutBindings;

    VkAttachmentDescription colorAttachment = {};
    colorAttachment.format = params.swapChainImageFormat;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = (clearFirst) ? VK_ATTACHMENT_LOAD_OP_CLEAR : VK_ATTACHMENT_LOAD_OP_LOAD;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = (clearFirst) ? VK_IMAGE_LAYOUT_UNDEFINED : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    outSetup.attachmentDescription = colorAttachment;

    VkAttachmentReference colorAttachmentRef = {};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    outSetup.attachmentReference = colorAttachmentRef;

    VkSubpassDescription subpass = {};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency = {};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    outSetup.subpassDependency = dependency;

    VkRenderPassCreateInfo renderPassInfo = {};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(params.device, &renderPassInfo, nullptr, out.renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }

    if(outSetup.descriptorSetLayoutBindings.size() == 0) {
        out.descriptorSetLayout = nullptr;
    } else {

        VkDescriptorSetLayoutCreateInfo layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(outSetup.descriptorSetLayoutBindings.size());
        layoutInfo.pBindings = outSetup.descriptorSetLayoutBindings.data();

        if (vkCreateDescriptorSetLayout(params.device, &layoutInfo, nullptr, out.descriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    auto vertShaderCode = readFile(params.vertexShaderPath.c_str());
    auto fragShaderCode = readFile(params.fragmentShaderPath.c_str());

    VkShaderModule vertShaderModule = createShaderModule(params.device, vertShaderCode);
    VkShaderModule fragShaderModule = createShaderModule(params.device, fragShaderCode);

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

    outSetup.shaderStages[0] = vertShaderStageInfo;
    outSetup.shaderStages[1] = fragShaderStageInfo;

    outSetup.vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

    outSetup.vertexBindingDescription = params.vertexBindingDescription;
    outSetup.vertexAttributeDescriptions = params.vertexAttributeDescriptions;

    outSetup.vertexInputInfo.vertexBindingDescriptionCount = 1;
    outSetup.vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(outSetup.vertexAttributeDescriptions.size());
    outSetup.vertexInputInfo.pVertexBindingDescriptions = &outSetup.vertexBindingDescription;
    outSetup.vertexInputInfo.pVertexAttributeDescriptions = outSetup.vertexAttributeDescriptions.data();

//    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
    outSetup.inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    outSetup.inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    outSetup.inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport = {};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(params.swapChainExtent.width);
    viewport.height = static_cast<float>(params.swapChainExtent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor = {};
    scissor.offset = {0, 0};
    scissor.extent = params.swapChainExtent;

//    VkPipelineViewportStateCreateInfo viewportState = {};
    outSetup.viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    outSetup.viewportState.viewportCount = 1;
    outSetup.viewportState.pViewports = &viewport;
    outSetup.viewportState.scissorCount = 1;
    outSetup.viewportState.pScissors = &scissor;

//    VkPipelineRasterizationStateCreateInfo rasterizer = {};
    outSetup.rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    outSetup.rasterizer.depthClampEnable = VK_FALSE;
    outSetup.rasterizer.rasterizerDiscardEnable = VK_FALSE;
    outSetup.rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    outSetup.rasterizer.lineWidth = 1.0f;
    outSetup.rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
    outSetup.rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
    outSetup.rasterizer.depthBiasEnable = VK_FALSE;

//    VkPipelineMultisampleStateCreateInfo multisampling = {};
    outSetup.multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    outSetup.multisampling.sampleShadingEnable = VK_FALSE;
    outSetup.multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

//    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
    outSetup.colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

    outSetup.colorBlendAttachment.blendEnable = VK_TRUE;
    outSetup.colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    outSetup.colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;

    outSetup.colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    outSetup.colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;

    outSetup.colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    outSetup.colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;

//    VkPipelineColorBlendStateCreateInfo colorBlending = {};
    outSetup.colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    outSetup.colorBlending.logicOpEnable = VK_FALSE;
    outSetup.colorBlending.logicOp = VK_LOGIC_OP_COPY;
    outSetup.colorBlending.attachmentCount = 1;
    outSetup.colorBlending.pAttachments = &outSetup.colorBlendAttachment;
    outSetup.colorBlending.blendConstants[0] = 0.0f;
    outSetup.colorBlending.blendConstants[1] = 0.0f;
    outSetup.colorBlending.blendConstants[2] = 0.0f;
    outSetup.colorBlending.blendConstants[3] = 0.0f;

//    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
    outSetup.pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;

    if(outSetup.descriptorSetLayoutBindings.size() == 0)
    {
        outSetup.pipelineLayoutInfo.setLayoutCount = 0;
        outSetup.pipelineLayoutInfo.pSetLayouts = nullptr;
    } else
    {
        outSetup.pipelineLayoutInfo.setLayoutCount = 1;
        outSetup.pipelineLayoutInfo.pSetLayouts = out.descriptorSetLayout;
    }

    if (vkCreatePipelineLayout(params.device, &outSetup.pipelineLayoutInfo, nullptr, out.pipelineLayout) != VK_SUCCESS) {
        throw std::runtime_error("failed to create pipeline layout!");
    }

    VkGraphicsPipelineCreateInfo pipelineInfo = {};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = outSetup.shaderStages;
    pipelineInfo.pVertexInputState = &outSetup.vertexInputInfo;
    pipelineInfo.pInputAssemblyState = &outSetup.inputAssembly;
    pipelineInfo.pViewportState = &outSetup.viewportState;
    pipelineInfo.pRasterizationState = &outSetup.rasterizer;
    pipelineInfo.pMultisampleState = &outSetup.multisampling;
    pipelineInfo.pColorBlendState = &outSetup.colorBlending;
    pipelineInfo.layout = *out.pipelineLayout;
    pipelineInfo.renderPass = *out.renderPass;
    pipelineInfo.subpass = 0;
    pipelineInfo.basePipelineHandle = VK_NULL_HANDLE;

    if (vkCreateGraphicsPipelines(params.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, out.graphicsPipeline) != VK_SUCCESS) {
        throw std::runtime_error("failed to create graphics pipeline!");
    }

    out.swapChainFramebuffers->resize(params.swapChainSize);

    createGenericFrameBuffers(  *out.renderPass,
                                params.device,
                                params.swapChainImageViews,
                                *out.swapChainFramebuffers,
                                params.swapChainExtent );

    return true;
}
