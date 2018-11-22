rem -H reports human-readable compiler output to the console, but only if the compilation finished.
rem -V tells the compiler to create SPIR-V binaries for Vulkan.
rem Note: -G tells the compiler to create SPIR-V binaries for OpenGL.
rem -o path/to/output/file.whatevs to use non-default naming.
C:\ThirdParty\VulkanSDK\1.1.85.0\Bin32\glslangValidator.exe -H -V triangle.vert
C:\ThirdParty\VulkanSDK\1.1.85.0\Bin32\glslangValidator.exe -H -V triangle.frag

rem pause so that we can read the console output
pause
