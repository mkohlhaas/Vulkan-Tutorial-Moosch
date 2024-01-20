// TODO: write error handling function
// TODO: separate source files; list all used vk functions; use header files and static functions
// TODO; remove rateDeviceSuitability(); we have only one GPU card

#include <limits.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

const char *WIN_TITLE = "SeEngine";
const uint32_t WIN_WIDTH = 800;
const uint32_t WIN_HEIGHT = 600;

const int MAX_FRAMES_IN_FLIGHT = 2;

uint32_t currentFrame = 0;
bool framebufferResized = false;

const bool isEnabledValidationLayers = true;
const uint32_t validationLayerCount = 1;
const char *validationLayers[] = {"VK_LAYER_KHRONOS_validation"};
const uint32_t deviceExtensionCount = 1;
const char *deviceExtensions[] = {VK_KHR_SWAPCHAIN_EXTENSION_NAME};

typedef struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  uint32_t formatCount;
  VkSurfaceFormatKHR *formats;
  uint32_t presentModeCount;
  VkPresentModeKHR *presentModes;
} SwapChainSupportDetails;

typedef struct QueueFamilyIndices {
  uint32_t graphicsFamily;
  bool isGraphicsFamily;
  uint32_t surfaceFamily;
  bool isSurfaceFamily;
} QueueFamilyIndices;

typedef struct App {
  GLFWwindow *window;
  VkInstance instance;
  VkDebugUtilsMessengerEXT debugMessenger;
  VkSurfaceKHR surface;
  VkPhysicalDevice physicalDevice;
  QueueFamilyIndices queueFamilyIndices;
  VkDevice device; // Logical device
  VkQueue graphicsQueue;
  VkQueue presentQueue;
  VkSwapchainKHR swapChain;
  uint32_t swapChainImageCount;
  VkImage *swapChainImages;
  VkFormat swapChainImageFormat;
  VkExtent2D swapChainExtent;
  VkImageView *swapChainImageViews;
  VkRenderPass renderPass;
  VkPipelineLayout pipelineLayout;
  VkPipeline graphicsPipeline;
  VkFramebuffer *swapChainFramebuffers;
  VkCommandPool commandPool;
  VkCommandBuffer *commandBuffers;
  VkSemaphore *imageAvailableSemaphores;
  VkSemaphore *renderFinishedSemaphores;
  VkFence *inFlightFences;
} App;

static void key_callback(GLFWwindow *window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    glfwSetWindowShouldClose(window, GLFW_TRUE);
}

void cleanupSwapChain(App *pApp) {
  for (uint32_t i = 0; i < pApp->swapChainImageCount; i++) {
    vkDestroyFramebuffer(pApp->device, pApp->swapChainFramebuffers[i], NULL);
  }

  for (uint32_t i = 0; i < pApp->swapChainImageCount; i++) {
    vkDestroyImageView(pApp->device, pApp->swapChainImageViews[i], NULL);
  }

  vkDestroySwapchainKHR(pApp->device, pApp->swapChain, NULL);
}

SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);
  details.formatCount = formatCount;
  VkSurfaceFormatKHR formats[formatCount];
  details.formats = formats;
  if (formatCount != 0) {
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats);
  }

  uint32_t presentCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, NULL);
  details.presentModeCount = presentCount;
  VkPresentModeKHR presentModes[presentCount];
  details.presentModes = presentModes;
  if (presentCount != 0) {
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentCount, details.presentModes);
  }

  return details;
}

VkSurfaceFormatKHR chooseSwapSurfaceFormat(uint32_t formatCount, VkSurfaceFormatKHR *availableFormats) {
  for (uint32_t i = 0; i < formatCount; i++) {
    if (availableFormats[i].format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormats[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormats[i];
    }
  }
  return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(uint32_t presentModeCount, VkPresentModeKHR *availablePresentModes) {
  for (uint32_t i = 0; i < presentModeCount; i++) {
    if (availablePresentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentModes[i];
    }
  }
  return VK_PRESENT_MODE_FIFO_KHR;
}

uint32_t clamp(uint32_t n, uint32_t min, uint32_t max) {
  return n < min ? min : n > max ? max : n;
}

VkExtent2D chooseSwapExtent(GLFWwindow *window, VkSurfaceCapabilitiesKHR capabilities) {
  if (capabilities.currentExtent.width != UINT_MAX) {
    return capabilities.currentExtent;
  } else {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);

    VkExtent2D actualExtent = {(uint32_t)width, (uint32_t)height};

    actualExtent.width =
        clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height =
        clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
  }
}

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
  QueueFamilyIndices indices = {};

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);

  VkQueueFamilyProperties queueFamilyProperties[queueFamilyCount];
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties);

  for (uint32_t i = 0; i < queueFamilyCount; i++) {
    if (queueFamilyProperties[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
      indices.isGraphicsFamily = true;
      break;
    }
    VkBool32 surfaceSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &surfaceSupport);
    if (surfaceSupport) {
      indices.surfaceFamily = i;
      indices.isSurfaceFamily = true;
    }
  }

  return indices;
}

