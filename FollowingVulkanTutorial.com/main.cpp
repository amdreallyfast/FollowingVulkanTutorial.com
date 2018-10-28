#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <iostream>
#include <sstream>  // for format-capable stringstream
#include <iomanip>  // for std::setfill(...) and std::setw(...)
#include <vector>
#include <optional>

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

    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(instance, "vkDestroyDebugUTilsMessengerEXT");
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
    int mWindowWidth = 800;
    int mWindowHeight = 600;


#ifdef NDEBUG
    const bool mEnableValidationLayers = false;
#else
    const bool mEnableValidationLayers = true;
#endif // NDEBUG


    VkInstance mInstance;
    VkDebugUtilsMessengerEXT mCallback;
    VkPhysicalDevice mPhysicalDevice = VK_NULL_HANDLE;

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        bool IsComplete() {
            return graphicsFamily.has_value();
        }
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
        
        std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;
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
        appInfo.apiVersion = VK_MAKE_VERSION(1, 1, 82);

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
        instanceCreateInfo.enabledLayerCount = static_cast<uint32_t>(requiredValidationLayers.size());
        instanceCreateInfo.ppEnabledLayerNames = requiredValidationLayers.data();
        instanceCreateInfo.pNext = &debugCreateInfo;

        if (vkCreateInstance(&instanceCreateInfo, nullptr, &mInstance) != VK_SUCCESS) {
            throw std::runtime_error("failed to create instance");
        }

        // and create the debug messenger itself
        if (CreateDebugUtilsMessengerEXT(mInstance, &debugCreateInfo, nullptr, &mCallback) != VK_SUCCESS) {
            throw std::runtime_error("failed to set up debug callback");
        }
    }

    ///*---------------------------------------------------------------------------------------------
    //Description:
    //    Queries the provided device and checks the properties to see if it supports graphics 
    //    command queues. (??is there a GPU that doesn't??)
    //Creator:    John Cox, 10/2018
    //---------------------------------------------------------------------------------------------*/
    //QueueFamilyIndices FindQueueFamilies(VkPhysicalDevice device) {
    //    QueueFamilyIndices indices;

    //    uint32_t queueFamilyCount = 0;
    //    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
    //    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    //    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    //    uint32_t i = 0;
    //    for (const auto &queueFamily : queueFamilies) {
    //        bool exists = queueFamily.queueCount > 0;
    //        bool supportsGraphics = (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
    //        if (exists && supportsGraphics) {
    //            indices.graphicsFamily = i;
    //            break;
    //        }
    //        i++;
    //    }
    //    return indices;
    //}

    /*---------------------------------------------------------------------------------------------
    Description:
        Checks if the necessary features to run this program are available on the provided GPU.
    Creator:    John Cox, 10/2018
    ---------------------------------------------------------------------------------------------*/
    bool IsDeviceSuitable(VkPhysicalDevice device) {
        VkPhysicalDeviceProperties deviceProperties;
        vkGetPhysicalDeviceProperties(device, &deviceProperties);
        VkPhysicalDeviceFeatures deviceFeatures;
        vkGetPhysicalDeviceFeatures(device, &deviceFeatures);
        
        // record the index (more like a handle/pointer to a hardware-specific location that 
        // won't change) for graphics command queues 
        QueueFamilyIndices indices;
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());
        uint32_t i = 0;
        for (const auto &queueFamily : queueFamilies) {
            bool exists = queueFamily.queueCount > 0;
            bool supportsGraphics = (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
            if (exists && supportsGraphics) {
                indices.graphicsFamily = i;
                break;
            }
            i++;
        }

        bool success =
            deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
            deviceFeatures.geometryShader &&
            indices.IsComplete();
        return success;
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
        PickPhysicalDevice();
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
            //DestroyDebugUtilsMessengEXT(mInstance, mCallback, nullptr);
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