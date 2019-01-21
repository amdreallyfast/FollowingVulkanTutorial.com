:: `rem` is a comment, but it will be echo'd to console out. To avoid console printing of 
:: comments, either preface the comment with `@`, preface the script with `@echo off`, or use `::` 
:: (https://stackoverflow.com/questions/11269338/how-to-comment-out-add-comment-in-a-batch-cmd)

:: -H reports human-readable compiler output to the console, but only if the compilation finished.
:: -V tells the compiler to create SPIR-V binaries for Vulkan.
:: -G tells the compiler to create SPIR-V binaries for OpenGL. (ignored; we're using Vulkan)
:: -o path/to/output/file.whatevs to use non-default naming.
C:\ThirdParty\VulkanSDK\1.1.85.0\Bin32\glslangValidator.exe -V triangle.vert
C:\ThirdParty\VulkanSDK\1.1.85.0\Bin32\glslangValidator.exe -V triangle.frag

:: pause so that we can read the console output
pause