void createSwapChain(App *pApp) {
  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(pApp->physicalDevice, pApp->surface);

  VkSurfaceFormatKHR surfaceFormat =
      chooseSwapSurfaceFormat(swapChainSupport.formatCount, swapChainSupport.formats);
  VkPresentModeKHR presentMode =
      chooseSwapPresentMode(swapChainSupport.presentModeCount, swapChainSupport.presentModes);
  VkExtent2D extent = chooseSwapExtent(pApp->window, swapChainSupport.capabilities);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo = {.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
                                         .surface = pApp->surface,
                                         .minImageCount = imageCount,
                                         .imageFormat = surfaceFormat.format,
                                         .imageColorSpace = surfaceFormat.colorSpace,
                                         .imageExtent = extent,
                                         .imageArrayLayers = 1,
                                         .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT};

  QueueFamilyIndices indices = findQueueFamilies(pApp->physicalDevice, pApp->surface);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.surfaceFamily};

  if (indices.graphicsFamily != indices.surfaceFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.queueFamilyIndexCount = 0;  // Optional
    createInfo.pQueueFamilyIndices = NULL; // Optional
  }

  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(pApp->device, &createInfo, NULL, &pApp->swapChain) != VK_SUCCESS) {
    fprintf(stderr, "Failed to create Swap Chain!\n");
    exit(EXIT_FAILURE);
  }

  vkGetSwapchainImagesKHR(pApp->device, pApp->swapChain, &imageCount, NULL);
  pApp->swapChainImages = malloc(sizeof(VkImage) * imageCount);

  vkGetSwapchainImagesKHR(pApp->device, pApp->swapChain, &imageCount, pApp->swapChainImages);
  pApp->swapChainImageCount = imageCount;

  pApp->swapChainImageFormat = surfaceFormat.format;
  pApp->swapChainExtent = extent;
}

void createImageViews(App *pApp) {
  pApp->swapChainImageViews = malloc(sizeof(VkImageView) * pApp->swapChainImageCount);

  for (uint32_t i = 0; i < pApp->swapChainImageCount; i++) {
    VkImageViewCreateInfo createInfo = {.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                                        .image = pApp->swapChainImages[i],
                                        .viewType = VK_IMAGE_VIEW_TYPE_2D,
                                        .format = pApp->swapChainImageFormat,
                                        .components.r = VK_COMPONENT_SWIZZLE_IDENTITY,
                                        .components.g = VK_COMPONENT_SWIZZLE_IDENTITY,
                                        .components.b = VK_COMPONENT_SWIZZLE_IDENTITY,
                                        .components.a = VK_COMPONENT_SWIZZLE_IDENTITY,
                                        .subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                                        .subresourceRange.baseMipLevel = 0,
                                        .subresourceRange.levelCount = 1,
                                        .subresourceRange.baseArrayLayer = 0,
                                        .subresourceRange.layerCount = 1};

    if (vkCreateImageView(pApp->device, &createInfo, NULL, &pApp->swapChainImageViews[i]) != VK_SUCCESS) {
      fprintf(stderr, "Failed to create image views!\n");
      exit(EXIT_FAILURE);
    }
  }
}

void createFramebuffers(App *pApp) {
  pApp->swapChainFramebuffers = malloc(pApp->swapChainImageCount * sizeof(VkFramebuffer));

  for (uint32_t i = 0; i < pApp->swapChainImageCount; i++) {
    VkImageView attachments[] = {pApp->swapChainImageViews[i]};

    VkFramebufferCreateInfo framebufferInfo = {};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = pApp->renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = pApp->swapChainExtent.width;
    framebufferInfo.height = pApp->swapChainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(pApp->device, &framebufferInfo, NULL, &pApp->swapChainFramebuffers[i]) !=
        VK_SUCCESS) {
      fprintf(stderr, "failed to create framebuffer!\n");
      exit(EXIT_FAILURE);
    }
  }
}

