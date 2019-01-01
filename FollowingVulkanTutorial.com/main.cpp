//#define GLFW_INCLUDE_VULKAN
//#include <GLFW/glfw3.h>
#include "vulkan_pch.h"

#define GLM_FORCE_RADIANS
#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include <glm/mat4x4.hpp>
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
    glm::vec2 pos;
    glm::vec3 color;

    // "5" in order to demonstrate that we do not have to use 0 just because we only have one 
    // vertex buffer at this time (12/29/2019) (could have used 3, or 7, or whatever)
    static const uint32_t VERTEX_BUFFER_BINDING_LOCATION = 5;

    static VkVertexInputBindingDescription GetBindingDescription() {
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = VERTEX_BUFFER_BINDING_LOCATION;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    static std::array<VkVertexInputAttributeDescription, 2> GetAttributeDescription() {
        std::array<VkVertexInputAttributeDescription, 2> attributeDescriptions{};

        // "pos" hijacks the color format enum to say "two 32bit floats"
        attributeDescriptions[0].binding = VERTEX_BUFFER_BINDING_LOCATION;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);

        attributeDescriptions[1].binding = VERTEX_BUFFER_BINDING_LOCATION;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32A32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, color);
        return attributeDescriptions;
    }
};

/*-------------------------------------------------------------------------------------------------
Description:
    In Vulkan, unlike OpenGL, Normalized Device Coordinate (NDC) (-1,-1) is the top left like
    Direct3D. In OpenGL, NDC (-1,-1) was the bottom left.
Creator:    John Cox, 12/2018
-------------------------------------------------------------------------------------------------*/
const std::vector<Vertex> gVertexValues = {
    {{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
    {{+0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
    {{+0.5f, +0.5f}, {0.0f, 0.0f, 1.0f}},
    {{-0.5f, +0.5f}, {1.0f, 1.0f, 1.0f}},
};

/*-------------------------------------------------------------------------------------------------
Description:
    If we have fewer than 65535 (2^16-1) unique vertices, we can save space on indices by using 
    16bit unsigned integers. If we have more than that many unique vertices, then we will have to 
    use 32bit unsigned integer indices.
Creator:    John Cox, 12/2018
-------------------------------------------------------------------------------------------------*/
const std::vector<uint16_t> gVertexIndices = {
    0,1,2,
    2,3,0,
};

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
    const int MAX_FRAMES_IN_FLIGHT = 2;

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
    std::vector<VkImage> mSwapChainImages;
    VkFormat mSwapChainImageFormat = VkFormat::VK_FORMAT_UNDEFINED;
    VkExtent2D mSwapChainExtent{};
    std::vector<VkImageView> mSwapChainImageViews;
    std::vector<VkFramebuffer> mSwapChainFramebuffers;
    VkRenderPass mRenderPass;
    VkDescriptorSetLayout mDescriptorSetLayout;
    VkPipelineLayout mPipelineLayout;    //??what is this??
    VkPipeline mGraphicsPipeline;
    VkCommandPool mCommandPool;
    std::vector<VkCommandBuffer> mCommandBuffers;
    std::vector<VkSemaphore> mSemaphoresImageAvailable;
    std::vector<VkSemaphore> mSemaphoresRenderFinished;
    std::vector<VkFence> mInFlightFences;
    size_t mCurrentFrame = 0;
    bool mFrameBufferResized = false;   // not all drivers properly handle window resize notifications in Vulkan
    VkBuffer mVertexBuffer;
    VkDeviceMemory mVertexBufferMemory;
    VkBuffer mVertexIndexBuffer;
    VkDeviceMemory mVertexIndexBufferMemory;
    std::vector<VkBuffer> mUniformBuffers;
    std::vector<VkDeviceMemory> mUniformBuffersMemory;

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

        // using a std::set(...) because it is possible that the queue that supports surface drawing is the same as the graphics queue, and don't want to create duplicate queues (??wat??)
        std::set<uint32_t> uniqueQueueFamiliesIndices{
            indices.graphicsFamily.value(),
            indices.presentationFamily.value()
        };  //??why a set??
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
        mSwapChainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(mLogicalDevice, mSwapChain, &imageCount, mSwapChainImages.data());

        // also store these for later
        mSwapChainImageFormat = surfaceFormat.format;
        mSwapChainExtent = extent;
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        This code is used during swap chain recreation, so it was moved from program-end Cleanup()
        to here.
    Creator:    John Cox, 12/2018
    ---------------------------------------------------------------------------------------------*/
    void CleanupSwapChain() {
        for (auto &framebuffer : mSwapChainFramebuffers) {
            vkDestroyFramebuffer(mLogicalDevice, framebuffer, nullptr);
        }
        vkFreeCommandBuffers(mLogicalDevice, mCommandPool, static_cast<uint32_t>(mCommandBuffers.size()), mCommandBuffers.data());
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
        CreateImageViews();
        CreateRenderPass();
        // Note: Not recreating the descriptor set layout because those are independent of image.
        CreateGraphicsPipeline();
        CreateFramebuffers();
        CreateCommandBuffers();
    }


    /*---------------------------------------------------------------------------------------------
    Description:
        Creates an "image view" object for each image in the swap chain. A "view" tells Vulkan how
        to handle the array of data that we generically call "image". Not every "image" is a
        finished drawing though. It may be a rendering from a particular point of view and will be
        used as a texture, or maybe it is a lighting map with simple intensity values, or maybe it
        really is a finished array of pixel colors.
    Creator:    John Cox, 11/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateImageViews() {
        //??is mSwapChainImages just a temporary value? can I delete it an only use it here??
        mSwapChainImageViews.resize(mSwapChainImages.size());

        for (size_t i = 0; i < mSwapChainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = mSwapChainImages.at(i);
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;    // 2D texture
            createInfo.format = mSwapChainImageFormat;

            // we could mess with the mappings of components.r, components.g, etc. and set all 
            // color components in the view to be "red" (VK_COMPONENT_SWIZZLE_R), thus giving us a 
            // monochrome red shade, or we could leave them at their default values 
            // (VK_COMPONENT_SWIZZLE_IDENTITY = 0), which is what we're going to do here 
            createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

            // using the image as a simple color texture target ("target" being what is rendered 
            // to) with no mipmapping 
            // Note: Only 1 layer since we are not doing steroscopic 3D (that needs 2).
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            // swap chain is created from the logical device, and so the image views of the swap 
            // chain's images are also created from logical device
            if (vkCreateImageView(mLogicalDevice, &createInfo, nullptr, &mSwapChainImageViews.at(i)) != VK_SUCCESS) {
                throw std::runtime_error("failed to create image views");
            }
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

        // "Every subpass references one or more of the attachments that we've described using the 
        // structure in the previous sections."
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;  // index into attachment descriptions array
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        //??multiple attachments in a single subpass??
        // Note: "The index of the attachment in this array is directly referenced form the 
        // fragment shader with `layout(location=0) out vec4 outColor`."
        //??what? tinker with this once you can draw something??
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        // Note: There is an implit "subpass" that occurs at the start of the pipeline and another 
        // at the end. There may not be an image available yet by the time the command buffer 
        // pipeline is ready to execute. Insert a subpass dependency to make sure that this 
        // implicit subpass doesn't begin prematurely.
        //??what? are you sure this is correct??
        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;    // "external" meaning the implict subpasses
        dependency.dstSubpass = 0;  // index 0 indicates our subpass (the only one right now)
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // finally, make the render pass object itself
        VkRenderPassCreateInfo renderPassCreateInfo{};
        renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassCreateInfo.attachmentCount = 1;
        renderPassCreateInfo.pAttachments = &colorAttachmentDesc;
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
        buffer objects, each with a a set of transforms, for every "bone" in a skeletal animation.
        We're just using a single descriptor though, so our descriptor count is only 1;
    Creator:    John Cox, 01/2019
    ---------------------------------------------------------------------------------------------*/
    void CreateDescriptorSetLayout() {
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0;   // same binding location as in the shader that uses it
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        uboLayoutBinding.descriptorCount = 1;
        uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
        uboLayoutBinding.pImmutableSamplers = nullptr;  // no textures yet at this time (1-1-2019)

        VkDescriptorSetLayoutCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        createInfo.bindingCount = 1;
        createInfo.pBindings = &uboLayoutBinding;

        if (vkCreateDescriptorSetLayout(mLogicalDevice, &createInfo, nullptr, &mDescriptorSetLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create descriptor set layout!");
        }
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
            mixed based upon transparency. This stage is FIXED.
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
        rasterizerCreateInfo.frontFace = VK_FRONT_FACE_CLOCKWISE;
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
        pipelineCreateInfo.subpass = 0;
        pipelineCreateInfo.basePipelineHandle = VK_NULL_HANDLE; // not deriving from existing pipeline

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
            VkImageView attachments[] = {
                mSwapChainImageViews[i]
            };
            VkFramebufferCreateInfo frameBufferCreateInfo{};
            frameBufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            frameBufferCreateInfo.renderPass = mRenderPass;
            frameBufferCreateInfo.attachmentCount = 1;
            frameBufferCreateInfo.pAttachments = attachments;
            frameBufferCreateInfo.width = mSwapChainExtent.width;
            frameBufferCreateInfo.height = mSwapChainExtent.height;
            frameBufferCreateInfo.layers = 1;

            if (vkCreateFramebuffer(mLogicalDevice, &frameBufferCreateInfo, nullptr, &mSwapChainFramebuffers[i]) != VK_SUCCESS) {
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
        Generates a dedicated command buffer for each framebuffer.

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
            auto currentCommandBuffer = mCommandBuffers[i];

            VkCommandBufferBeginInfo commandBufferBeginInfo{};
            commandBufferBeginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            commandBufferBeginInfo.flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            if (vkBeginCommandBuffer(currentCommandBuffer, &commandBufferBeginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording command buffer");
            }

            VkRenderPassBeginInfo renderPassBeginInfo{};
            renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassBeginInfo.renderPass = mRenderPass;
            renderPassBeginInfo.framebuffer = mSwapChainFramebuffers[i];
            renderPassBeginInfo.renderArea.offset = { 0, 0 };
            renderPassBeginInfo.renderArea.extent = mSwapChainExtent;

            // defines the colors for the color attachment's .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR
            VkClearValue clearColor = { 0.0, 0.0f, 0.0f, 1.0f };
            renderPassBeginInfo.clearValueCount = 1;
            renderPassBeginInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(currentCommandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);
            {
                vkCmdBindPipeline(currentCommandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, mGraphicsPipeline);

                VkBuffer vertexBuffers[] = { mVertexBuffer };
                VkDeviceSize offsets[] = { 0 };
                uint32_t firstBindingIndex = Vertex::VERTEX_BUFFER_BINDING_LOCATION;
                uint32_t bindingCounter = 1;
                vkCmdBindVertexBuffers(currentCommandBuffer, firstBindingIndex, bindingCounter, vertexBuffers, offsets);

                VkDeviceSize offset = 0;
                vkCmdBindIndexBuffer(currentCommandBuffer, mVertexIndexBuffer, offset, VK_INDEX_TYPE_UINT16);

                uint32_t indexCount = static_cast<uint32_t>(gVertexIndices.size());
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
            if (vkCreateSemaphore(mLogicalDevice, &semaphoreCreateInfo, nullptr, &mSemaphoresImageAvailable[i]) != VK_SUCCESS ||
                vkCreateSemaphore(mLogicalDevice, &semaphoreCreateInfo, nullptr, &mSemaphoresRenderFinished[i]) != VK_SUCCESS ||
                vkCreateFence(mLogicalDevice, &fenceCreateInfo, nullptr, &mInFlightFences[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to create synchronization objects for a frame");
            }
        }
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

        VkBufferCopy copyRegion{};
        copyRegion.size = size;
        uint32_t regionCount = 1;
        vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, regionCount, &copyRegion);
        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        uint32_t submitCount = 1;
        vkQueueSubmit(mGraphicsQueue, submitCount, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(mGraphicsQueue);

        uint32_t commandBufferCount = 1;
        vkFreeCommandBuffers(mLogicalDevice, mCommandPool, commandBufferCount, &commandBuffer);
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
        VkDeviceSize bufferSize = sizeof(gVertexIndices[0]) * gVertexIndices.size();

        VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
        CreateBuffer(bufferSize, bufferUsage, memProperties, stagingBuffer, stagingBufferMemory);

        void *data = nullptr;
        VkDeviceSize offset = 0;
        VkMemoryMapFlags flags = 0;
        vkMapMemory(mLogicalDevice, stagingBufferMemory, offset, bufferSize, flags, &data);
        memcpy(data, gVertexIndices.data(), static_cast<size_t>(bufferSize));
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
    Creator:    John Cox, 01/2018
    ---------------------------------------------------------------------------------------------*/
    void CreateUniformBuffers() {
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);
        VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        mUniformBuffers.resize(mSwapChainImages.size());
        mUniformBuffersMemory.resize(mSwapChainImages.size());

        for (size_t i = 0; i < mSwapChainImages.size(); i++) {
            CreateBuffer(bufferSize, bufferUsage, memProperties, mUniformBuffers[i], mUniformBuffersMemory[i]);
            // need to write new transforms every frame, so w'll do memory mapping later
        }
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
        VkDeviceSize bufferSize = sizeof(gVertexValues[0]) * gVertexValues.size();
        
        VkBufferUsageFlags bufferUsage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        VkMemoryPropertyFlags memProperties = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        VkBuffer stagingBuffer = VK_NULL_HANDLE;
        VkDeviceMemory stagingBufferMemory = VK_NULL_HANDLE;
        CreateBuffer(bufferSize, bufferUsage, memProperties, stagingBuffer, stagingBufferMemory);

        void *data = nullptr;
        VkDeviceSize offset = 0;
        VkMemoryMapFlags flags = 0;
        vkMapMemory(mLogicalDevice, stagingBufferMemory, offset, bufferSize, flags, &data);
        memcpy(data, gVertexValues.data(), static_cast<size_t>(bufferSize));
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
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void InitWindow() {
        glfwInit();

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
        CreateImageViews();
        CreateRenderPass();
        CreateDescriptorSetLayout();
        CreateGraphicsPipeline();
        CreateFramebuffers();
        CreateCommandPool();
        CreateVertexBuffer();
        CreateVertexIndexBuffer();
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
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        
        // eye at (2,2,2), looking at (0,0,0), with Z axis as "up"
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

        float aspectRatio = mSwapChainExtent.width / static_cast<float>(mSwapChainExtent.height);
        float nearPlaneDist = 0.1f;
        float farPlaneDist = 10.0f;
        ubo.proj = glm::perspective(glm::radians(45.0f), aspectRatio, nearPlaneDist, farPlaneDist);
        ubo.proj[1][1] *= -1;   // flip the Y (don't know why this math works, but it does)

        void *data = nullptr;
        VkDeviceSize offset = 0;
        VkMemoryMapFlags flags = 0;
        vkMapMemory(mLogicalDevice, mUniformBuffersMemory[swapChainImageIndex], offset, sizeof(ubo), flags, &data);
        memcpy(data, &ubo, sizeof(ubo));
        vkUnmapMemory(mLogicalDevice, mUniformBuffersMemory[swapChainImageIndex]);
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
        VkFence *pCurrentFrameFence = &mInFlightFences[inflightFrameIndex];
        VkBool32 waitAllFences = VK_TRUE; // we only wait on one fence, so "wait all" irrelevant
        uint64_t timeout_ns = std::numeric_limits<uint64_t>::max();
        vkWaitForFences(mLogicalDevice, 1, pCurrentFrameFence, waitAllFences, timeout_ns);

        uint32_t imageIndex = 0;
        VkFence nullFence = VK_NULL_HANDLE;
        VkResult result = vkAcquireNextImageKHR(mLogicalDevice, mSwapChain, timeout_ns, mSemaphoresImageAvailable[inflightFrameIndex], nullFence, &imageIndex);
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
        VkSemaphore waitSemaphores[] = { mSemaphoresImageAvailable[inflightFrameIndex] };
        VkSemaphore signalSemaphores[] = { mSemaphoresRenderFinished[inflightFrameIndex] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &mCommandBuffers[imageIndex];
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

        // TODO: change uniform buffer binding to, say, 7
        // TODO: replace all vector/array accesses of [i] with .at(i)

        vkDestroyDescriptorSetLayout(mLogicalDevice, mDescriptorSetLayout, nullptr);
        for (size_t i = 0; i < mSwapChainImages.size(); i++) {
            vkDestroyBuffer(mLogicalDevice, mUniformBuffers[i], nullptr);
            vkFreeMemory(mLogicalDevice, mUniformBuffersMemory[i], nullptr);
        }

        vkDestroyBuffer(mLogicalDevice, mVertexBuffer, nullptr);
        vkFreeMemory(mLogicalDevice, mVertexBufferMemory, nullptr);
        vkDestroyBuffer(mLogicalDevice, mVertexIndexBuffer, nullptr);
        vkFreeMemory(mLogicalDevice, mVertexIndexBufferMemory, nullptr);

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(mLogicalDevice, mSemaphoresImageAvailable[i], nullptr);
            vkDestroySemaphore(mLogicalDevice, mSemaphoresRenderFinished[i], nullptr);
            vkDestroyFence(mLogicalDevice, mInFlightFences[i], nullptr);
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

    return 0;
}
