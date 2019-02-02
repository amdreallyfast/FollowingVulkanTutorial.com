// Note: Recommended that precompiled header properties be set for each file and not across the 
// entire project unless you are certain that *all* the source files in the project will be using 
// at least some of the headers in the precompiled header. 

// Also Note: If you do this, then the compiler will complain that pch.h is not included in the 
// source file (or something of the sort) for *every single source file that doesn't include it*.

// To use precompiled headers (in Visual Studio 2017, at least):
// - Create pch.h to be the header that will include what you want to precompile.
// - Create pch.cpp. One line: #include "pch.h" (note the use of "" instead of <> because this is 
//  a project-specific file and not something out of the "additional include directories"). 
// - Alternately, create DerBerger.h and FancyPants.cpp, whatever makes sense.
// - Move common include paths from whatever source files will be using it (ex: main.cpp includes 
//  vulkan.h, which is a big honking header) to pch.h and replace the include paths in the source 
//  files with #include "pch.h".
// - Right-click .cpp file -> Properties -> Precompiled Header
//      - Use "All Configurations" and "All Platforms".
//      - Set "Precompiled Header" to "Create (/Yc)".
//      - Set "Precompiled header file" to "pch.h".
// - Right-click source files that include pch.h -> Properties -> Precompiled Headers.
//      - Use "All Configurations" and "All Platforms".
//      - Set "Precompiled Header" to "Use (/Yu)".
//      - Set "Precompiled header file" to "pch.h".
#ifndef PCH_H
#define PCH_H

//#include <vulkan/vulkan.h>
// this will include vulkan.h too
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#endif // !PCH_H