void recreateSwapChain(App *pApp) {
  int width = 0, height = 0;
  glfwGetFramebufferSize(pApp->window, &width, &height);
  while (width == 0 || height == 0) {
    glfwGetFramebufferSize(pApp->window, &width, &height);
    glfwWaitEvents();
  }

  vkDeviceWaitIdle(pApp->device);

  cleanupSwapChain(pApp);

  createSwapChain(pApp);
  createImageViews(pApp);
  createFramebuffers(pApp);
}

void recordCommandBuffer(App *pApp, VkCommandBuffer commandBuffer, uint32_t imageIndex) {
  VkCommandBufferBeginInfo beginInfo = {};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = 0;               // Optional
  beginInfo.pInheritanceInfo = NULL; // Optional

  if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
    fprintf(stderr, "failed to begin recording command buffer!\n");
    exit(EXIT_FAILURE);
  }

  VkRenderPassBeginInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
  renderPassInfo.renderPass = pApp->renderPass;
  renderPassInfo.framebuffer = pApp->swapChainFramebuffers[imageIndex];
  renderPassInfo.renderArea.offset.x = 0;
  renderPassInfo.renderArea.offset.y = 0;
  renderPassInfo.renderArea.extent = pApp->swapChainExtent;

  VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
  renderPassInfo.clearValueCount = 1;
  renderPassInfo.pClearValues = &clearColor;

  vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

  vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pApp->graphicsPipeline);

  VkViewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float)pApp->swapChainExtent.width;
  viewport.height = (float)pApp->swapChainExtent.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;
  vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

  VkRect2D scissor = {};
  scissor.offset.x = 0;
  scissor.offset.y = 0;
  scissor.extent = pApp->swapChainExtent;
  vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

  vkCmdDraw(commandBuffer, 3, 1, 0, 0);

  vkCmdEndRenderPass(commandBuffer);

  if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
    fprintf(stderr, "failed to record command buffer!\n");
    exit(EXIT_FAILURE);
  }
}

void drawFrame(App *pApp) {
  vkWaitForFences(pApp->device, 1, &pApp->inFlightFences[currentFrame], VK_TRUE, UINT64_MAX);

  vkResetFences(pApp->device, 1, &pApp->inFlightFences[currentFrame]);

  uint32_t imageIndex;
  VkResult result =
      vkAcquireNextImageKHR(pApp->device, pApp->swapChain, UINT64_MAX,
                            pApp->imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);

  if (result == VK_ERROR_OUT_OF_DATE_KHR) {
    recreateSwapChain(pApp);
    return;
  } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
    fprintf(stderr, "Failed to acquire swap chain image!\n");
    exit(EXIT_FAILURE);
  }

  // Only reset the fence if we are submitting work
  vkResetFences(pApp->device, 1, &pApp->inFlightFences[currentFrame]);

  vkResetCommandBuffer(pApp->commandBuffers[currentFrame], 0);
  recordCommandBuffer(pApp, pApp->commandBuffers[currentFrame], imageIndex);

  VkSubmitInfo submitInfo = {};
  submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  VkSemaphore waitSemaphores[] = {pApp->imageAvailableSemaphores[currentFrame]};
  VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  submitInfo.waitSemaphoreCount = 1;
  submitInfo.pWaitSemaphores = waitSemaphores;
  submitInfo.pWaitDstStageMask = waitStages;

  submitInfo.commandBufferCount = 1;
  submitInfo.pCommandBuffers = &pApp->commandBuffers[currentFrame];

  VkSemaphore signalSemaphores[] = {pApp->renderFinishedSemaphores[currentFrame]};
  submitInfo.signalSemaphoreCount = 1;
  submitInfo.pSignalSemaphores = signalSemaphores;

  if (vkQueueSubmit(pApp->graphicsQueue, 1, &submitInfo, pApp->inFlightFences[currentFrame]) != VK_SUCCESS) {
    fprintf(stderr, "Failed to submit draw command buffer!\n");
    exit(EXIT_FAILURE);
  }

  VkPresentInfoKHR presentInfo = {};
  presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

  presentInfo.waitSemaphoreCount = 1;
  presentInfo.pWaitSemaphores = signalSemaphores;

  VkSwapchainKHR swapChains[] = {pApp->swapChain};
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = swapChains;
  presentInfo.pImageIndices = &imageIndex;

  presentInfo.pResults = NULL; // Optional

  VkResult queueResult = vkQueuePresentKHR(pApp->presentQueue, &presentInfo);

  if (queueResult == VK_ERROR_OUT_OF_DATE_KHR || queueResult == VK_SUBOPTIMAL_KHR || framebufferResized) {
    framebufferResized = false;
    recreateSwapChain(pApp);
  } else if (queueResult != VK_SUCCESS) {
    fprintf(stderr, "Failed to present swap chain image!\n");
    exit(EXIT_FAILURE);
  }

  currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
  PFN_vkDestroyDebugUtilsMessengerEXT fn =
      (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
  if (fn) {
    fn(instance, debugMessenger, pAllocator);
  }
}

