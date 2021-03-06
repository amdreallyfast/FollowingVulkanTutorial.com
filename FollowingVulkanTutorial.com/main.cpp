//#define GLFW_INCLUDE_VULKAN
//#include <GLFW/glfw3.h>
#include "vulkan_pch.h"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

// by default GLM understands angle arguments to matrix transform generation as degrees
#define GLM_FORCE_RADIANS

// Note: Vulkan's depth buffer is 0-1. By default glm::perspective(...) produces a perspective 
// transform for OpenGL, and as such the final Z range is -1 to +1. Vulkan needs 0 to +1. GLM can 
// be told to do that with this #define.
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/gtc/matrix_transform.hpp>

#include <chrono>
#include <iostream>
#include <sstream>      // for format-capable stringstream
#include <iomanip>      // for std::setfill(...) and std::setw(...)
#include <vector>
#include <array>
#include <set>          // for eliminating potentially duplicate stuff from multiple objects
#include <optional>     // for return values that may not exist
#include <algorithm>    // std::min/max
#include <unordered_map>

#include <fstream>      // for loading shader binaries
#include <streambuf>    // for loading shader binaries
#include <string>       // for loading shader binaries
#include <cerrno>       // for loading shader binaries


/*-------------------------------------------------------------------------------------------------
Description:
    Makes vertex info lookup easy.
Creator:    John Cox, 12/2018
-------------------------------------------------------------------------------------------------*/
struct Vertex {
    glm::vec3 pos;
    glm::vec3 color;
    glm::vec2 texCoord;

    // Note: Using "5" in order to demonstrate that we do not have to use 0 just because we only 
    // have one vertex buffer at this time (12/29/2019). We could have used 3, or 7, or whatever, 
    // though I think that there is a maximum limit, because saying 99 causes an error)
    static const uint32_t VERTEX_BUFFER_BINDING_LOCATION = 5;

    static VkVertexInputBindingDescription GetBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = VERTEX_BUFFER_BINDING_LOCATION;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 3> GetAttributeDescription() {
        std::array<VkVertexInputAttributeDescription, 3> attributeDescriptions;

        // "pos" hijacks the color format enum to say "two 32bit floats"
        attributeDescriptions.at(0).binding = VERTEX_BUFFER_BINDING_LOCATION;
        attributeDescriptions.at(0).location = 0;
        attributeDescriptions.at(0).format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions.at(0).offset = offsetof(Vertex, pos);

        attributeDescriptions.at(1).binding = VERTEX_BUFFER_BINDING_LOCATION;
        attributeDescriptions.at(1).location = 1;
        attributeDescriptions.at(1).format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions.at(1).offset = offsetof(Vertex, color);

        attributeDescriptions.at(2).binding = VERTEX_BUFFER_BINDING_LOCATION;
        attributeDescriptions.at(2).location = 2;
        attributeDescriptions.at(2).format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions.at(2).offset = offsetof(Vertex, texCoord);

        return attributeDescriptions;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Used to avoid the duplication of vertices when loading the model into memory.
    Creator:    John Cox, 02/2019
    ---------------------------------------------------------------------------------------------*/
    bool operator==(const Vertex &other) const {
        return pos == other.pos
            && color == other.color
            && texCoord == other.texCoord;
    }
};

/*-------------------------------------------------------------------------------------------------
Description:
    We want to avoid using duplicates, so we will be using a std::unordered_map<...> to track
    which vertices we've already used. In order to use a Vertex instance as a key though, we need
    to define a hash function for it in this way. As a consequence, this hashing *cannot* be moved
    into Vertex as a method if we still want to use the Vertex object as a key to a map.
Creator:    John Cox, 02/2019
-------------------------------------------------------------------------------------------------*/
namespace std {
    template<> struct hash<Vertex> {
        size_t operator()(Vertex const &v) const {
            return ((hash<glm::vec3>()(v.pos) ^
                (hash<glm::vec3>()(v.color) << 1)) >> 1) ^
                (hash<glm::vec2>()(v.texCoord) << 1);
        }
    };
}

/*-------------------------------------------------------------------------------------------------
Description:
    Rather than specify three separate uniforms to bring the transform matrices into the shaders,
    load them all at one time.

    ??why three separate matrices? is it just normal practice to separate them??
Creator:    John Cox, 01/2019
-------------------------------------------------------------------------------------------------*/
struct UniformBufferObject {
    glm::mat4 model;
    glm::mat4 view;
    glm::mat4 proj;
};

/*-------------------------------------------------------------------------------------------------
Description:
    A wrapper for the dynamic calling of vkCreateDebugUtilsMessengerEXT(...).
Creator:    John Cox, 10/2018
-------------------------------------------------------------------------------------------------*/
VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pCallback) {

    // Note: It seems that static_cast<...> doesn't work. Use the C-style forced cast.
    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, pCreateInfo, pAllocator, pCallback);
    }
    else {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }
}

/*-------------------------------------------------------------------------------------------------
Description:
    A wrapper for the dynamic calling of vkDestroyDebugUTilsMessengerEXT(...).
Creator:    John Cox, 10/2018
-------------------------------------------------------------------------------------------------*/
void DestroyDebugUtilsMessengEXT(
    VkInstance instance,
    VkDebugUtilsMessengerEXT callback,
    const VkAllocationCallbacks *pAllocator) {

    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
        return func(instance, callback, pAllocator);
    }
    else {
        // ignore; the "destroy" function does not return anything
    }
}

/*-------------------------------------------------------------------------------------------------
Description:
    Reads an entire file into a string and returns it.

    Source: http://insanecoding.blogspot.com/2011/11/how-to-read-in-file-in-c.html
Creator:    John Cox, 11/2018
-------------------------------------------------------------------------------------------------*/
std::string ReadFileIntoString(const std::string &file_path) {
    std::ifstream in(file_path.c_str(), std::ios::in | std::ios::binary);
    if (in) {
        return(std::string(
            std::istreambuf_iterator<char>(in),
            std::istreambuf_iterator<char>()));
    }

    std::stringstream ss;
    ss << errno;

    throw std::runtime_error(ss.str());
}

/*-------------------------------------------------------------------------------------------------
Description:
    The class for this tutorial series.
Creator:    John Cox, 10/2018
-------------------------------------------------------------------------------------------------*/
class HelloTriangleApplication {
private:
    GLFWwindow *mWindow = nullptr;
    uint32_t mWindowWidth = 800;
    uint32_t mWindowHeight = 600;
    static const size_t MAX_FRAMES_IN_FLIGHT = 2;

#ifdef NDEBUG
    const bool mEnableValidationLayers = false;
#else
    const bool mEnableValidationLayers = true;
#endif // NDEBUG


    VkInstance mInstance = VK_NULL_HANDLE;
    VkDebugUtilsMessengerEXT mCallback = VK_NULL_HANDLE;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;
    VkDevice mLogicalDevice = VK_NULL_HANDLE;
    VkQueue mGraphicsQueue = VK_NULL_HANDLE;
    VkSurfaceKHR mSurface = VK_NULL_HANDLE;
    VkQueue mPresentationQueue = VK_NULL_HANDLE;
    VkSwapchainKHR mSwapChain = VK_NULL_HANDLE;
    VkFormat mSwapChainImageFormat = VkFormat::VK_FORMAT_UNDEFINED;
    VkExtent2D mSwapChainExtent{};
    std::vector<VkImageView> mSwapChainImageViews;
    std::vector<VkFramebuffer> mSwapChainFramebuffers;
    VkRenderPass mRenderPass = VK_NULL_HANDLE;
    VkDescriptorSetLayout mDescriptorSetLayout = VK_NULL_HANDLE;
    VkPipelineLayout mPipelineLayout = VK_NULL_HANDLE;
    VkPipeline mGraphicsPipeline = VK_NULL_HANDLE;
    VkCommandPool mCommandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkSemaphore> mSemaphoresImageAvailable;
    std::vector<VkSemaphore> mSemaphoresRenderFinished;
    std::vector<VkFence> mInFlightFences;
    size_t mCurrentFrame = 0;
    bool mFrameBufferResized = false;   // not all drivers properly handle window resize notifications in Vulkan

    std::vector<Vertex> mVertexes;
    std::vector<uint32_t> mVertexIndices;
    VkBuffer mVertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mVertexBufferMemory = VK_NULL_HANDLE;
    VkBuffer mVertexIndexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory mVertexIndexBufferMemory = VK_NULL_HANDLE;

    std::vector<VkBuffer> mUniformBuffers;
    std::vector<VkDeviceMemory> mUniformBuffersMemory;

    VkDescriptorPool mDescriptorPool = VK_NULL_HANDLE;
    std::vector<VkDescriptorSet> mDescriptorSets;

    uint32_t mTextureMipLevels = 0;
    VkImage mTextureImage = VK_NULL_HANDLE;
    VkImageView mTextureImageView = VK_NULL_HANDLE;
    VkDeviceMemory mTextureImageMemory = VK_NULL_HANDLE;
    VkSampler mTextureSampler = VK_NULL_HANDLE;

    VkImage mDepthImage = VK_NULL_HANDLE;
    VkDeviceMemory mDepthImageMemory = VK_NULL_HANDLE;
    VkImageView mDepthImageView = VK_NULL_HANDLE;


    const std::vector<const char *> mRequiredDeviceExtensions{
        VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };

