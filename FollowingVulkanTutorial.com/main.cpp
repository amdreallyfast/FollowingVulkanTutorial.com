#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>      // for format-capable stringstream
#include <iomanip>      // for std::setfill(...) and std::setw(...)
#include <vector>
#include <set>          // for eliminating potentially duplicate stuff from multiple objects
#include <optional>     // for return values that may not exist
#include <algorithm>    // std::min/max

//VKAPI_ATTR VkBool32 VKAPI_CALL DebugCallback(
//    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
//    VkDebugUtilsMessageTypeFlagsEXT messageType,
//    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
//    void *pUserData) {
//
//    std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
//    return VK_FALSE;
//}
//
//VkResult CreateDebugUtilsMessengerEXT(
//    VkInstance instance,
//    const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
//    const VkAllocationCallbacks *pAllocator,
//    VkDebugUtilsMessengerEXT *pCallback) {
//
//    // Note: It seems that static_cast<...> doesn't work. Use the C-style forced cast.
//    auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT");
//    if (func != nullptr) {
//        return func(instance, pCreateInfo, pAllocator, pCallback);
//    }
//    else {
//        return VK_ERROR_EXTENSION_NOT_PRESENT;
//    }
//}
//
//std::vector<const char *> GetRequiredExtensions() {
//    uint32_t glfwExtensionCount = 0;
//    const char ** glfwExtensions;
//    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
//    std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
//
//    // also want the "debug utils"
//    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
//    return extensions;
//}
//
//int main(int argc, char *argv[]) {
//    glfwInit();
//
//    // Note: GLFW was originally designed to create an OpenGL context. Possible "client API"
//    // values are OpenGL or OpenGL ES. At this time (10/20/2018), there is no option for
//    // Vulkan, so tell it to use "no API".
//    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
//    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // disable resizing for now
//
//    GLFWmonitor *monitor = nullptr;
//    GLFWwindow *window = glfwCreateWindow(800, 600, "Vulkan window", monitor, nullptr);
//
//    // create the instance
//    VkApplicationInfo appInfo{};
//    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
//    appInfo.pApplicationName = "Hello Triangle";
//    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
//    appInfo.pEngineName = "No Engine";
//    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
//    appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 82);
//
//    auto requiredExtensions = GetRequiredExtensions();
//    const std::vector<const char *> requiredValidationLayers{
//        "VK_LAYER_LUNARG_standard_validation",
//    };
//
//    // create the "debug utils EXT" callback object
//    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
//    debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
//    debugCreateInfo.messageSeverity =
//        VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
//        //VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
//        VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
//        VkDebugUtilsMessageSeverityFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
//    debugCreateInfo.messageType =
//        VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
//        VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
//        VkDebugUtilsMessageTypeFlagBitsEXT::VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
//    debugCreateInfo.pfnUserCallback = DebugCallback;    // global function
//    debugCreateInfo.pUserData = nullptr;
//
//    VkInstanceCreateInfo instanceCreateInfo{};
//    instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
//    instanceCreateInfo.pApplicationInfo = &appInfo;
//    instanceCreateInfo.enabledExtensionCount = static_cast<uint32_t>(requiredExtensions.size());
//    instanceCreateInfo.ppEnabledExtensionNames = requiredExtensions.data();
//    instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
//    instanceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();
//    instanceCreateInfo.pNext = &debugCreateInfo;
//
//    VkInstance instance;
//    if (vkCreateInstance(&instanceCreateInfo, nullptr, &instance) != VK_SUCCESS) {
//        throw std::runtime_error("failed to create instance");
//    }
//
//    VkDebugUtilsMessengerEXT callback;
//    if (CreateDebugUtilsMessengerEXT(instance, &debugCreateInfo, nullptr, &callback) != VK_SUCCESS) {
//        throw std::runtime_error("failed to set up debug callback");
//    }
//
//    // do the main loop here
//
//    // intentionally destroying instance without destroying the callback to try to trigger error
//    vkDestroyInstance(instance, nullptr);
//    glfwDestroyWindow(window);
//    glfwTerminate();
//}






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
    The class for this tutorial series.
Creator:    John Cox, 10/2018
-------------------------------------------------------------------------------------------------*/
class HelloTriangleApplication {
private:
    GLFWwindow *mWindow = nullptr;
    uint32_t mWindowWidth = 800;
    uint32_t mWindowHeight = 600;

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

    const std::vector<const char *> mRequiredDeviceExtensions {
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

#ifndef PRINT_EXTENSIONS
        // carry on
#else
        std::stringstream ss;
        std::string name("    Name: ");
        std::string spec("Spec Version: ");

        std::cout << "Instance extensions (GLFW) required:" << std::endl;
        ss << std::setfill(' ') << std::left;
        for (uint32_t i = 0; i < glfwExtensionCount; i++) {
            ss << std::setw(name.length()) << name
                << std::setw(40) << glfwExtensions[i] << std::endl;
        }
        std::cout << ss.str();

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
        const std::vector<const char *> requiredValidationLayers {
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
        If the preferred (??why??) format (B8G8R8A8 nonlinear (??what??)) is available, return 
        that. Else return the first available supported image format.
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
        if (capabilities.currentExtent.width == std::numeric_limits<uint32_t>::max()) {
            printf("");
        }

        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
            // window manager will select image extent for us
            return capabilities.currentExtent;
        }

        // manually clamp to window size
        // Note: Surface capability can be ??how big? how small??
        uint32_t width = std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, mWindowWidth));
        uint32_t height = std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, mWindowHeight));
        return VkExtent2D{ width, height };
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
        ??
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
        with "front" and "back" buffers (where "buffer" means "a pixel array"), Vulkan has a 
        "swap chain", which is a set of image buffers that can be swapped out as desired. This "chain" (??is it a linked list??) has more flexibility than OpenGL's framebuffer. 
        
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

        // all that to lead up to this
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
        Creates a GLFW window (??anything else??).
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void InitWindow() {
        glfwInit();

        // Note: GLFW was originally designed to create an OpenGL context. Possible "client API"
        // values are OpenGL or OpenGL ES. At this time (10/20/2018), there is no option for
        // Vulkan, so tell it to use "no API".
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

        // resizing takes special care; we'll handle that later; disable resizing for now
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

        GLFWmonitor *monitor = nullptr;
        mWindow = glfwCreateWindow(mWindowWidth, mWindowHeight, "Vulkan", monitor, nullptr);
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
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Responsible for the "while(not exit trigger) do program" loop.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void MainLoop() {
        //while (!glfwWindowShouldClose(mWindow)) {
        //    glfwPollEvents();
        //}
    }

    /*---------------------------------------------------------------------------------------------
    Description:
        Governs cleanup.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    void Cleanup() {
        if (mCallback != 0) {
            // Note: This is an externally synchronized object (that is, created at runtime in a 
            // forked thread), and so *must* not be called when a callback is active. (??how to enforce??)
            DestroyDebugUtilsMessengEXT(mInstance, mCallback, nullptr);
        }
        
        vkDestroySwapchainKHR(mLogicalDevice, mSwapChain, nullptr);
        vkDestroyDevice(mLogicalDevice, nullptr);
        vkDestroySurfaceKHR(mInstance, mSurface, nullptr);
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