void framebufferResizeCallback(GLFWwindow *window, int width, int height) {
  // App *app = glfwGetWindowUserPointer(window);
  framebufferResized = true;
}

void initWindow(App *pApp) {
  glfwInit();

  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

  pApp->window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, WIN_TITLE, NULL, NULL);
  // glfwSetWindowUserPointer(pApp->window, pApp);
  glfwSetFramebufferSizeCallback(pApp->window, framebufferResizeCallback);
  glfwSetKeyCallback(pApp->window, key_callback);
}

void mainLoop(App *pApp) {
  while (!glfwWindowShouldClose(pApp->window)) {
    glfwPollEvents();
    drawFrame(pApp);
  }

  vkDeviceWaitIdle(pApp->device);
}

void cleanup(App *pApp) {
  cleanupSwapChain(pApp);

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(pApp->device, pApp->imageAvailableSemaphores[i], NULL);
    vkDestroySemaphore(pApp->device, pApp->renderFinishedSemaphores[i], NULL);
    vkDestroyFence(pApp->device, pApp->inFlightFences[i], NULL);
  }

  vkDestroyCommandPool(pApp->device, pApp->commandPool, NULL);

  vkDestroyPipeline(pApp->device, pApp->graphicsPipeline, NULL);
  vkDestroyPipelineLayout(pApp->device, pApp->pipelineLayout, NULL);
  vkDestroyRenderPass(pApp->device, pApp->renderPass, NULL);

  DestroyDebugUtilsMessengerEXT(pApp->instance, pApp->debugMessenger, NULL);

  vkDestroyDevice(pApp->device, NULL);

  vkDestroySurfaceKHR(pApp->instance, pApp->surface, NULL);
  vkDestroyInstance(pApp->instance, NULL);

  glfwDestroyWindow(pApp->window);

  glfwTerminate();
}

bool verifyExtensionSupport(uint32_t extensionCount, VkExtensionProperties *extensions,
                            uint32_t glfwExtensionCount, const char **glfwExtensions) {
  for (uint32_t i = 0; i < glfwExtensionCount; i++) {
    bool extensionFound = false;
    for (uint32_t j = 0; j < extensionCount; j++) {
      if (strcmp(extensions[j].extensionName, glfwExtensions[i]) == 0) {
        extensionFound = true;
        break;
      }
    }
    if (!extensionFound) {
      return false;
    }
  }

  return true;
}

bool checkValidationLayerSupport() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, NULL);

  VkLayerProperties availableLayers[layerCount];
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers);

  for (uint32_t i = 0; i < validationLayerCount; i++) {
    bool layerFound = false;
    for (uint32_t j = 0; j < layerCount; j++) {
      if (strcmp(availableLayers[j].layerName, validationLayers[i]) == 0) {
        layerFound = true;
        break;
      }
    }
    if (!layerFound) {
      return false;
    }
  }

  return true;
}

static VkBool32 debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                              VkDebugUtilsMessageTypeFlagsEXT messageType,
                              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData) {

  fprintf(stderr, "Validation layer: %s\n", pCallbackData->pMessage);
  return VK_FALSE;
}

void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT *createInfo) {
  createInfo->sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo->messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo->messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo->pfnUserCallback = debugCallback;
}