    /*---------------------------------------------------------------------------------------------
    Description:
        Encapsulates info about whether or not the necessary command queue indexes
        (driver-specific-but-won't-change-at-runtime locations) have been acquired.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    struct QueueFamilyIndices {
        // can the GPU draw graphics?
        std::optional<uint32_t> graphicsFamily;

        // can the GPU's driver draw to the surface that we're using?
        std::optional<uint32_t> presentationFamily; //??"present family"??

        bool IsComplete() {
            return graphicsFamily.has_value() && presentationFamily.has_value();
        }
    };

    /*---------------------------------------------------------------------------------------------
    Description:
        Windows are containers. We don't draw to windows. Window managers provide a "surface" that
        is displayed on top of the window (and that usually encompasses less than the whole thing
        so that we can have a border, etc.). We draw to these surfaces. To use it properly, we
        must understand what our chosen window manager's surface support is capable of doing.

        Those details are stored here.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    struct SwapChainSupportDetails {
        // min/max image count, min/max image width, min/max image height, others
        VkSurfaceCapabilitiesKHR capabilities;

        // pixel format and color space for each image
        std::vector<VkSurfaceFormatKHR> formats;

        // ??presentation modes?? for each image
        std::vector<VkPresentModeKHR> presentModes;
    };

public:
    void Run() {
        InitWindow();
        InitVulkan();
        MainLoop();
        Cleanup();
    }

private:

    /*---------------------------------------------------------------------------------------------
    Description:
        Support function for CreateInstance(). It checks ever string in the argument against the
        extension list provided by the driver. If there is something in the required list that
        isn't in the driver's list, an error is thrown.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void CheckRequiredExtensionsSupportedByDriver(const std::vector<const char *> &requiredExtensions) const {
        uint32_t instanceExtensionCount = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, nullptr);
        std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
        vkEnumerateInstanceExtensionProperties(nullptr, &instanceExtensionCount, instanceExtensions.data());

#define PRINT_EXTENSIONS
#ifndef PRINT_EXTENSIONS
        // carry on
#else
        std::stringstream ss;
        std::string name("    Name: ");
        std::string spec("Spec Version: ");

        //std::cout << "Instance extensions (GLFW) required:" << std::endl;
        //ss << std::setfill(' ') << std::left;
        //for (uint32_t i = 0; i < instanceExtensionCount; i++) {
        //    ss << std::setw(name.length()) << name
        //        << std::setw(40) << instanceExtensions[i].extensionName << std::endl;
        //}
        //std::cout << ss.str();

        ss.clear();
        std::cout << "Instance extensions (driver) available:" << std::endl;
        ss << std::setfill(' ') << std::left;
        for (auto &ext : instanceExtensions) {
            ss << std::setw(name.length()) << name
                << std::setw(40) << ext.extensionName
                << std::setw(spec.length()) << spec
                << std::setw(3) << ext.specVersion << std::endl;
        }
        std::cout << ss.str();
#undef PRINT_EXTENSIONS
#endif // NDEBUG

        for (auto &reqExt : requiredExtensions) {
            bool found = false;
            for (auto &prop : instanceExtensions) {
                if (strcmp(reqExt, prop.extensionName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                std::stringstream ss;
                ss << "Required extension '" << reqExt << "' not found in the available driver extensions";
                throw std::runtime_error(ss.str());
            }

        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Support function for CreateInstance(). Like CheckRequiredExtensionsSupportedByDriver(...),
        but for validation layers.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void CheckValidationLayersSupportedByDriver(const std::vector<const char *> &requiredLayers) const {
        uint32_t layerCount;
        vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
        std::vector<VkLayerProperties> availableLayers(layerCount);
        vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

#define PRINT_EXTENSIONS
#ifndef PRINT_EXTENSIONS
        // carry on
#else
        std::stringstream ss;
        std::string name("    Name: ");
        std::string spec("Spec Version: ");

        ss.clear();
        std::cout << "Instance validation layers (driver) available:" << std::endl;
        ss << std::setfill(' ') << std::left;
        for (auto &layer : availableLayers) {
            ss << std::setw(name.length()) << name
                << std::setw(40) << layer.layerName
                << std::setw(spec.length()) << spec
                << std::setw(3) << layer.specVersion << std::endl;
        }
        std::cout << ss.str();
#undef PRINT_EXTENSIONS
#endif // NDEBUG


        for (auto reqLayer : requiredLayers) {
            bool found = false;
            for (auto &availableLayer : availableLayers) {
                if (strcmp(reqLayer, availableLayer.layerName) == 0) {
                    found = true;
                    break;
                }
            }

            if (!found) {
                std::stringstream ss;
                ss << "Required layer '" << reqLayer << "' not found in the available layers";
                throw std::runtime_error(ss.str());
            }
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        The function called by VK_EXT_debug_utils callback object.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    static VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData) {

        std::cerr << "validation layer [" << pCallbackData->pMessageIdName << "]: " << pCallbackData->pMessage << std::endl << std::flush;
        return VK_FALSE;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Creates the VkInstance class member. Includes checking for extension and layer support.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateInstance() {
        // general info about the application
        VkApplicationInfo appInfo{};
        appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        appInfo.pApplicationName = "Hello Triangle";
        appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.pEngineName = "No Engine";
        appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
        appInfo.apiVersion = VK_MAKE_VERSION(1, 0, 0);

        // have to ask GLFW for info about the required extensions
        uint32_t glfwExtensionCount = 0;
        const char ** glfwExtensions;
        glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
        std::vector<const char *> requiredExtensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
        if (mEnableValidationLayers) {
            requiredExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
        }

        // for debugging
        const std::vector<const char *> requiredValidationLayers{
            "VK_LAYER_LUNARG_core_validation",
            "VK_LAYER_LUNARG_standard_validation",
        };
        if (mEnableValidationLayers) {
            CheckValidationLayersSupportedByDriver(requiredValidationLayers);
            CheckRequiredExtensionsSupportedByDriver(requiredExtensions);
        }

        // for printing debug info with the "debug utils" callback messenger object
        // https://www.lunarg.com/wp-content/uploads/2018/05/Vulkan-Debug-Utils_05_18_v1.pdf
        VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};   // default null
        if (mEnableValidationLayers) {
            debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            debugCreateInfo.messageSeverity =
                VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                //VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
                VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            debugCreateInfo.messageType =
                VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            debugCreateInfo.pfnUserCallback = DebugCallback;    // global function
            debugCreateInfo.pUserData = nullptr;
        }

        // finally, pull all that info together to create the instance
        VkInstanceCreateInfo instanceCreateInfo{};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &appInfo;
        instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
        instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();

        if (mEnableValidationLayers) {
            instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
            instanceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();
        }

        instanceCreateInfo.pNext = &debugCreateInfo;

        if (vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance");
        }

        // and create the debug messenger itself
        if (CreateDebugUtilsMessengerEXT(mInstance, &debugCreateInfo, nullptr, &mCallback) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug callback");
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        The window in an OS-specific container. You can't draw to a "window" object as designed in modern OS'. It's become a graphical paradigm that a window will have a "surface" object instead, a ??what is it??
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateSurface() {
        if (glfwCreateWindowSurface(mInstance, mWindow, nullptr, &mSurface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Queries the provided device and checks the properties to see if it supports graphics
        command queues. (??is there a GPU that doesn't??)
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) {
        QueueFamilyIndices indices;

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilyProperties(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilyProperties.data());

        for (uint32_t index = 0; index < queueFamilyCount; index++) {
            const auto &queueFamilyProp = queueFamilyProperties.at(index);
            bool queueExists = queueFamilyProp.queueCount > 0;

            bool supportsGraphics = (queueFamilyProp.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            if (queueExists && supportsGraphics) {
                indices.graphicsFamily = index;
            }

            VkBool32 surfaceSupported = false;    //??"present support"? what does that mean??
            vkGetPhysicalDeviceSurfaceSupportKHR(device, index, mSurface, &surfaceSupported);
            if (queueExists && surfaceSupported) {
                indices.presentationFamily = index;
            }

            if (indices.IsComplete()) {
                break;
            }
        }
        return indices;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Self-explanatory.
        TODO: ??make into a "Get" function instead? shove "Required" items into a namespace or another file??
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    bool CheckDeviceExtensionsSupport(VkPhysicalDevice device) {
        uint32_t extensionCount = 0;
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

        std::vector<VkExtensionProperties> availableExtensions(extensionCount);
        vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

        std::set<std::string> requiredExtensions(mRequiredDeviceExtensions.begin(), mRequiredDeviceExtensions.end());
        for (const auto &ext : availableExtensions) {
            requiredExtensions.erase(ext.extensionName);
        }

        // if empty, all required extensions have been found
        return requiredExtensions.empty();
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        If the preferred format sRGB (B8G8R8A8 nonlinear) is available, return that. Else return
        the first available supported image format.

        More on non-linear color space:
        https://stackoverflow.com/questions/12524623/what-are-the-practical-differences-when-working-with-colors-in-a-linear-vs-a-no
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    VkSurfaceFormatKHR ChooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats) {
        if (availableFormats.size() == 1 && availableFormats.front().format == VK_FORMAT_UNDEFINED) {
            return {
                VK_FORMAT_B8G8R8A8_UNORM,
                VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };
        }

        for (const auto &available : availableFormats) {
            if (available.format == VK_FORMAT_B8G8R8A8_UNORM &&
                available.colorSpace == VK_COLORSPACE_SRGB_NONLINEAR_KHR) {
                return available;
            }
        }

        // preferred format not available, so just use the first supported; should be good enough
        return availableFormats.front();
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Rankings:
        (1) Mailbox
        (2) Immediate
        (3) FIFO

        Note: The tutorial places "Immediate" above "FIFO", saying "Unfortunately some drivers
        currently don't properly support VK_PRESENT_MODE_FIFO_KHR, so we should prefer
        VK_PRESENT_MODE_IMMEDIATE_KHR if VK_PRESENT_MODE_MAILBOX_KHR is not available".

        Mailbox can be used to implement a triple buffer => first choice.

        FIFO is a simple double buffer, reminiscient of OpenGL's size 2 framebuffer. It is
        guaranteed to be an option.

        Immediate shoves finished images to screen without waiting for screen refresh. This can
        cause tearing => last choice.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    VkPresentModeKHR ChooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
        bool immediateAvailable = false;

        for (const auto &available : availablePresentModes) {
            if (available == VK_PRESENT_MODE_MAILBOX_KHR) {
                // found most preferred option; don't bother with the rest of the loop
                return available;
            }
            else if (available == VK_PRESENT_MODE_IMMEDIATE_KHR) {
                immediateAvailable = true;
            }
        }

        if (immediateAvailable) {
            return VK_PRESENT_MODE_IMMEDIATE_KHR;
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        "Extent" is the resolution (width, height) of the images that are put into the swap chain.
        This is either slightly smaller than the window's resolution (windowed mode) or equal to
        it (usually full screen mode).

        Note: Some window managers (GLFW included) allow the extent of the image to be
        automatically selected by the window manager. If the window manager did not automatically
        select the resolution (either because it couldn't or for some reason it just didn't), then
        the "extent" is set to maximum uint32. In that case, manually clamp the resolution to the
        size of the window.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    VkExtent2D ChooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            // window manager will select image extent for us
            return capabilities.currentExtent;
        }

        int width = 0;
        int height = 0;
        glfwGetFramebufferSize(mWindow, &width, &height);
        VkExtent2D actualExtent = {
            static_cast<uint32_t>(width),
            static_cast<uint32_t>(height)
        };

        std::cout << "Extent2D width: " << width << ", height: " << height << ", window width: " << mWindowWidth << ", height: " << mWindowHeight << std::endl;

        // manually clamp to window size
        actualExtent.width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));
        return actualExtent;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Unlike OpenGL, which has a default framebuffer, we have to set one up manually. In Vulkan,
        the buffer of images that the surface will pull from to swap out the current image is
        called a "swap chain". Need to make sure that the surface can pull from a buffer created
        by this device.

        Note: Not all GPUs can do this. Computers made for blade PCs in a server rack may not even
        have an output, and so it is unlikely that the creator of that GPU's driver would have
        even bothered to set it up to talk with the OS' window surfaces. (??is this right??)
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    SwapChainSupportDetails QuerySwapChainSupport(VkPhysicalDevice device) {
        SwapChainSupportDetails details;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, mSurface, &details.capabilities);

        uint32_t formatCount = 0;
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, nullptr);
        if (formatCount != 0) {
            details.formats.resize(formatCount);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, mSurface, &formatCount, details.formats.data());
        }

        uint32_t presentModeCount = 0;
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, nullptr);
        if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, mSurface, &presentModeCount, details.presentModes.data());
        }

        return details;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Checks if the necessary features to run this program are available on the provided GPU.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    bool IsDeviceSuitable(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        bool isDiscreteGpu = (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU);

        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        bool supportsGeometryShader = deviceFeatures.geometryShader;
        bool supportsSamplerAnisotropy = deviceFeatures.samplerAnisotropy;

        QueueFamilyIndices indices = FindQueueFamilies(device);
        bool hasAllRequiredQueueFamilyIndices = indices.IsComplete();

        bool deviceExtensionsSupported = CheckDeviceExtensionsSupport(device);

        bool swapChainAdequate = false;
        if (deviceExtensionsSupported) {
            SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(device);
            swapChainAdequate =
                !swapChainSupport.formats.empty() &&
                !swapChainSupport.presentModes.empty();
        }

        bool suitable =
            isDiscreteGpu &&
            supportsGeometryShader &&
            supportsSamplerAnisotropy &&
            hasAllRequiredQueueFamilyIndices &&
            deviceExtensionsSupported &&
            swapChainAdequate;
        return suitable;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Sets the GPU that will be used for the rest of the program.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void PickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(mInstance, &deviceCount, nullptr);
        if (deviceCount == 0) {
            throw std::runtime_error("failed to find GPUs with Vulkan support");
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(mInstance, &deviceCount, devices.data());

        // take the first one that Vulkan can use
        for (const auto &dev : devices) {
            if (IsDeviceSuitable(dev)) {
                mPhysicalDevice = dev;
                break;
            }
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        A logical device seems to be the C-based Vulkan's version of a "base class" that
        implements common functions regardless of the physical device.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateLogicalDevice() {
        float queuePriority = 1.0f;
        QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);

        std::vector<VkDeviceQueueCreateInfo> deviceCommandQueuesCreateInfo;

        // Note: Graphics command queues on dedicated graphics cards will also be accepted by the 
        // OS as being able to draw to a surface. This is not always the case across all graphics 
        // devices and all OSs. This tutorial is being done on an NVidia GTX 980 TI, who's 
        // graphics commands queue can put out results to a surface. For design reasons, we are 
        // keeping the "graphics queue" and the "presentation queue" two queues in separate 
        // structures, but they are actually the same thing. When creating a logical device, we 
        // need to tell it how many *unique* command queues it has, hence std::set(...). I could 
        // certainly hard-code the use of one of them and move one, but this verbose approach is 
        // better for a tutorial.
        std::set<uint32_t> uniqueQueueFamiliesIndices{
            indices.graphicsFamily.value(),
            indices.presentationFamily.value()
        };
        for (const auto &queueFamilyIndex : uniqueQueueFamiliesIndices) {
            VkDeviceQueueCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            createInfo.queueFamilyIndex = queueFamilyIndex;
            createInfo.queueCount = 1;
            createInfo.pQueuePriorities = &queuePriority;   //??same priority??
            deviceCommandQueuesCreateInfo.push_back(createInfo);
        }

        // don't need any features yet, so leave it blank for now
        VkPhysicalDeviceFeatures deviceFeatures{};
        deviceFeatures.samplerAnisotropy = VK_TRUE;
        //deviceFeatures.fillModeNonSolid = VK_TRUE;

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<uint32_t>(deviceCommandQueuesCreateInfo.size());
        createInfo.pQueueCreateInfos = deviceCommandQueuesCreateInfo.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<uint32_t>(mRequiredDeviceExtensions.size());
        createInfo.ppEnabledExtensionNames = mRequiredDeviceExtensions.data();

        if (vkCreateDevice(mPhysicalDevice, &createInfo, nullptr, &mLogicalDevice) != VK_SUCCESS) {
            throw std::runtime_error("failed to create logical device");
        }

        // and retrieve the command queue that was created with it (based on createInfo)
        // Note: Only created one command queue with the device, so use index 0.
        // Also Note: Not to be confused with queueFamilyIndex, which is physical-device-specific.
        // Gah.
        uint32_t queueIndex = 0;

        // if the graphics queue's family also supported surface drawing, then both queues will have the same handle now (??isn't that a bad thing??)
        vkGetDeviceQueue(mLogicalDevice, indices.graphicsFamily.value(), queueIndex, &mGraphicsQueue);
        vkGetDeviceQueue(mLogicalDevice, indices.presentationFamily.value(), queueIndex, &mPresentationQueue);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Rather than the simple "swap" functionality of OpenGL, which had a default "framebuffer"
        with "front" and "back" buffers (where "buffer" means "a pixel array"), Vulkan's
        framebuffer requires more setup and puts in place a "swap chain", which is a set of image
        buffers that can be swapped out as desired. This "chain" (??is it a linked list??) has more flexibility than OpenGL's framebuffer.

        There is a minimum and maximum size to this swap chain. When the window manager's creators
        implemented Vulkan support on the window's drawing surface, they determined these values.
        We just need to query them.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateSwapChain() {
        SwapChainSupportDetails swapChainSupport = QuerySwapChainSupport(mPhysicalDevice);
        VkSurfaceFormatKHR surfaceFormat = ChooseSwapSurfaceFormat(swapChainSupport.formats);
        VkPresentModeKHR presentMode = ChooseSwapPresentMode(swapChainSupport.presentModes);
        VkExtent2D extent = ChooseSwapExtent(swapChainSupport.capabilities);

        // attempt to allocate enough images for a triple buffer
        // Note: Min image count should always be 2 (minimum required for present mode FIFO 
        // (simple double buffer), which is always available). For a triple buffer, we want the 
        // min image count to be 3.
        uint32_t createMinImageCount = swapChainSupport.capabilities.minImageCount + 1;
        if (swapChainSupport.capabilities.maxImageCount == 0) {
            // indicates no limit for swap chain size aside from memory
        }
        else {
            // there is a constraint aside from memory
            // Note: The count is unsigned, so implicitly this means "> 0".
            if (createMinImageCount > swapChainSupport.capabilities.maxImageCount) {
                // it seems that max image count == min image count; clamp "min" to prevent errors
                createMinImageCount = swapChainSupport.capabilities.maxImageCount;
            }
            else {
                // no problem with minimum 3 images
            }
        }

        // 1 layer per image in the swapchain unless doing stereoscopic 3D, in which case there 
        // will be 2 layers, one for each eye, that get pushed out the swap chain simultaneously
        uint32_t numImageLayers = 1;

        // will draw directly to an image in the swap chain
        // Note: If doing post-processing, will need to create one swap chain with 
        // VK_IMAGE_USAGE_TRANSFER_SRC_BIT, create another swap chain with 
        // VK_IMAGE_USAGE_TRANSFER_DST_BIT, and use a memory operation to transfer the rendered 
        // image from the "source" swap chain to the "destination" swap chain.
        VkImageUsageFlags imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = mSurface;
        createInfo.minImageCount = createMinImageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = numImageLayers;
        createInfo.imageUsage = imageUsage;

        //???what does this sentence mean: "we need to specify how to handle swap chain images that will be used across multiple queue families"??
        QueueFamilyIndices indices = FindQueueFamilies(mPhysicalDevice);
        if (indices.graphicsFamily == indices.presentationFamily) {
            // Note: Default "exclusive", meaning that only one queue family can access the swap 
            // chain at a time. This allows Vulkan to optimize and is thus the most efficient.
        }
        else {
            // both the graphics queue family and the presentation queue family need to use this 
            // swap chain; this is the tutorial's way of handling it for now, saying that it will 
            // do something more latter
            uint32_t queueFamilyIndices[]{
                indices.graphicsFamily.value(),
                indices.presentationFamily.value()
            };
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }

        // Note: We can bake a transform (90 degree clockwise rotation, horizontal flip, etc.) 
        // into the swap chain. For example, a fish-eye transform can be run on the image before 
        // the image gets sent to a VR headset.
        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

        // set to opaque unless you want to make the window surface transluscent (might want to do 
        // this in mobile GUIs)
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

        // retrieved earlier
        createInfo.presentMode = presentMode;

        // if a region of the surface is not visible (ex: another window is in front of this one), 
        // discard the obscured pixels
        createInfo.clipped = VK_TRUE;

        // Note: If a swap chain becomes invalid at runtime (ex: window resized, so image extents 
        // are no longer valid), then need to recreate the swap chain from scratch. We'll deal 
        // with that later. For now (11/3/2018), our GLFW window size is fixed.
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        // all that leads up to this
        if (vkCreateSwapchainKHR(mLogicalDevice, &createInfo, nullptr, &mSwapChain) != VK_SUCCESS) {
            throw std::runtime_error("failed to create swap chain");
        }

        // Note: Cannot create space for the images in the swap chain prior to creating it because 
        // the create info only specifies "minimum" image count. Vulkan is allowed to create more 
        // than that (I don't know why it would, but it can, and need to account for it).
        uint32_t imageCount = 0;
        vkGetSwapchainImagesKHR(mLogicalDevice, mSwapChain, &imageCount, nullptr);
        std::vector<VkImage> swapChainImages(imageCount, VK_NULL_HANDLE);
        vkGetSwapchainImagesKHR(mLogicalDevice, mSwapChain, &imageCount, swapChainImages.data());

        // store these for later
        mSwapChainImageFormat = surfaceFormat.format;
        mSwapChainExtent = extent;

        // create a "view" for each image
        // Note: A "view" tells Vulkan how to handle the array of data that we generically call 
        // "image". Not every "image" is a finished drawing. It may be a rendering from a 
        // particular point of view and will be used as a texture, or it may be a lighting map 
        // with simple intensity values, or it could simply be a finished array of pixel colors.
        mSwapChainImageViews.resize(swapChainImages.size());
        for (size_t i = 0; i < swapChainImages.size(); i++) {
            uint32_t mipLevels = 1;
            mSwapChainImageViews.at(i) = CreateImageView(swapChainImages.at(i), mSwapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        This code is used during swap chain recreation, so it was moved from program-end Cleanup()
        to here.
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    void CleanupSwapChain() {
        // not necessary to destroy in reverse order of creation since they are all created off 
        // the logical device and will not complain when destroyed out of order unless the logical 
        // device is destroyed first, but I'm doing it anyway because it looks orderly
        vkFreeCommandBuffers(mLogicalDevice, mCommandPool, static_cast<uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());
        for (auto &framebuffer : mSwapChainFramebuffers) {
            vkDestroyFramebuffer(mLogicalDevice, framebuffer, nullptr);
        }
        vkDestroyImageView(mLogicalDevice, mDepthImageView, nullptr);
        vkDestroyImage(mLogicalDevice, mDepthImage, nullptr);
        vkFreeMemory(mLogicalDevice, mDepthImageMemory, nullptr);
        vkDestroyPipeline(mLogicalDevice, mGraphicsPipeline, nullptr);
        vkDestroyPipelineLayout(mLogicalDevice, mPipelineLayout, nullptr);
        vkDestroyRenderPass(mLogicalDevice, mRenderPass, nullptr);
        for (auto &imageView : mSwapChainImageViews) {
            vkDestroyImageView(mLogicalDevice, imageView, nullptr);
        }
        vkDestroySwapchainKHR(mLogicalDevice, mSwapChain, nullptr);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        During window resizing, image extent is no longer valid, and therefore the swap chain
        create info is no longer valid, and neither are the image views, or the render pass, or
        anything downstream that depended upon image extent.
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    void RecreateSwapChain() {
        int width = 0;
        int height = 0;
        while (width == 0 || height == 0) {
            std::cout << "Waiting on frame buffer" << std::endl;
            glfwGetFramebufferSize(mWindow, &width, &height);
            glfwWaitEvents();
        }
        vkDeviceWaitIdle(mLogicalDevice);

        CleanupSwapChain();

        CreateSwapChain();
        CreateRenderPass();
        // Note: Not recreating the descriptor set layout because those are independent of image.
        CreateGraphicsPipeline();
        CreateDepthResources();
        CreateFramebuffers();
        CreateCommandBuffers();
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        At this time (1/12/2019) in the tutorial (Texture Mapping: Image view and sampler), the
        image view create is mostly the same, so while image creation is special, image view
        creation is not. We are not using images for depth, which require a different type of
        image view.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    VkImageView CreateImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
        VkImageViewCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = image;
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = format;

        // we could mess with the mappings of components.r, components.g, etc. and set all 
        // color components in the view to be "red" (VK_COMPONENT_SWIZZLE_R), thus giving us a 
        // monochrome red shade, or we could leave them at their default values 
        // (VK_COMPONENT_SWIZZLE_IDENTITY = 0), which is what we're going to do here 
        // Note: I could ignore this completely since structure creation with {} makes them all 0, 
        // but for the sake fo the tutorial, I will be explicit.
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        // no mip mapping, and 1 layer (stereoscopic 3D needs 2 layers (??I think??))
        createInfo.subresourceRange.aspectMask = aspectFlags;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = mipLevels;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VkImageView view = VK_NULL_HANDLE;
        if (vkCreateImageView(mLogicalDevice, &createInfo, nullptr, &view) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image views");
        }
        return view;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        "Before we can finish creating the pipeline, we need to tell Vulkan about the framebuffer
        attachments that will be used while rendering. We need to specify how many color and depth
        buffers there will be, how many samples to use for each of them and how their contents
        should be handled throughout the rendering operations. All of this information is wrapped
        in a render pass object..."

        Note: An "attachment" is one of the Vulkan synonyms for images. See my (mildly popular)
        post on Reddit that asked what it is.
        https://www.reddit.com/r/vulkan/comments/a27cid/what_is_an_attachment_in_the_render_passes/

        ??what? "color and depth buffers"? "framebuffer attachments"??

        https://vulkan-tutorial.com/Drawing_a_triangle/Graphics_pipeline_basics/Render_passes

        Render Passes in Vulkan
        https://www.youtube.com/watch?v=x2SGVjlVGhE
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateRenderPass() {
        VkAttachmentDescription colorAttachmentDesc{};
        colorAttachmentDesc.format = mSwapChainImageFormat;
        colorAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;    // not doing multisampling yet, so ??one sample per texture? per pixel??
        colorAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;  // don't know, don't care
        colorAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentDescription depthAttachmentDesc{};
        depthAttachmentDesc.format = FindDepthFormat();
        depthAttachmentDesc.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachmentDesc.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachmentDesc.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDesc.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachmentDesc.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachmentDesc.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachmentDesc.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        std::array<VkAttachmentDescription, 2> attachments = { colorAttachmentDesc, depthAttachmentDesc };

        // Note: The render pass' create info specifies all the attachments that will be available 
        // during it. Each subpass description only specifies references though, and it does this 
        // through indexing.
        // Also Note: This index is the same as the "layout(location=0/location=1/etc.)" in the 
        // shaders that are in use during this render pass.
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;  // index into attachment descriptions array
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // Note: There is a "count" for the color attachments but not for the depth because a 
        // shader that is used within a subpass may be designed to read from/write to (??I think 
        // multiple write??) multiple color attachments, hence the need for a count, but cannot be 
        // designed to read from/write to more than one depth attachment because it is assumed 
        // that the depth attachment is relevant only to the current image under construction, and 
        // so there would be no need to check any other image's depth data. (??I think??)
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;

        // Note: There is an implit "subpass" that occurs at the start of the pipeline and another 
        // at the end. There may not be an image available yet by the time the command buffer 
        // pipeline is ready to execute. Insert a subpass dependency to make sure that this 
        // implicit subpass doesn't begin prematurely. In other words, specify that all other 
        // subpasses must finish before beginning subpass 0 (index 0 indicating our one (and only) 
        // subpass).
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // finally, make the render pass object itself
        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassCreateInfo.pAttachments = attachments.data();
        renderPassCreateInfo.subpassCount = 1;
        renderPassCreateInfo.pSubpasses = &subpass;
        renderPassCreateInfo.dependencyCount = 1;
        renderPassCreateInfo.pDependencies = &dependency;

        if (vkCreateRenderPass(mLogicalDevice, &renderPassCreateInfo, nullptr, &mRenderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass");
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        A "descriptor" is shorthand for the general programming term "data descriptor", which is a
        structure containing information that describes data. How helpful :/. In Vulkan, a
        descriptor is used to provide information to shaders in a pipeline about resources that
        are available (uniforms, texture samplers, etc.).

        Note: It is possible for the shader's descriptor at a particular binding index (in this
        case we're using 0) to specify an array of descriptor objects, such as an array of uniform
        buffer objects, each with a set of transforms, for every "bone" in a skeletal animation.
        We're just using a single descriptor though, so our descriptor count is only 1;
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateDescriptorSetLayout() {
        // Note: Despite the name, this is info about a single descriptor, not a set. It is to be 
        // understood as a single descriptor "binding" within a descriptor set.
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;   // same binding location as in the shader that uses it
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;  // no textures yet at this time (1-1-2019)

        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerLayoutBinding.pImmutableSamplers = nullptr;

        std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };

        VkDescriptorSetLayoutCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        createInfo.pBindings = bindings.data();

        if (vkCreateDescriptorSetLayout(mLogicalDevice, &createInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Given a file path to a compiled shader binary, reads the entire file and constructs a
        VkShaderModule object out of the data, then returns it.

        Note: Thge shader module requires explicit destruction since it was created with a
        "vkCreate*(...)" call.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    VkShaderModule CreateShaderModule(const std::string &filePath) {
        auto shader_binary = ReadFileIntoString(filePath);

        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = shader_binary.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(shader_binary.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(mLogicalDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module");
        }

        return shaderModule;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Governs the creation of all stages of the graphics pipeline.

        Some stages are fixed, and others are programmable.
        - The "fixed" stages can be configured with a set number of options from the SDK.
        - The "programmable" stages can be set up with a shader or be skipped entirely.

        From the tutorial:
        - The input assembler collects the raw vertex data from the buffers you specify and may
            also use an index buffer to repeat certain elements without having to duplicate the
            vertex data itself. This stage is FIXED.
        - The vertex shader is run for every vertex and generally applies transformations to turn
            vertex positions from model space to screen space. It also passes per-vertex data down
            the pipeline. This stage is PROGRAMMABLE.
        - The tessellation shaders allow you to subdivide geometry based on certain rules to
            increase the mesh quality. This is often used to make surfaces like brick walls and
            staircases look less flat when they are nearby. This stage is PROGRAMMABLE.
        - The geometry shader is run on every primitive (triangle, line, point) and can discard it
            or output more primitives than came in. This is similar to the tessellation shader,
            but much more flexible. However, it is not used much in today's applications because
            the performance is not that good on most graphics cards except for Intel's integrated
            GPUs. This stage is PROGRAMMABLE.
        - The rasterization stage discretizes the primitives into fragments. These are the pixel
            elements that they fill on the framebuffer. Any fragments that fall outside the screen
            are discarded and the attributes outputted by the vertex shader are interpolated
            across the fragments, as shown in the figure. Usually the fragments that are behind
            other primitive fragments are also discarded here because of depth testing. This stage
            is FIXED.
        - The fragment shader is invoked for every fragment that survives and determines which
            framebuffer(s) the fragments are written to and with which color and depth values. It
            can do this using the interpolated data from the vertex shader, which can include
            things like texture coordinates and normals for lighting. This stage is PROGRAMMABLE.
        - The color blending stage applies operations to mix different fragments that map to the
            same pixel in the framebuffer. Fragments can simply overwrite each other, add up or be
            mixed based upon transparency. This stage is FIXED (??is it configurable??)
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateGraphicsPipeline() {
        VkShaderModule vertShaderModule = CreateShaderModule("shaders/vert.spv");
        VkShaderModule fragShaderModule = CreateShaderModule("shaders/frag.spv");

        std::vector<VkPipelineShaderStageCreateInfo> shaderStageCreateInfos;
        {
            VkPipelineShaderStageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
            createInfo.module = vertShaderModule;
            createInfo.pName = "main";
            shaderStageCreateInfos.push_back(createInfo);
        }
        {
            VkPipelineShaderStageCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            createInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
            createInfo.module = fragShaderModule;
            createInfo.pName = "main";
            shaderStageCreateInfos.push_back(createInfo);
        }

        auto bindingDescription = Vertex::GetBindingDescription();
        auto attributeDescription = Vertex::GetAttributeDescription();
        VkPipelineVertexInputStateCreateInfo vertexInputCreateInfo{};
        vertexInputCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputCreateInfo.vertexBindingDescriptionCount = 1;
        vertexInputCreateInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputCreateInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescription.size());
        vertexInputCreateInfo.pVertexAttributeDescriptions = attributeDescription.data();

        // the Input Assembly State's "topology" is like OpenGL's "draw style" (GL_LINES, 
        // GL_TRIANGLES, etc.)
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyCreateInfo{};
        inputAssemblyCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssemblyCreateInfo.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssemblyCreateInfo.primitiveRestartEnable = VK_FALSE;

        // Note: Vulkan's Y values are like everyone else now, with (0,0) in the upper left. This 
        // is unlike OpenGL, which had (0,0) in the lower left.
        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = static_cast<float>(mSwapChainExtent.width);
        viewport.height = static_cast<float>(mSwapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        // Note: Vulkan's rectangle uses "offset" and "extent" instead of "x,y" and 
        // "width,height". They work out to the same thing though.
        VkRect2D scissor{};
        scissor.offset = VkOffset2D{ 0,0 };
        scissor.extent = mSwapChainExtent;

        // Vulkan wants to have the viewport and scissor in a single structure
        // Note: There may be cases in which multiple viewports are desired, such as a flight 
        // simulated with a viewport for each instrument, though there are likely more efficient 
        // ways to handle that through textures. Still, multiple viewports could be a nice thing 
        // in the toolbag, even if we're not going to be using them.
        VkPipelineViewportStateCreateInfo viewportStateCreateInfo{};
        viewportStateCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportStateCreateInfo.viewportCount = 1;
        viewportStateCreateInfo.pViewports = &viewport;
        viewportStateCreateInfo.scissorCount = 1;
        viewportStateCreateInfo.pScissors = &scissor;

        // "The rasterizer takes the geometry that is shaped by the vertices from the vertex 
        // shader and turns it into fragments to be colored by the fragment shader. It also 
        // performs depth testing, face culling and the scissor test, and it can be configured to 
        // output fragments that fill entire polygons or just the edges (wireframe rendering)."
        // Note: If depth clamping is enabled, fragments beyond near/far planes are discarded. 
        // Disabling depth clamping could be useful if we want a render stage that considers all 
        // depth values.
        VkPipelineRasterizationStateCreateInfo rasterizerCreateInfo{};
        rasterizerCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizerCreateInfo.depthClampEnable = VK_FALSE; //??why are we using false??
        rasterizerCreateInfo.rasterizerDiscardEnable = VK_FALSE; // definitely want rasterization
        rasterizerCreateInfo.polygonMode = VK_POLYGON_MODE_FILL;    // or LINE or POINT
        rasterizerCreateInfo.lineWidth = 1.0f;  //>1.0f requires enabling "wideLines" GPU feature
        rasterizerCreateInfo.cullMode = VK_CULL_MODE_BACK_BIT;

        // changed from `CLOCKWISE` because the GLM project matrix had to have its Y axis flipped 
        // to bring it in line with Vulkan's NDCs
        rasterizerCreateInfo.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        //rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizerCreateInfo.depthBiasEnable = VK_FALSE;    //??what is "depth bias"??

        // multisampling is an antialiasing technique that combines fragment shader results of 
        // multiple polygons that rasterize to the same pixel, but we won't be using this at this 
        // time (11/24/2018)
        // Note: Enabling this requires enabling a GPU feature (??which one??)
        VkPipelineMultisampleStateCreateInfo multisamplingCreateInfo{};
        multisamplingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisamplingCreateInfo.sampleShadingEnable = VK_FALSE; //VK_TRUE;
        multisamplingCreateInfo.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        // color blending requires two structures: (1) the "blending attachment state" is per 
        // framebuffer (we only have one) and (2) the "blend state create info", which contains 
        // global color blending settings
        // Note: Color blending often used in conjunction with alpha values.
        // Ex: .dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA; //(1-srcAlpha)
        // Pseudocode:
        //    if (blendEnable) {
        //        finalColor.rgb = (srcColorBlendFactor * newColor.rgb) < colorBlendOp > (dstColorBlendFactor * oldColor.rgb);
        //        finalColor.a = (srcAlphaBlendFactor * newColor.a) < alphaBlendOp > (dstAlphaBlendFactor * oldColor.a);
        //    }
        //    else {
        //        finalColor = newColor;
        //    }
        //    finalColor = finalColor & colorWriteMask;
        VkPipelineColorBlendAttachmentState colorBlendAttachmentState{};
        colorBlendAttachmentState.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachmentState.blendEnable = VK_FALSE;
        //colorBlendAttachmentState.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;    //??
        //colorBlendAttachmentState.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;   //??
        //colorBlendAttachmentState.colorBlendOp = VK_BLEND_OP_ADD;               //??
        //colorBlendAttachmentState.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;    //??
        //colorBlendAttachmentState.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;   //??
        //colorBlendAttachmentState.alphaBlendOp = VK_BLEND_OP_ADD;               //??

        VkPipelineColorBlendStateCreateInfo colorBlendCreateInfo{};
        colorBlendCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlendCreateInfo.logicOpEnable = VK_FALSE;
        colorBlendCreateInfo.logicOp = VK_LOGIC_OP_COPY;
        colorBlendCreateInfo.attachmentCount = 1;   // only one framebuffer
        colorBlendCreateInfo.pAttachments = &colorBlendAttachmentState;

        //??does this 4-item array follow the RGBA order? and what does it do??
        colorBlendCreateInfo.blendConstants[0] = 0.0f;
        colorBlendCreateInfo.blendConstants[1] = 0.0f;
        colorBlendCreateInfo.blendConstants[2] = 0.0f;
        colorBlendCreateInfo.blendConstants[3] = 0.0f;

        // pipeline layout is where we will be using uniforms, but for now (11/24/2018), we leave 
        // it blank
        VkPipelineLayoutCreateInfo pipelineLayoutCreateInfo{};
        pipelineLayoutCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutCreateInfo.setLayoutCount = 1;
        pipelineLayoutCreateInfo.pSetLayouts = &mDescriptorSetLayout;   // MUST have been created prior to this
        if (vkCreatePipelineLayout(mLogicalDevice, &pipelineLayoutCreateInfo, nullptr, &mPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout");
        }

        // need to tell the pipeline to use depth testing
        // Note: Depth testing says to compare depth of a new fragment with the existing depth in 
        // the buffer for that pixel. Depth writing means to write the new depth if the new 
        // fragment is closer to the screen (could be altered by the "compare op", but let's not 
        // mess with normality). The former we always want (unless we are doing something very 
        // odd), but we don't want the latter if we are drawing a transparent object. It would 
        // require another pipeline (all these things get baked into the pipeline, so even minor 
        // changes require another pipeline) to perform depth writing for previously compared 
        // fragments for opaque objects, but if the current pipeline is trying to draw transparent 
        // objects, then we *don't* want to write new depth tests. Right now (1/20/2019) though, 
        // for this tutorial, we do.
        VkPipelineDepthStencilStateCreateInfo depthStencilCreateInfo{};
        depthStencilCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencilCreateInfo.depthTestEnable = VK_TRUE;
        depthStencilCreateInfo.depthWriteEnable = VK_TRUE;
        depthStencilCreateInfo.depthCompareOp = VK_COMPARE_OP_LESS;
        depthStencilCreateInfo.depthBoundsTestEnable = VK_FALSE;    //??what??
        depthStencilCreateInfo.stencilTestEnable = VK_FALSE;        //??what is this??

        VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
        pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineCreateInfo.stageCount = 2;
        pipelineCreateInfo.pStages = shaderStageCreateInfos.data();
        pipelineCreateInfo.pVertexInputState = &vertexInputCreateInfo;
        pipelineCreateInfo.pInputAssemblyState = &inputAssemblyCreateInfo;
        pipelineCreateInfo.pViewportState = &viewportStateCreateInfo;
        pipelineCreateInfo.pRasterizationState = &rasterizerCreateInfo;
        pipelineCreateInfo.pMultisampleState = &multisamplingCreateInfo;
        pipelineCreateInfo.pColorBlendState = &colorBlendCreateInfo;
        pipelineCreateInfo.layout = mPipelineLayout;
        pipelineCreateInfo.renderPass = mRenderPass;
        pipelineCreateInfo.subpass = 0; // index of the subpass in the given render pass where this pipeline will be used (??what??)
        pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // not deriving from existing pipeline
        pipelineCreateInfo.pDepthStencilState = &depthStencilCreateInfo;

        VkPipelineCache pipelineCache = VK_NULL_HANDLE;
        uint32_t pipelineCreateInfoCount = 1;
        if (vkCreateGraphicsPipelines(mLogicalDevice, pipelineCache, pipelineCreateInfoCount, &pipelineCreateInfo, nullptr, &mGraphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline");
        }

        // Note: Vulkan's packaged shader code is only necessary during loading, but since it was 
        // created with a "vkCreate*(...)" call, it needs to be explicitly destroyed.
        vkDestroyShaderModule(mLogicalDevice, vertShaderModule, nullptr);
        vkDestroyShaderModule(mLogicalDevice, fragShaderModule, nullptr);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Generates a framebuffer object for each image in the swap chain.
        ??how is the image in the swap chain different from the framebuffer for it??
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateFramebuffers() {
        mSwapChainFramebuffers.resize(mSwapChainImageViews.size());
        for (size_t i = 0; i < mSwapChainImageViews.size(); i++) {
            std::array<VkImageView, 2> attachments{
                mSwapChainImageViews.at(i),

                // Note: The drawing waits for all items in the Color Attachment pipeline stage to 
                // finish before moving on, so our semaphores may be unnecessary with this 
                // particular design (??you sure??), and as a side effect we can use the same 
                // depth image view for every frame instead of having to do one depth image for 
                // each frame. Personally, I think that this is flimsly justification to avoid 
                // creating extra images. I want a better explanation (??but how do I get one??)
                mDepthImageView,
            };
            VkFramebufferCreateInfo frameBufferCreateInfo{};
            frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frameBufferCreateInfo.renderPass = mRenderPass;
            frameBufferCreateInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            frameBufferCreateInfo.pAttachments = attachments.data();
            frameBufferCreateInfo.width = mSwapChainExtent.width;
            frameBufferCreateInfo.height = mSwapChainExtent.height;
            frameBufferCreateInfo.layers = 1;

            if (vkCreateFramebuffer(mLogicalDevice, &frameBufferCreateInfo, nullptr, &mSwapChainFramebuffers.at(i)) != VK_SUCCESS) {
                throw std::runtime_error("failed to create framebuffer");
            }
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Generates the command pool object that will store the command buffers. Command pools
        manage the memory that is used to store the buffers, and command buffers are allocated
        from them.

        Note: Each command pool can only allocate command buffers for a single type of queue.
        This means that there cannot exist a command buffer that contains both compute and
        graphics commands. Why? The API spec for Mantle (forerunner to Vulkan) describes the
        "Queue submission model" in the section "Execution Model" as different CPU threads
        submitting commands to different queues (graphics queue, compute queue), which are in turn
        submitted to the GPU's "3D Engine" and "Compute Engine", respectively. Vulkan's queue
        family indices are looked up with the VkPhysicalDevice, and combine this with the Mantle
        API descriptions, and it seems that the command queues, and thus the command queue
        families, are a software design model that accomodate's the GPU hardware design that
        implements separate graphics and compute "engines" (??what are these??)
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateCommandPool() {
        QueueFamilyIndices queueFamilyIndices = FindQueueFamilies(mPhysicalDevice);
        VkCommandPoolCreateInfo commandPoolCreateInfo{};
        commandPoolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        commandPoolCreateInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();

        if (vkCreateCommandPool(mLogicalDevice, &commandPoolCreateInfo, nullptr, &mCommandPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create command pool");
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Takes a buffer of data on the GPU and copies it into another chunk of memory that is
        reserved for use by a particular image. This is how we get texture data into an image for
        use as a sampler.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CopyBufferToImage(VkBuffer buffer, VkImage image, VkImageLayout memLayout, uint32_t width, uint32_t height) {
        // need to specify which parts of the buffer will be copied to this image
        VkBufferImageCopy region{};

        // offset where pixels start
        region.bufferOffset = 0;

        // 0 for both indicates close packing (that is, no padding between rows)
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        // indicates the parts of the image that we want to copy
        // Note: If we really wanted to, this allows us to copy one particular mipmap level.
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = { 0,0,0 };
        region.imageExtent = { width,height,1 };


        VkCommandBuffer commandBuffer = BeginSingleUseCommandBuffer();
        vkCmdCopyBufferToImage(commandBuffer, buffer, image, memLayout, 1, &region);
        SubmitAndEndSingleUseCommandBuffer(commandBuffer);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Changes the usage of the texture image from ?? to ??.

        Note: We could use VK_IMAGE_LAYOUT_UNDEFINED if we didn't mind the image potentially
        getting clobbered (implementation defined whether it does this or not), but we already
        used the image as a copy destination and copied the JPEG's pixels to it, so we definitely
        don't want it clobbered and therefore specify the existing layout.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout currentLayout, VkImageLayout newLayout, uint32_t mipLevels) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = currentLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;  // exclusive to graphics queue right now (1/1/2019)
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;

        if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

            // TODO: remove "format" argument, replace with "aspect bitmask"
            if (HasStencilComponent(format)) {
                barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            }
        }
        else {
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }

        barrier.subresourceRange.baseMipLevel = 0;      // no mipmapping right now (1/1/2019)
        barrier.subresourceRange.levelCount = mipLevels;
        barrier.subresourceRange.baseArrayLayer = 0;    // not an array right now (1/1/2019)
        barrier.subresourceRange.layerCount = 1;

        // all work specified in the "source" stage must complete before any work is performed in 
        // the "destination" stage
        VkPipelineStageFlags srcStageMask = 0;
        VkPipelineStageFlags dstStageMask = 0;
        if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
            barrier.srcAccessMask = 0;  // because VkAccessFlagBits enum has no 0 value
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
        }
        else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
            dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        }
        else if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL) {
            // Note: Depth buffer reading occurs in the early fragment tests and writing will 
            // occur in the late fragment tests. Of these two requirements, pick the one that 
            // needs to be available earlier, and then you will be gauranteed that the latter 
            // requirement will certainly be available on time.
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
            srcStageMask = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            dstStageMask = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        }
        else {
            throw std::invalid_argument("unsupported layout transition");
        }

        VkCommandBuffer commandBuffer = BeginSingleUseCommandBuffer();

        VkDependencyFlags dependencies = 0; // VkDependencyFlagBits::VK_DEPENDENCY_BY_REGION_BIT (supposedly, the reading stage is allowed to do so from writes that have already finished, even though the whole write has not...nifty)
        uint32_t memoryBarrierCount = 0;
        VkMemoryBarrier *pMemoryBarrier = nullptr;
        uint32_t bufferMemoryBarrierCount = 0;
        VkBufferMemoryBarrier *pBufferMemoryBarrier = nullptr;
        vkCmdPipelineBarrier(commandBuffer,
            srcStageMask,
            dstStageMask,
            dependencies,
            memoryBarrierCount,
            pMemoryBarrier,
            bufferMemoryBarrierCount,
            pBufferMemoryBarrier,
            1,
            &barrier);

        SubmitAndEndSingleUseCommandBuffer(commandBuffer);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Creates an image that is not part of the swap chain.

        Note: When creating the swap chain, image extent was specified and applied to every image
        that it created. We are now creating a standalone image that is the size of the texture.
        If we have lots of textures, then we will have lots of standalone images, and many will
        not share the same extent. Thus we need to specify the extent for each and every texture
        image.


        TODO: split into two functions, one that creates a VkImage, the other which allocates VkDeviceMemory, or better yet, a class that does this for you and that handles cleanup


    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags memProperties, VkImage &image, VkDeviceMemory &imageMemory) {
        VkImageCreateInfo imageCreateInfo{};
        imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
        imageCreateInfo.extent.width = static_cast<uint32_t>(width);
        imageCreateInfo.extent.height = static_cast<uint32_t>(height);
        imageCreateInfo.extent.depth = 1;   //??what happens at 0??
        imageCreateInfo.mipLevels = mipLevels;
        imageCreateInfo.arrayLayers = 1;
        imageCreateInfo.format = format;
        imageCreateInfo.tiling = tiling;
        imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageCreateInfo.usage = usage;
        imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        if (vkCreateImage(mLogicalDevice, &imageCreateInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image for texture");
        }

        // now allocate memory for the image
        VkMemoryRequirements memRequirements{};
        vkGetImageMemoryRequirements(mLogicalDevice, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = FindMemoryType(memRequirements.memoryTypeBits, memProperties);
        if (vkAllocateMemory(mLogicalDevice, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory");
        }

        VkDeviceSize offset = 0;
        vkBindImageMemory(mLogicalDevice, image, imageMemory, offset);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        ??what??
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    VkFormat FindSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
        for (const VkFormat &format : candidates) {
            VkFormatProperties props{};
            vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, format, &props);

            if (tiling == VK_IMAGE_TILING_LINEAR &&
                (props.linearTilingFeatures & features) == features) {
                return format;
            }
            else if (tiling == VK_IMAGE_TILING_OPTIMAL &&
                (props.optimalTilingFeatures & features) == features) {
                return format;
            }
        }

        throw std::runtime_error("failed to find supported format");    //??wat??
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        ??again, what??
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    VkFormat FindDepthFormat() {
        return FindSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        ??yet again, what??
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    bool HasStencilComponent(VkFormat format) {
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT ||
            format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        ??
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateDepthResources() {
        uint32_t mipLevels = 1;
        VkFormat depthFormat = FindDepthFormat();
        CreateImage(mSwapChainExtent.width, mSwapChainExtent.height, mipLevels, depthFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, mDepthImage, mDepthImageMemory);
        mDepthImageView = CreateImageView(mDepthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, mipLevels);
        TransitionImageLayout(mDepthImage, depthFormat, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, mipLevels);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Given an image (assuming it is used for color in 2 dimensions), generates progressively
        more and more scaled down image resolutions for each increasing mip level (level 0 = base
        image, and the detail decreases from there).

        Note: Scaling is performed by VkCmdBlit(...). This function calculates scaling by, for
        each axis, dividing the specified size of the source region by the specified size of the
        destination region. The result could be >1, and Vulkan will be fine with this. We want to
        generate lesser-detailed mip levels though, so we will make sure that our calculations
        result in progressively more and more scaled down images.

        Also Note: Vulkan will automatically place the new texels in the appropriate memory
        location when we tell it what mip level the destination is. Sweet.
    Creator:    John Cox, 02/2019
    ---------------------------------------------------------------------------------------------*/
    void GenerateMipMaps(VkImage image, VkFormat imageFormat, int32_t tWidth, int32_t tHeight, uint32_t mipLevels) {
        // first check if image format supports blitting like this ("linear blitting")
        // Note: Not all platforms support "blitting", so need to check for it's support if 
        // planning on making this program cross-platform.
        // (??what are "platforms"? Windows vs Linux? PC vs phone??) 
        // Also Note: The image was created with VK_IMAGE_TILING_OPTIMAL, so we need to check if 
        // "optimal tiling" even supports linear filtering from one image to another (it is 
        // expected to; if it doesn't...don't know what to do).
        VkFormatProperties formatProperties;
        vkGetPhysicalDeviceFormatProperties(mPhysicalDevice, imageFormat, &formatProperties);
        if (!(formatProperties.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT)) {
            throw std::runtime_error("texture image format does not support linear blitting");
        }

        VkCommandBuffer commandBuffer = BeginSingleUseCommandBuffer();

        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.image = image;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.subresourceRange.levelCount = 1;

        int32_t mipWidth = tWidth;
        int32_t mipHeight = tHeight;
        int32_t mipDepth = 1;
        VkDependencyFlags barrierDependencyFlags = 0;

        // Note: On each loop:
        // - transition mip level (index) i - 1 from a transfer destination to a source
        // - wait for that level to be filled (either vkCmdBlitImage or vkCmdCopyBufferToImage)
        // - start a blit command to create the next mip level from the previous
        for (uint32_t mipIndex = 1; mipIndex < mipLevels; mipIndex++) {
            // Note: Each mip level can have its own layout. The entire image (all mip levels) is 
            // created with the same layout. The CreateTextureImage(...) function transitions 
            // images after creation and buffer copy to being a transfer destination. As this loop 
            // progresses, need to change the previous texture region (mip level) to a transfer 
            // source. For the first loop, the base texture image is set to source.
            barrier.subresourceRange.baseMipLevel = mipIndex - 1;
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_PIPELINE_STAGE_TRANSFER_BIT, barrierDependencyFlags,
                0, nullptr,     // memory barrier
                0, nullptr,     // buffer memory barrier
                1, &barrier);   // image memory barrier

            // minimum mipmap size 1px x 1px
            // Note: We are only dealing with 2D textures at this time (2/2/2019), so Z will not 
            // have any changes.
            // Also Note: Each mip map level must be constructed from the previous, so only one 
            // region at a time
            std::vector<VkImageBlit> blit(1);

            // describe source region (width = x_src_1 - x_src_0, etc.)
            blit.at(0).srcOffsets[0] = { 0, 0, 0 }; // VkOffset3D
            blit.at(0).srcOffsets[1] = { mipWidth, mipHeight, mipDepth };
            blit.at(0).srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.at(0).srcSubresource.mipLevel = mipIndex - 1;
            blit.at(0).srcSubresource.baseArrayLayer = 0;
            blit.at(0).srcSubresource.layerCount = 1;

            // describe destination region (width = x_dst_1 - x_dst_0, etc.)
            blit.at(0).dstOffsets[0] = { 0, 0, 0 }; // VkOffset3D;
            blit.at(0).dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, mipDepth };
            blit.at(0).dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.at(0).dstSubresource.mipLevel = mipIndex;
            blit.at(0).dstSubresource.baseArrayLayer = 0;
            blit.at(0).dstSubresource.layerCount = 1;