void createInstance(App *pApp) {
  if (isEnabledValidationLayers && !checkValidationLayerSupport()) {
    fprintf(stderr, "Validation layers requested but not available!\n");
    exit(1);
  }

  VkApplicationInfo appInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = WIN_TITLE,
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  uint32_t glfwExtensionCount;
  const char **glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

  const char *glfwExtensionsWithDebug[glfwExtensionCount + 1];
  for (uint32_t i = 0; i < glfwExtensionCount; i++) {
    glfwExtensionsWithDebug[i] = glfwExtensions[i];
  }
  if (isEnabledValidationLayers) {
    glfwExtensionsWithDebug[glfwExtensionCount] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
  }

  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo = {};
  VkInstanceCreateInfo createInfo = {.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
                                     .pApplicationInfo = &appInfo};
  // TODO: this is for backward compatiblility; remove it!
  if (isEnabledValidationLayers) {
    createInfo.enabledLayerCount = validationLayerCount;
    createInfo.ppEnabledLayerNames = validationLayers;
    createInfo.enabledExtensionCount = glfwExtensionCount + 1;
    createInfo.ppEnabledExtensionNames = glfwExtensionsWithDebug;

    populateDebugMessengerCreateInfo(&debugCreateInfo);
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.pNext = NULL;
  }

  if (vkCreateInstance(&createInfo, NULL, &pApp->instance)) {
    fprintf(stderr, "Failed to create Vulkan Instance!\n");
    exit(EXIT_FAILURE);
  }

  // Vulkan extensions
  uint32_t extensionCount = 0;
  vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, NULL);

  VkExtensionProperties extensions[extensionCount];
  vkEnumerateInstanceExtensionProperties(NULL, &extensionCount, extensions);

  if (!verifyExtensionSupport(extensionCount, extensions, glfwExtensionCount, glfwExtensions)) {
    fprintf(stderr, "Missing extension support!\n");
    exit(EXIT_FAILURE);
  }
}

void createSurface(App *pApp) {
  if (glfwCreateWindowSurface(pApp->instance, pApp->window, NULL, &pApp->surface) != VK_SUCCESS) {
    fprintf(stderr, "Failed to create window surface!\n");
    exit(EXIT_FAILURE);
  }
}

bool checkDeviceExtensionSupport(VkPhysicalDevice device) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
  VkExtensionProperties availableExtensions[extensionCount];
  vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions);

  for (uint32_t i = 0; i < deviceExtensionCount; i++) {
    bool extensionFound = false;
    for (uint32_t j = 0; j < extensionCount; j++) {
      if (strcmp(deviceExtensions[i], availableExtensions[j].extensionName) == 0) {
        extensionFound = true;
        break;
      }
    }
    if (!extensionFound) {
      return false;
    }
  }

  return true;
}

uint32_t rateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface) {
  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceProperties(device, &deviceProperties);
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

  uint32_t score = 0;

  // Discrete GPUs have a significant performance advantage
  if (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 1000;
  }

  // Maximum possible size of textures affects graphics quality
  score += deviceProperties.limits.maxImageDimension2D;

  // Applications can't function without geometry shaders
  if (!deviceFeatures.geometryShader) {
    return 0;
  }

  // Check device supports required queue families
  // Note: to improve performance, we could favour queue families that have both
  // graphcs and present support. We could check the returned indices and if
  // they are the same, increase the score.
  QueueFamilyIndices indices = findQueueFamilies(device, surface);
  if (!indices.isGraphicsFamily) {
    fprintf(stderr, "Queue Family not supported!\n");
    return 0;
  }

  bool extensionsSupported = checkDeviceExtensionSupport(device);
  if (!extensionsSupported) {
    fprintf(stderr, "Required device extensions not supported!\n");
    return 0;
  }

  SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device, surface);
  if (swapChainSupport.formatCount == 0 || swapChainSupport.presentModeCount == 0) {
    fprintf(stderr, "Swap chain not adequately supported!\n");
    return 0;
  }

  return score;
}

void pickPhysicalDevice(App *pApp) {
  uint32_t numDevices;
  vkEnumeratePhysicalDevices(pApp->instance, &numDevices, NULL);

  if (!numDevices) {
    fprintf(stderr, "Failed to find a GPU with Vulkan support!\n");
    exit(EXIT_FAILURE);
  }
  fprintf(stderr, "Number of Vulkan devices: %d\n", numDevices);

  VkPhysicalDevice devices[numDevices];
  vkEnumeratePhysicalDevices(pApp->instance, &numDevices, devices);

  VkPhysicalDevice device;
  uint32_t deviceScore = 0;
  for (uint32_t i = 0; i < numDevices; i++) {
    uint32_t score = rateDeviceSuitability(devices[i], pApp->surface);
    if (score > deviceScore) {
      deviceScore = score;
      device = devices[i];
    }
  }

  if (device == NULL) {
    fprintf(stderr, "Failed to find a stuitable GPU!\n");
    exit(EXIT_FAILURE);
  }
  pApp->physicalDevice = device;
  fprintf(stderr, "GPU selected.\n");

  pApp->queueFamilyIndices = findQueueFamilies(device, pApp->surface);
}

void getFamilyDeviceQueues(VkDeviceQueueCreateInfo *queues, QueueFamilyIndices indices) {
  // GraphicsQueue
  VkDeviceQueueCreateInfo graphicsQueueCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                     .queueFamilyIndex = indices.graphicsFamily,
                                                     .queueCount = 1};

  float graphicsQueuePriority = 1.0f;
  graphicsQueueCreateInfo.pQueuePriorities = &graphicsQueuePriority;
  queues[0] = graphicsQueueCreateInfo;

  // PresentQueue
  VkDeviceQueueCreateInfo presentQueueCreateInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
                                                    .queueFamilyIndex = indices.surfaceFamily,
                                                    .queueCount = 1};

  float presentQueuePriority = 1.0f;
  graphicsQueueCreateInfo.pQueuePriorities = &presentQueuePriority;
  queues[1] = presentQueueCreateInfo;
}

void createLogicalDevice(App *pApp) {
  QueueFamilyIndices indices = findQueueFamilies(pApp->physicalDevice, pApp->surface);

  VkPhysicalDeviceFeatures deviceFeatures;
  vkGetPhysicalDeviceFeatures(pApp->physicalDevice, &deviceFeatures);

  VkDeviceQueueCreateInfo queues[2];
  getFamilyDeviceQueues(queues, indices);

  VkDeviceCreateInfo createInfo = {.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
                                   //.pQueueCreateInfos = &queueCreateInfo,
                                   .pQueueCreateInfos = queues,
                                   .queueCreateInfoCount = 1,
                                   .pEnabledFeatures = &deviceFeatures,
                                   .enabledExtensionCount = deviceExtensionCount,
                                   .ppEnabledExtensionNames = deviceExtensions};

  if (isEnabledValidationLayers) {
    createInfo.enabledLayerCount = validationLayerCount;
    createInfo.ppEnabledLayerNames = validationLayers;
  } else {
    createInfo.enabledLayerCount = 0;
  }

  if (vkCreateDevice(pApp->physicalDevice, &createInfo, NULL, &pApp->device) != VK_SUCCESS) {
    fprintf(stderr, "Failed to create logical device!\n");
    exit(EXIT_FAILURE);
  }

  vkGetDeviceQueue(pApp->device, pApp->queueFamilyIndices.graphicsFamily, 0, &pApp->graphicsQueue);
  vkGetDeviceQueue(pApp->device, pApp->queueFamilyIndices.surfaceFamily, 0, &pApp->presentQueue);
}

typedef struct ShaderFile {
  size_t size;
  char *code;
} ShaderFile;

VkShaderModule createShaderModule(App *pApp, ShaderFile *shaderFile) {
  VkShaderModuleCreateInfo createInfo = {.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                                         .codeSize = shaderFile->size,
                                         .pCode = (uint32_t *)shaderFile->code};

  VkShaderModule shaderModule;
  if (vkCreateShaderModule(pApp->device, &createInfo, NULL, &shaderModule) != VK_SUCCESS) {
    fprintf(stderr, "failed to create shader module!\n");
    exit(EXIT_FAILURE);
  }

  return shaderModule;
}

void readFile(const char *filename, ShaderFile *shader) {
  FILE *pFile;

  pFile = fopen(filename, "rb");
  if (pFile == NULL) {
    fprintf(stderr, "Failed to open %s\n", filename);
    exit(EXIT_FAILURE);
  }

  fseek(pFile, 0L, SEEK_END);
  shader->size = ftell(pFile);

  fseek(pFile, 0L, SEEK_SET);

  shader->code = malloc(sizeof(char) * shader->size);
  size_t readCount = fread(shader->code, shader->size, sizeof(char), pFile);
  fprintf(stderr, "ReadCount: %ld\n", readCount);

  fclose(pFile);
}

void createRenderPass(App *pApp) {
  VkAttachmentDescription colorAttachment = {};
  colorAttachment.format = pApp->swapChainImageFormat;
  colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
  colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
  colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  VkAttachmentReference colorAttachmentRef = {};
  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

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
  dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

  VkRenderPassCreateInfo renderPassInfo = {};
  renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  renderPassInfo.attachmentCount = 1;
  renderPassInfo.pAttachments = &colorAttachment;
  renderPassInfo.subpassCount = 1;
  renderPassInfo.pSubpasses = &subpass;
  renderPassInfo.dependencyCount = 1;
  renderPassInfo.pDependencies = &dependency;

  if (vkCreateRenderPass(pApp->device, &renderPassInfo, NULL, &pApp->renderPass) != VK_SUCCESS) {
    fprintf(stderr, "failed to create render pass!\n");
    exit(EXIT_FAILURE);
  }
}