            // do the actual command to scale down the previous image to make this mip level
            vkCmdBlitImage(commandBuffer,
                image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,    // src image == dst image
                image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                static_cast<uint32_t>(blit.size()), blit.data(),
                VK_FILTER_LINEAR);

            // reuse the VkImageBarrier to transition the previous region (mip level) to be shader 
            // read-only before moving on
            barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            vkCmdPipelineBarrier(commandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, barrierDependencyFlags,
                0, nullptr,
                0, nullptr,
                1, &barrier);

            // make sure to do the ">1?" check on both dimensions in case our texture isn't square
            mipWidth = (mipWidth > 1) ? mipWidth / 2 : 1;
            mipHeight = (mipHeight > 1) ? mipHeight / 2 : 1;
        }

        // now transition that last mip level (index) to be used by the fragment shader
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, barrierDependencyFlags,
            0, nullptr,
            0, nullptr,
            1, &barrier);

        SubmitAndEndSingleUseCommandBuffer(commandBuffer);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Loads a JPEG into a buffer of pixel data, creates a VkImage for it and allocates memory
        for it, copies the pixels into it, then transitions the image for optimal use by the
        shaders.

        Stock image:
        https://pixabay.com/en/statue-sculpture-figure-1275469/
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateTextureImage() {
        int tWidth = 0;
        int tHeight = 0;
        int numActualChannels = 0;
        int requestedComposition = STBI_rgb_alpha;
        /*stbi_uc *pixels = stbi_load("textures/statue.jpg", &tWidth, &tHeight, &numActualChannels, STBI_rgb_alpha);*/
        stbi_uc *pixels = stbi_load("textures/chalet.jpg", &tWidth, &tHeight, &numActualChannels, STBI_rgb_alpha);

        // - max finds largest dimension (w or h)
        // - log2 finds how many times that dimension can be divided by 2
        // - floor rounds down to the nearest integer just in case the largest dimension was not a 
        //  power of two
        // - at least 1 (base image is mip level 0 and must exist for the texture to draw at all)
        mTextureMipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(tWidth, tHeight)))) + 1;

        // Note: We requested the image with RGBA, so even if it doesn't actually have 4 channels, 
        // we'll get a 4-channel image (alpha expected to be 0), so we should allocate space for 
        // that.
        VkDeviceSize imageSize = tWidth * tHeight * 4;

        if (pixels == nullptr) {
            throw std::runtime_error("failed to load texture image");
        }

        // we want this image to live in GPU memory for fast access, but as with the vertex 
        // buffer, DEVICE_LOCAL memory is not host coherent, so we'll have to make a staging 
        // buffer for it, copy to that, then copy that into device-only-accessible memory
        VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
        CreateBuffer(imageSize, bufferUsage, memProperties, stagingBuffer, stagingBufferMemory);

        // copy pixels to host-coherent GPU memory
        void *data = nullptr;
        VkDeviceSize zeroOffset = 0;
        VkMemoryMapFlags flags = 0;
        vkMapMemory(mLogicalDevice, stagingBufferMemory, zeroOffset, imageSize, flags, &data);
        memcpy(data, pixels, static_cast<size_t>(imageSize));
        vkUnmapMemory(mLogicalDevice, stagingBufferMemory);
        stbi_image_free(pixels);    // done with the image now

        // now create a Vulkan image for it
        VkFormat imageFormat = VK_FORMAT_R8G8B8A8_UNORM;      //??what happens if it isn't? is this just JPEG??
        VkImageTiling imageTiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags imageUsage =
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT |
            VK_IMAGE_USAGE_SAMPLED_BIT;
        memProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        CreateImage(tWidth, tHeight, mTextureMipLevels,
            imageFormat,
            imageTiling,
            imageUsage,
            memProperties,
            mTextureImage,
            mTextureImageMemory);

        // copy staging buffer into VkImage memory
        VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VkImageLayout destLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        TransitionImageLayout(mTextureImage, imageFormat, currentLayout, destLayout, mTextureMipLevels);
        CopyBufferToImage(stagingBuffer, mTextureImage, destLayout, static_cast<uint32_t>(tWidth), static_cast<uint32_t>(tHeight));

        // generate the lesser detailed textures for the non-base mip levels
        GenerateMipMaps(mTextureImage, VK_FORMAT_R8G8B8A8_UNORM, tWidth, tHeight, mTextureMipLevels);

        // lastly, create a view for the new image
        mTextureImageView = CreateImageView(mTextureImage, imageFormat, VK_IMAGE_ASPECT_COLOR_BIT, mTextureMipLevels);

        // cleanup
        vkDestroyBuffer(mLogicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(mLogicalDevice, stagingBufferMemory, nullptr);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        A texture sampler tells Vulkan how to access the pixels of an image ("an image" indicating
        to any image that it is applied to; the sampler itself does not have a link to any image).
        If the fragment density exceeds the texel density for a given triangle and texture, and
        nothing clever is done but rather the fragment shader just takes the closest pixel, then
        the result is blocky and pixelated as a chunk of fragments take on the same texel, and an
        adjacent chunk of fragments take on the adjacent text. This is called "oversampling", and
        can be seen with low-res textures up close.

        On the flip side (that is, the fragment density is less than the texel density), adjacent
        fragments will take on non-adjacent texels, and the result is blurry. This is called
        "undersampling", and can be seen with hi-res textures at a distance.

        A sampler can specify filtering techniques and how to handle texel coordinate requests
        that are outside the border of the texture (repeat for a tiled image, mirrored repeat,
        clamping).

        Note: "Minification" is the process of dealing with oversampling, and "magnification" is
        the process of dealing with undersampling.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateTextureSampler() {
        VkSamplerCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        createInfo.magFilter = VK_FILTER_LINEAR;    // more texels than fragments
        createInfo.minFilter = VK_FILTER_LINEAR;    // more fragments than texels
        createInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        createInfo.minLod = 0.0f;
        //createInfo.maxLod = 0.0f;
        createInfo.maxLod = static_cast<float>(mTextureMipLevels);
        createInfo.mipLodBias = 0.0f;
        createInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;   // in other words, tiling
        createInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        createInfo.anisotropyEnable = VK_TRUE;
        createInfo.maxAnisotropy = 16;
        createInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        createInfo.unnormalizedCoordinates = VK_FALSE;  // ??maybe TRUE if using sparse textures??
        createInfo.compareEnable = VK_FALSE;    // not using "texel compare" operations for now
        createInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        if (vkCreateSampler(mLogicalDevice, &createInfo, nullptr, &mTextureSampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler");
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Loads the vertices and vertex indexes contained in the object model into vertex storage.
    Creator:    John Cox, 02/2019
    ---------------------------------------------------------------------------------------------*/
    void LoadModel() {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn;
        std::string err;

        // Note: Faces can contain an arbitrary number of vertices, and we might try to load a 
        // model that was constructed with, say, quads, but LoadObj has an optional parameter 
        // "triangulate" and is set to true by default, so we don't need to worry about anything 
        // except triangles.
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, "models/chalet.obj")) {
            throw std::runtime_error(warn + err);
        }

        for (const auto &s : shapes) {
            std::cout << "shape name: " << s.name << std::endl;
            std::cout << "shape mesh face count: " << s.mesh.num_face_vertices.size() << std::endl;

            // there are some duplicate vertices in this mesh that we'll want to trim out
            // Note: The whole point of using vertex indices is to avoid duplicating vertices. An 
            // index is cheap to copy. A vertex much less so. There are 1/2 million faces in this 
            // model, ~700k vertices extracted (but only ~260k unique ones), and 1.5 million 
            // indices. Definitely want to avoid duplicating vertices when this much memory is at 
            // stake.
            std::unordered_map<Vertex, uint32_t> uniqueVertexIndices{};

            for (const auto &i : s.mesh.indices) {
                Vertex v{};
                v.pos = {
                    attrib.vertices[(3 * i.vertex_index) + 0],
                    attrib.vertices[(3 * i.vertex_index) + 1],
                    attrib.vertices[(3 * i.vertex_index) + 2],
                };
                v.texCoord = {
                    attrib.texcoords[(2 * i.texcoord_index) + 0],
                    1.0f - attrib.texcoords[(2 * i.texcoord_index) + 1],
                };
                v.color = { 1.0f, 1.0f, 1.0f };

                if (uniqueVertexIndices.count(v) == 0) {
                    // haven't seen this before, so store the current vertex index
                    uniqueVertexIndices[v] = static_cast<uint32_t>(mVertexes.size());
                    mVertexes.push_back(v);
                }

                mVertexIndices.push_back(uniqueVertexIndices.at(v));
            }
        }

        return;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Not all memory is created equal. Some is only available on the GPU ("device local"), some
        is visible to the host (the program), some is coherent (immediately copied to a duplicate
        chunk of GPU memory upon writing, though this has system-memory->GPU-memory transfer
        costs), etc.

        ??why are there a bunch of property-less pieces of memory??
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    uint32_t FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memoryProperties{};
        vkGetPhysicalDeviceMemoryProperties(mPhysicalDevice, &memoryProperties);
        for (uint32_t i = 0; i < memoryProperties.memoryTypeCount; i++) {
            //std::stringstream ss;
            //ss << "memory type[" << i << "], heap index '" << memoryProperties.memoryTypes[i].heapIndex << "':";
            //if ((memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT) != 0) {
            //    ss << " DEVICE_LOCAL";
            //}
            //if ((memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) != 0) {
            //    ss << " HOST_VISIBLE";
            //}
            //if ((memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT) != 0) {
            //    ss << " HOST_COHERENT";
            //}
            //if ((memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_HOST_CACHED_BIT) != 0) {
            //    ss << " HOST_CACHED";
            //}
            //if ((memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT) != 0) {
            //    ss << " LAZILY_ALLOCATED";
            //}
            //if ((memoryProperties.memoryTypes[i].propertyFlags & VK_MEMORY_PROPERTY_PROTECTED_BIT) != 0) {
            //    ss << " PROTECTED_BIT";
            //}
            //std::cout << ss.str() << std::endl;

            bool typeOk = (typeFilter & (1 << i)) != 0;

            // Problem: More than one memory property may be requested, so a simple bitwise 
            // AND != 0 check won't be suitable, and there may be more properties available than 
            // we are asking for, so a simple equivalence check won't work either. Solution: Check 
            // if the bitwise AND has all the requested properties.
            bool propertyOk = (memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
            if (typeOk && propertyOk) {
                return i;
            }
        }
        throw std::runtime_error("failed to find suitable memory type");
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Creates a VkBuffer of the requested type, then allocates memory for it with the requested
        memory usage and properties (specifying these allows the driver to optimize where it can).
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateBuffer(VkDeviceSize bufferSize, VkBufferUsageFlags bufferUsage, VkMemoryPropertyFlags memProperties, VkBuffer &buffer, VkDeviceMemory &bufferMemory) {
        VkBufferCreateInfo bufferCreateInfo{};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize;
        bufferCreateInfo.usage = bufferUsage;

        // only in use by one queue family (graphics queue)
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(mLogicalDevice, &bufferCreateInfo, nullptr, &buffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create vertex buffer");
        }

        VkMemoryRequirements memoryRequirements;
        vkGetBufferMemoryRequirements(mLogicalDevice, buffer, &memoryRequirements);

        VkMemoryAllocateInfo memoryAllocateInfo{};
        memoryAllocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memoryAllocateInfo.allocationSize = memoryRequirements.size;
        memoryAllocateInfo.memoryTypeIndex = FindMemoryType(memoryRequirements.memoryTypeBits, memProperties);
        if (vkAllocateMemory(mLogicalDevice, &memoryAllocateInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate vertex buffer memory");
        }

        uint32_t memoryOffset = 0;
        vkBindBufferMemory(mLogicalDevice, buffer, bufferMemory, memoryOffset);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Copies from the non-device-local host-visible buffer to the device-local non-host-visible
        buffer.
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    void CopyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
        VkCommandBuffer commandBuffer = BeginSingleUseCommandBuffer();

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        uint32_t regionCount = 1;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, &copyRegion);

        SubmitAndEndSingleUseCommandBuffer(commandBuffer);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        With not only vertex buffers needing copying but also texture images needing initial
        processing, this common code for the beginning of a one-time command buffer was broken
        into two dedicated functions to prevent copying.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    VkCommandBuffer BeginSingleUseCommandBuffer() {
        VkCommandBufferAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocateInfo.commandPool = mCommandPool;
        allocateInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
        vkAllocateCommandBuffers(mLogicalDevice, &allocateInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

        vkBeginCommandBuffer(commandBuffer, &beginInfo);
        return commandBuffer;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        This is the second half of the common code used in executing a one-time command buffer.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void SubmitAndEndSingleUseCommandBuffer(VkCommandBuffer commandBuffer) {
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        uint32_t submitCount = 1;
        VkFence nullFence = VK_NULL_HANDLE;
        vkQueueSubmit(mGraphicsQueue, submitCount, &submitInfo, nullFence);
        vkQueueWaitIdle(mGraphicsQueue);

        uint32_t commandBufferCount = 1;
        vkFreeCommandBuffers(mLogicalDevice, mCommandPool, commandBufferCount, &commandBuffer);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Creates a non-device-local but host-visible and host-coherent buffer, meaning that the
        buffer will reside in system memory, though it will have a GPU-memory equivalent, and the
        program will have write access and that it will be immediately uploaded to the GPU upon
        writing. This has memory copy costs though because the memory has to be sent to the GPU.
        The vertex data is copied to this buffer once.

        Then we create another buffer that is device local only and that is not host visible,
        meaning that the memory will reside on the GPU only and that the program will not write to
        it (that is, vkMapMemory(...) will blow up on us if we try to use it on this buffer's
        memory). We will then issue a command to copy the memory from the first buffer to the
        second, then dismiss the first buffer. The program will then run by accessing the
        device-local memory.
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateVertexBuffer() {
        VkDeviceSize bufferSize = sizeof(mVertexes[0]) * mVertexes.size();

        VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
        CreateBuffer(bufferSize, bufferUsage, memProperties, stagingBuffer, stagingBufferMemory);

        void *data = nullptr;
        VkDeviceSize offset = 0;
        VkMemoryMapFlags flags = 0;
        vkMapMemory(mLogicalDevice, stagingBufferMemory, offset, bufferSize, flags, &data);
        memcpy(data, mVertexes.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(mLogicalDevice, stagingBufferMemory);

        bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        memProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        CreateBuffer(bufferSize, bufferUsage, memProperties, mVertexBuffer, mVertexBufferMemory);

        CopyBuffer(stagingBuffer, mVertexBuffer, bufferSize);
        vkDestroyBuffer(mLogicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(mLogicalDevice, stagingBufferMemory, nullptr);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        To avoid duplicate vertex data, we will tell the drawing commands to use vertex by index
        instead of sequentially plucking out 3 vertices at a time from the vertex buffer. After
        setting this up, the drawing will sequentially pluck out 3 indices at a time from the
        index buffer, where it is cheaper to have duplicates (16bit duplicate indices much cheaper
        than duplicating an entire 2x 32bit position float + 3x 32bit color float vertex).
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateVertexIndexBuffer() {
        VkDeviceSize bufferSize = sizeof(mVertexIndices[0]) * mVertexIndices.size();

        VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
        CreateBuffer(bufferSize, bufferUsage, memProperties, stagingBuffer, stagingBufferMemory);

        void *data = nullptr;
        VkDeviceSize offset = 0;
        VkMemoryMapFlags flags = 0;
        vkMapMemory(mLogicalDevice, stagingBufferMemory, offset, bufferSize, flags, &data);
        memcpy(data, mVertexIndices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(mLogicalDevice, stagingBufferMemory);

        bufferUsage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        memProperties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
        CreateBuffer(bufferSize, bufferUsage, memProperties, mVertexIndexBuffer, mVertexIndexBufferMemory);
        CopyBuffer(stagingBuffer, mVertexIndexBuffer, bufferSize);
        vkDestroyBuffer(mLogicalDevice, stagingBuffer, nullptr);
        vkFreeMemory(mLogicalDevice, stagingBufferMemory, nullptr);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Creates a uniform buffer for every image in the swap chain so that we neither risk running
        over the current image-in-flight's uniforms nor have to wait for the image to finish
        before beginning the next one. With every image having it's own uniforms, we can go as
        fast as possible.

        Note: We're going to have upload new transforms every frame, so it would be pointless to
        create device-local memory that has to wait on a coherent staging buffer. We'll just use
        coherent memory (as soon as it is written to in system memory, it starts uploading to the
        GPU). The swap chain will give it a bit of time since it's busy with the current frame.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        mUniformBuffers.resize(mSwapChainImageViews.size());
        mUniformBuffersMemory.resize(mSwapChainImageViews.size());

        for (size_t i = 0; i < mSwapChainImageViews.size(); i++) {
            CreateBuffer(bufferSize, bufferUsage, memProperties, mUniformBuffers.at(i), mUniformBuffersMemory.at(i));
            // need to write new transforms every frame, so w'll do memory mapping later
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        For this tutorial at this stage (Texture mapping: Combined image sampler), we will
        allocate a UBO and a sampler for each possible frame.

        Note: We could do fewer than the swap chain image size, but then we might run out, and
        allocating more will leave some of them unused, and these things are cheap little
        structures anyway, so just allocate 1 for each frame.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateDescriptorPool() {
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes.at(0).type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes.at(0).descriptorCount = static_cast<uint32_t>(mSwapChainImageViews.size());
        poolSizes.at(1).type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes.at(1).descriptorCount = static_cast<uint32_t>(mSwapChainImageViews.size());

        VkDescriptorPoolCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        createInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        createInfo.pPoolSizes = poolSizes.data();
        createInfo.maxSets = static_cast<uint32_t>(mSwapChainImageViews.size());

        // Note: It is possible to create a "free descriptor set pool" via the 
        // VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT flag, in which descriptor sets can 
        // be freed and recreated at runtime. We won't be creating any descriptor sets on the fly, 
        // so the maximum number of descriptor sets is the same as the number required.
        createInfo.maxSets = static_cast<uint32_t>(mSwapChainImageViews.size());

        if (vkCreateDescriptorPool(mLogicalDevice, &createInfo, nullptr, &mDescriptorPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor pool");
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        A descriptor is something like a uniform or an image sampler. The descriptor (shorthand
        for "data descriptor") tells Vulkan where this information is coming from. Info on
        individual descriptors is generalized in the descriptor set layout setup and given exact
        info here via VkDescriptorBufferInfo (or VkDescriptorImageInfo, or others).

        Note: The designers of Vulkan did not expect users to have only one descriptor in their
        shaders (not most of the time, at least), so we do not see a VkDescriptor structure,
        instead only seeing VkDescriptorSet, indicating possibility for multiple descriptors. When
        you only have one descriptor, such as in this tutorial prior to image sampling, then the
        set is size 1 and the plural terminology is confusing.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateDescriptorSets() {
        // create duplicate descriptor set layouts (because Vulkan expects an array of them and 
        // cannot be told that they are all one and the same)
        std::vector<VkDescriptorSetLayout> layouts(mSwapChainImageViews.size(), mDescriptorSetLayout);
        VkDescriptorSetAllocateInfo allocateInfo{};
        allocateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocateInfo.descriptorPool = mDescriptorPool;
        allocateInfo.descriptorSetCount = static_cast<uint32_t>(mSwapChainImageViews.size());
        allocateInfo.pSetLayouts = layouts.data();

        // Note: This is an "allocate" function, not a "create" function, and it allocates from a 
        // pool that was already created, so we don't need to explicitly free or destroy the 
        // descriptor sets. They will be destroyed when the pool is.
        mDescriptorSets.resize(mSwapChainImageViews.size());
        if (vkAllocateDescriptorSets(mLogicalDevice, &allocateInfo, mDescriptorSets.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate descriptor sets");
        }

        // now we write info to the descriptor sets
        for (size_t i = 0; i < mSwapChainImageViews.size(); i++) {
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = mUniformBuffers.at(i);
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject);

            // arguably should be called VkDescriptorSamplerInfo, but eh
            // Note: The layout is the same value used when transitioning the texture image after 
            // copying.
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = mTextureImageView;
            imageInfo.sampler = mTextureSampler;

            // Note: The updating of descriptor sets expects a pointer to an array, so even if we 
            // only have a single descriptor, we will still need to pass a pointer to the "write" 
            // structure.
            std::array<VkWriteDescriptorSet, 2> descriptorWrites{};
            descriptorWrites.at(0).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites.at(0).dstSet = mDescriptorSets.at(i);
            descriptorWrites.at(0).dstBinding = 0;  // same as during layout setup
            descriptorWrites.at(0).dstArrayElement = 0;
            descriptorWrites.at(0).descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrites.at(0).descriptorCount = 1;
            descriptorWrites.at(0).pBufferInfo = &bufferInfo;

            descriptorWrites.at(1).sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrites.at(1).dstSet = mDescriptorSets.at(i);
            descriptorWrites.at(1).dstBinding = 1;  // same as during layout setup
            descriptorWrites.at(1).dstArrayElement = 0;
            descriptorWrites.at(1).descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrites.at(1).descriptorCount = 1;
            descriptorWrites.at(1).pImageInfo = &imageInfo;

            uint32_t writeCount = static_cast<uint32_t>(descriptorWrites.size());
            uint32_t copyCount = 0; // ??why would you copy descriptors at runtime??
            VkCopyDescriptorSet descriptorCopy{};
            vkUpdateDescriptorSets(mLogicalDevice, writeCount, descriptorWrites.data(), copyCount, &descriptorCopy);
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Generates a dedicated command buffer for each framebuffer with the same drawing commands
        in each.

        Note:
        - "Primary" level command buffers can be submitted to a queue for execution.
        - "Secondary" level command buffers cannot be submitted directly, but can be submitted
            from primary command buffers. Primary buffers cannot do this.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateCommandBuffers() {
        mCommandBuffers.resize(mSwapChainFramebuffers.size());

        // Note: "AllocateInfo", not "CreateInfo", because command buffers are allocated from the
        // command pool, not "created", which, in Vulkan terminology, involves creating memory 
        // for it. There is already memory in the command pool, so we're just allocating from it.
        VkCommandBufferAllocateInfo commandBufferAllocateInfo{};
        commandBufferAllocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        commandBufferAllocateInfo.commandPool = mCommandPool;
        commandBufferAllocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        commandBufferAllocateInfo.commandBufferCount = static_cast<uint32_t>(mCommandBuffers.size());
        if (vkAllocateCommandBuffers(mLogicalDevice, &commandBufferAllocateInfo, mCommandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers");
        }

        for (size_t i = 0; i < mCommandBuffers.size(); i++) {
            // type is actually a pointer, so non-reference assignment is ok
            auto currentCommandBuffer = mCommandBuffers.at(i);

            VkCommandBufferBeginInfo commandBufferBeginInfo{};
            commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            if (vkBeginCommandBuffer(currentCommandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer");
            }

            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderPass = mRenderPass;
            renderPassBeginInfo.framebuffer = mSwapChainFramebuffers.at(i);
            renderPassBeginInfo.renderArea.offset = { 0, 0 };
            renderPassBeginInfo.renderArea.extent = mSwapChainExtent;

            // defines the colors for the color and depth attachments' .loadOp = 
            // VK_ATTACHMENT_LOAD_OP_CLEAR
            // Note: For depth, clear to 1.0f because closer to 0 (as long as it is between 0-1) 
            // take precedence, so if we were to clear the depth to 0 (closest possible value), 
            // then the cleared color would be all that we would see. 
            // Also Note: The second depth clear value is "stencil", which is not in yse right now 
            // (1/20/2019).
            std::array<VkClearValue, 2> clearValues{};
            clearValues.at(0).color = { 0.0, 0.0f, 0.0f, 1.0f };
            clearValues.at(1).depthStencil = { 1.0f, 0 };
            renderPassBeginInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
            renderPassBeginInfo.pClearValues = clearValues.data();

            vkCmdBeginRenderPass(currentCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            {
                VkPipelineBindPoint graphicsBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
                vkCmdBindPipeline(currentCommandBuffer, graphicsBindPoint, mGraphicsPipeline);

                VkBuffer vertexBuffers[] = { mVertexBuffer };
                VkDeviceSize offsets[] = { 0 };
                uint32_t firstBindingIndex = Vertex::VERTEX_BUFFER_BINDING_LOCATION;
                uint32_t bindingCounter = 1;
                vkCmdBindVertexBuffers(currentCommandBuffer, firstBindingIndex, bindingCounter, vertexBuffers, offsets);

                VkDeviceSize offset = 0;
                vkCmdBindIndexBuffer(currentCommandBuffer, mVertexIndexBuffer, offset, VK_INDEX_TYPE_UINT32);

                uint32_t firstDescriptorSetIndex = 0;
                uint32_t descriptorSetCount = 1;
                uint32_t dynamicOffsetCount = 0;    // not considering dynamic descriptors now (1/1/2019)
                vkCmdBindDescriptorSets(
                    currentCommandBuffer,
                    graphicsBindPoint,
                    mPipelineLayout,
                    firstDescriptorSetIndex,
                    descriptorSetCount,
                    &mDescriptorSets.at(i),
                    dynamicOffsetCount,
                    nullptr);

                uint32_t indexCount = static_cast<uint32_t>(mVertexIndices.size());
                uint32_t instanceCount = 1;
                uint32_t firstIndex = 0;
                uint32_t vertexOffset = 0;  // you can add a constant offset to the indices (??why would you do that??)
                uint32_t firstInstance = 0;
                vkCmdDrawIndexed(currentCommandBuffer, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
            }
            vkCmdEndRenderPass(currentCommandBuffer);
            if (vkEndCommandBuffer(currentCommandBuffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer");
            }
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Generates the semaphores that will let the
        "acquire image" ->
        "execute command buffer" ->
        "return to swap chain" events occur in order.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateSyncObjects() {
        mSemaphoresImageAvailable.resize(MAX_FRAMES_IN_FLIGHT);
        mSemaphoresRenderFinished.resize(MAX_FRAMES_IN_FLIGHT);
        mInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreCreateInfo{};
        semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceCreateInfo{};
        fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            if (vkCreateSemaphore(mLogicalDevice, &semaphoreCreateInfo, nullptr, &mSemaphoresImageAvailable.at(i)) != VK_SUCCESS ||
                vkCreateSemaphore(mLogicalDevice, &semaphoreCreateInfo, nullptr, &mSemaphoresRenderFinished.at(i)) != VK_SUCCESS ||
                vkCreateFence(mLogicalDevice, &fenceCreateInfo, nullptr, &mInFlightFences.at(i)) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame");
            }
        }
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Called when GLFW detects a change in the size of the renderable area.

        Note: Created static because GLFW callback registration function was built with C-style
        function pointers in mind and therefore does not know how to call a member function with
        the correct "this" pointer.
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    static void FramebufferResizeCallback(GLFWwindow *window, int width, int height) {
        auto app = reinterpret_cast<HelloTriangleApplication*>(glfwGetWindowUserPointer(window));
        app->mFrameBufferResized = true;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Creates a GLFW window (??anything else??).
    Creator:    John Cox, 10/2018M
    ---------------------------------------------------------------------------------------------*/
    void InitWindow() {
        if (glfwInit() != GLFW_TRUE) {
            return;
        }

        // Note: GLFW was originally designed to create an OpenGL context. Possible "client API"
        // values are OpenGL or OpenGL ES. At this time (10/20/2018), there is no option for
        // Vulkan, so tell it to use "no API".
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        GLFWmonitor *monitor = nullptr;
        mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "Vulkan", monitor, nullptr);
        glfwSetWindowUserPointer(mWindow, this);    // gets a class pointer into callback function
        glfwSetFramebufferSizeCallback(mWindow, FramebufferResizeCallback);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Governs the initialization of a Vulkan instance and devices.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void InitVulkan() {
        CreateInstance();
        CreateSurface();
        PickPhysicalDevice();
        CreateLogicalDevice();
        CreateSwapChain();
        CreateRenderPass();
        CreateDescriptorSetLayout();
        CreateGraphicsPipeline();
        CreateCommandPool();
        CreateDepthResources();
        CreateFramebuffers();
        CreateTextureImage();
        CreateTextureSampler();
        LoadModel();
        CreateVertexBuffer();
        CreateVertexIndexBuffer();
        CreateUniformBuffers();
        CreateDescriptorPool();
        CreateDescriptorSets();
        CreateCommandBuffers();
        CreateSyncObjects();
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Updates the uniform buffer for the current frame so that the vertex shader will have the
        correct transforms.

        Note: GLM was designed for OpenGL ("GLM" = "GL Math"), in which the Y values in the
        Normalized Device Coordinates (NDC) increased from bottom to top. This was chosen because
        cartesian coordinates in 2D graphs start at (0,0) and increase going right (+x) and up
        (+y). Early text editors were terminals (ex: Command Prompt in Windows), in which the
        first character was entered in the upper left, as we English speakers expect of our text,
        and early image processing (ASCII characters, not pixels) leveraged this text mechanism,
        thereby declaring (0,0) to also be the upper left, and it never changed when image
        processing became pixel-based and there was no longer any technical reason to be the upper
        left. Direct3D followed image processing. And OpenGL didn't change, so as far as the rest
        of the image processing world is concerned, OpenGL (and therefore GLM) is upside down.

        That's why we flip projection's Y.
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void UpdateUniformBuffer(uint32_t swapChainImageIndex) {
        static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};

        // starting with identity matrix, rotate with elapsed time around the Z axis
        //ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.model = glm::rotate(glm::mat4(1.0f), glm::radians(220.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        // eye at (2,2,2), looking at (0,0,0), with Z axis as "up"
        // Note: Flip the camera's "up" (in this case Z) axis from + to - as an alternative to 
        // dealing with the counterclockwise face culling problem that is currently dealt with by 
        // flipping one of the projection transform's Y axes.
        float zoomAxis = sinf(time / 2.0f);
        ubo.view = glm::lookAt(glm::vec3(2.0f + zoomAxis, 2.0f + zoomAxis, 2.0f + zoomAxis), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view[1][1] *= +1;

        // Note: If the screen was resized and the swap chain had to be recreated, this 
        // calculation will use the latest values, and so RecreateSwapChain(...) doesn't have to 
        // handle that.
        // Also Note: We *could* make it do that because the perspective projection (that is, the 
        // camera->projection transform) is constant for a given screen size, but we aren't 
        // because then we'd have to split the UBO population between this function and that 
        // function, and that would be a bit of an encapsulation pain.
        float aspectRatio = mSwapChainExtent.width / static_cast<float>(mSwapChainExtent.height);
        float nearPlaneDist = 0.1f;
        float farPlaneDist = 10.0f;
        ubo.proj = glm::perspective(glm::radians(45.0f), aspectRatio, nearPlaneDist, farPlaneDist);

        // Note: See the vertex structure's description for more detail, but in essance, this 
        // tutorial is considered counterclockwise faces to be the front, but positioning the 
        // camera ("eye"/"view") as it is moves the vertices such that they draw clockwise, and so 
        // the tutorial flipped the Y on the projection matrix. I experimented and found that 
        // flipping view[1][1] in the same way doesn't work, so this hack on the Y axis only works 
        // in the projection matrix. This could also be achieved by flipping the camera's Z axis 
        // over from + to -, or by rearranging the vertices to draw differently. For whatever 
        // reason, he went with this approach. In the long rong, it is probably not important since 
        // we'll start importing thousands of vertices or more from scene files.
        ubo.proj[1][1] *= -1;

        void *data = nullptr;
        VkDeviceSize offset = 0;
        VkMemoryMapFlags flags = 0;
        vkMapMemory(mLogicalDevice, mUniformBuffersMemory.at(swapChainImageIndex), offset, sizeof(ubo), flags, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(mLogicalDevice, mUniformBuffersMemory.at(swapChainImageIndex));
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        (1) Acquires an image from the swap chain
        (2) Executes the command buffer with that image as attachment in the framebuffer (??what??)
        (3) Returns the image to the swap chain for presentation

        The tutorial says that these events are executed asynchronously unless things are put in
        place to guarantee that they execute in order. There are two synchronization mechanisms:
        - Fence: Application can wait via vkWaitForFences(...) until the fence(s) are clear.
        - Semaphore: Cannot be accessed from the application. These are used for the GPu to
            synchronize operations within or accross command queues.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void DrawFrame() {
        // don't submit another queue of graphics commands until 
        size_t inflightFrameIndex = mCurrentFrame % MAX_FRAMES_IN_FLIGHT;
        VkFence *pCurrentFrameFence = &mInFlightFences.at(inflightFrameIndex);
        VkBool32 waitAllFences = VK_TRUE; // we only wait on one fence, so "wait all" irrelevant
        uint64_t timeout_ns = std::numeric_limits<uint64_t>::max();
        vkWaitForFences(mLogicalDevice, 1, pCurrentFrameFence, waitAllFences, timeout_ns);

        uint32_t imageIndex = 0;
        VkFence nullFence = VK_NULL_HANDLE;
        VkResult result = vkAcquireNextImageKHR(mLogicalDevice, mSwapChain, timeout_ns, mSemaphoresImageAvailable.at(inflightFrameIndex), nullFence, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            RecreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image");
        }

        // only reset the fences once we're good to go (that is, have image and swap chain is not 
        // out of date)
        vkResetFences(mLogicalDevice, 1, pCurrentFrameFence);

        UpdateUniformBuffer(imageIndex);

        // submit the command buffer for this image
        // Note: In short, this reads, "wait for 'image available semaphore', execute command 
        // buffer on the render passes' color attachment, then raise the 'render finished' 
        // semaphore".
        VkSemaphore waitSemaphores[] = { mSemaphoresImageAvailable.at(inflightFrameIndex) };
        VkSemaphore signalSemaphores[] = { mSemaphoresRenderFinished.at(inflightFrameIndex) };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &mCommandBuffers.at(imageIndex);
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        // once all commands have been completed, the provided fence will be signaled
        uint32_t submitCount = 1;
        if (vkQueueSubmit(mGraphicsQueue, submitCount, &submitInfo, *pCurrentFrameFence) != VK_SUCCESS) {
            throw std::runtime_error("failed to submit draw command buffer");
        }

        VkSwapchainKHR swapChains[] = { mSwapChain };
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores; // wait on command buffer
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        // used if there were multiple swap chains that could return success/fail statuses, but 
        // we're just using a single swap chain, so we don't need this
        presentInfo.pResults = nullptr;

        result = vkQueuePresentKHR(mPresentationQueue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || mFrameBufferResized) {
            RecreateSwapChain();
            mFrameBufferResized = false;
        }
        else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image");
        }

        //vkQueueWaitIdle(mPresentationQueue);
        mCurrentFrame++;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Responsible for the "while(not exit trigger) do program" loop.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void MainLoop() {
        while (!glfwWindowShouldClose(mWindow)) {
            glfwPollEvents();
            DrawFrame();
        }
        vkDeviceWaitIdle(mLogicalDevice);
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Governs cleanup.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void Cleanup() {
        CleanupSwapChain();

        vkDestroySampler(mLogicalDevice, mTextureSampler, nullptr);
        vkDestroyImageView(mLogicalDevice, mTextureImageView, nullptr);
        vkDestroyImage(mLogicalDevice, mTextureImage, nullptr);
        vkFreeMemory(mLogicalDevice, mTextureImageMemory, nullptr);

        vkDestroyDescriptorSetLayout(mLogicalDevice, mDescriptorSetLayout, nullptr);
        for (size_t i = 0; i < mSwapChainImageViews.size(); i++) {
            vkDestroyBuffer(mLogicalDevice, mUniformBuffers.at(i), nullptr);
            vkFreeMemory(mLogicalDevice, mUniformBuffersMemory.at(i), nullptr);
        }
        vkDestroyDescriptorPool(mLogicalDevice, mDescriptorPool, nullptr);

        vkDestroyBuffer(mLogicalDevice, mVertexBuffer, nullptr);
        vkFreeMemory(mLogicalDevice, mVertexBufferMemory, nullptr);
        vkDestroyBuffer(mLogicalDevice, mVertexIndexBuffer, nullptr);
        vkFreeMemory(mLogicalDevice, mVertexIndexBufferMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(mLogicalDevice, mSemaphoresImageAvailable.at(i), nullptr);
            vkDestroySemaphore(mLogicalDevice, mSemaphoresRenderFinished.at(i), nullptr);
            vkDestroyFence(mLogicalDevice, mInFlightFences.at(i), nullptr);
        }
        vkDestroyCommandPool(mLogicalDevice, mCommandPool, nullptr);
        vkDestroyDevice(mLogicalDevice, nullptr);
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);

        if (mCallback != VK_NULL_HANDLE) {
            // Note: This is an externally synchronized object (that is, created at runtime in a 
            // forked thread), and so *must* not be called when a callback is active. (??how to enforce??)
            DestroyDebugUtilsMessengEXT(mInstance, mCallback, nullptr);
        }

        vkDestroyInstance(mInstance, nullptr);
        glfwDestroyWindow(mWindow);
        glfwTerminate();
    }
};


/*-------------------------------------------------------------------------------------------------
Description:
    Startup and tells the program to go. Encases everything in a try-catch-all block to ensure
    that, regardless of what happens, destructors are called that clean up their own resources.

    Note: Also serves as a misc "don't know where to put this yet" when still figuring things out.
    As more stuff is added, I'll figure out how things are working, get an idea of how it *should*
    work, and move the code to an appropriate class.
Creator:    John Cox, 10/2018
-------------------------------------------------------------------------------------------------*/
int main() {
    try {
        HelloTriangleApplication app;
        app.Run();
    }
    catch (const std::exception& e) {
        std::cout << "Try-Catch All triggered: " << std::endl
            << e.what() << std::endl;
    }

    system("pause");
    return 0;
}