void createGraphicsPipeline(App *pApp) {
  ShaderFile vertShader = {};
  ShaderFile fragShader = {};
  readFile("shaders/vert.spv", &vertShader);
  readFile("shaders/frag.spv", &fragShader);

  VkShaderModule vertShaderModule = createShaderModule(pApp, &vertShader);
  VkShaderModule fragShaderModule = createShaderModule(pApp, &fragShader);

  VkPipelineShaderStageCreateInfo vertShaderStageInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_VERTEX_BIT,
      .module = vertShaderModule,
      .pName = "main"
      //.pSpecializationInfo = NULL
  };

  VkPipelineShaderStageCreateInfo fragShaderStageInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
      .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
      .module = fragShaderModule,
      .pName = "main"};

  VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

  VkPipelineVertexInputStateCreateInfo vertexInputInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
      .vertexBindingDescriptionCount = 0,
      .pVertexBindingDescriptions = NULL, // Optional
      .vertexAttributeDescriptionCount = 0,
      .pVertexAttributeDescriptions = NULL // Optional
  };

  uint32_t dynamicStatesSize = 2;
  VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

  VkPipelineDynamicStateCreateInfo dynamicState = {.sType =
                                                       VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
                                                   .dynamicStateCount = dynamicStatesSize,
                                                   .pDynamicStates = dynamicStates};

  VkPipelineInputAssemblyStateCreateInfo inputAssembly = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
      .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
      .primitiveRestartEnable = VK_FALSE};

  VkViewport viewport = {.x = 0.0f,
                         .y = 0.0f,
                         .width = (float)pApp->swapChainExtent.width,
                         .height = (float)pApp->swapChainExtent.height,
                         .minDepth = 0.0f,
                         .maxDepth = 1.0f};

  VkRect2D scissor = {.offset = {0, 0}, .extent = pApp->swapChainExtent};

  VkPipelineViewportStateCreateInfo viewportState = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
      .viewportCount = 1,
      .pViewports = &viewport,
      .scissorCount = 1,
      .pScissors = &scissor};

  VkPipelineRasterizationStateCreateInfo rasterizer = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
      .depthClampEnable = VK_FALSE,
      .rasterizerDiscardEnable = VK_FALSE,
      .polygonMode = VK_POLYGON_MODE_FILL,
      .lineWidth = 1.0f,
      .cullMode = VK_CULL_MODE_BACK_BIT,
      .frontFace = VK_FRONT_FACE_CLOCKWISE,
      .depthBiasEnable = VK_FALSE,
      .depthBiasConstantFactor = 0.0f, // Optional
      .depthBiasClamp = 0.0f,          // Optional
      .depthBiasSlopeFactor = 0.0f     // Optional
  };

  VkPipelineMultisampleStateCreateInfo multisampling = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
      .sampleShadingEnable = VK_FALSE,
      .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
      .minSampleShading = 1.0f,          // Optional
      .pSampleMask = NULL,               // Optional
      .alphaToCoverageEnable = VK_FALSE, // Optional
      .alphaToOneEnable = VK_FALSE       // Optional
  };

  VkPipelineColorBlendAttachmentState colorBlendAttachment = {
      .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT |
                        VK_COLOR_COMPONENT_A_BIT,
      .blendEnable = VK_FALSE,
      .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,  // Optional
      .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
      .colorBlendOp = VK_BLEND_OP_ADD,             // Optional
      .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,  // Optional
      .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO, // Optional
      .alphaBlendOp = VK_BLEND_OP_ADD              // Optional
  };

  VkPipelineColorBlendStateCreateInfo colorBlending = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
      .logicOpEnable = VK_FALSE,
      .logicOp = VK_LOGIC_OP_COPY, // Optional
      .attachmentCount = 1,
      .pAttachments = &colorBlendAttachment,
      .blendConstants[0] = 0.0f, // Optional
      .blendConstants[1] = 0.0f, // Optional
      .blendConstants[2] = 0.0f, // Optional
      .blendConstants[3] = 0.0f  // Optional
  };

  VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
      .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
      .setLayoutCount = 0,         // Optional
      .pSetLayouts = NULL,         // Optional
      .pushConstantRangeCount = 0, // Optional
      .pPushConstantRanges = NULL  // Optional
  };

  if (vkCreatePipelineLayout(pApp->device, &pipelineLayoutInfo, NULL, &pApp->pipelineLayout) != VK_SUCCESS) {
    fprintf(stderr, "failed to create pipeline layout!");
    exit(EXIT_FAILURE);
  }

  VkGraphicsPipelineCreateInfo pipelineInfo = {};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.stageCount = 2;
  pipelineInfo.flags = 0;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = NULL; // Optional
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pApp->pipelineLayout;
  pipelineInfo.renderPass = pApp->renderPass;
  pipelineInfo.subpass = 0;
  pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
  pipelineInfo.basePipelineIndex = -1;              // Optional

  if (vkCreateGraphicsPipelines(pApp->device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL,
                                &pApp->graphicsPipeline) != VK_SUCCESS) {
    fprintf(stderr, "Failed to create graphics pipeline!\n");
    exit(9);
  }

  free(vertShader.code);
  free(fragShader.code);
  vkDestroyShaderModule(pApp->device, fragShaderModule, NULL);
  vkDestroyShaderModule(pApp->device, vertShaderModule, NULL);
}

void createCommandPool(App *pApp) {
  QueueFamilyIndices indices = findQueueFamilies(pApp->physicalDevice, pApp->surface);

  VkCommandPoolCreateInfo poolInfo = {};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = indices.graphicsFamily;

  if (vkCreateCommandPool(pApp->device, &poolInfo, NULL, &pApp->commandPool) != VK_SUCCESS) {
    fprintf(stderr, "failed to create command pool!\n");
    exit(EXIT_FAILURE);
  }
}

void createCommandBuffers(App *pApp) {
  pApp->commandBuffers = malloc(sizeof(VkCommandBuffer) * MAX_FRAMES_IN_FLIGHT);

  VkCommandBufferAllocateInfo allocInfo = {};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = pApp->commandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;

  if (vkAllocateCommandBuffers(pApp->device, &allocInfo, pApp->commandBuffers) != VK_SUCCESS) {
    fprintf(stderr, "failed to allocate command buffers!\n");
    exit(EXIT_FAILURE);
  }
}

void createSyncObjects(App *pApp) {
  pApp->imageAvailableSemaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
  pApp->renderFinishedSemaphores = malloc(sizeof(VkSemaphore) * MAX_FRAMES_IN_FLIGHT);
  pApp->inFlightFences = malloc(sizeof(VkFence) * MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo = {};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo = {};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(pApp->device, &semaphoreInfo, NULL, &pApp->imageAvailableSemaphores[i]) !=
        VK_SUCCESS) {
      fprintf(stderr, "Failed to create imageAvailableSemaphore!\n");
      exit(EXIT_FAILURE);
    }
    if (vkCreateSemaphore(pApp->device, &semaphoreInfo, NULL, &pApp->renderFinishedSemaphores[i]) !=
        VK_SUCCESS) {
      fprintf(stderr, "Failed to create renderFinishedSemaphore!\n");
      exit(EXIT_FAILURE);
    }
    if (vkCreateFence(pApp->device, &fenceInfo, NULL, &pApp->inFlightFences[i]) != VK_SUCCESS) {
      fprintf(stderr, "Failed to create fence!\n");
      exit(EXIT_FAILURE);
    }
  }
}

VkResult CreateDebugUtilsMessengerEXT(VkInstance instance,
                                      const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
                                      const VkAllocationCallbacks *pAllocator,
                                      VkDebugUtilsMessengerEXT *pDebugMessenger) {
  PFN_vkCreateDebugUtilsMessengerEXT fn =
      (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");

  if (fn) {
    return fn(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void setupDebugMessenger(App *pApp) {
  if (isEnabledValidationLayers) {
    VkDebugUtilsMessengerCreateInfoEXT createInfo = {};
    populateDebugMessengerCreateInfo(&createInfo);

    if (CreateDebugUtilsMessengerEXT(pApp->instance, &createInfo, NULL, &pApp->debugMessenger)) {
      fprintf(stderr, "Failed to setup debug messenger!\n");
      exit(EXIT_FAILURE);
    }
  }
}

void initVulkan(App *pApp) {
  // TODO: use this as a reference for separate source files
  createInstance(pApp);
  setupDebugMessenger(pApp);
  createSurface(pApp);
  pickPhysicalDevice(pApp);
  createLogicalDevice(pApp);
  createSwapChain(pApp);
  createImageViews(pApp);
  createRenderPass(pApp);
  createGraphicsPipeline(pApp);
  createFramebuffers(pApp);
  createCommandPool(pApp);
  createCommandBuffers(pApp);
  createSyncObjects(pApp);
}

int main(void) {
  App app = {};

  initWindow(&app);
  initVulkan(&app);
  mainLoop(&app);
  cleanup(&app);
}
