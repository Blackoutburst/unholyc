@echo off

:: Source
set NAME=unholyc-graphics
set SRC=out\src\*.cc out\src\window\*.cc out\src\core\*.cc out\src\debug\*.cc out\src\utils\*.cc
set INCLUDE=-Iout/include

:: UnholyC std
set I_UHC=..\dist\include
set L_UHC=..\dist\lib
set UHC=-I%I_UHC% -L%L_UHC% -luhc

:: Vulkan (update paths as needed for your Windows install)
set I_VK=C:\VulkanSDK\Include
set L_VK=C:\VulkanSDK\Lib
set VK=-I%I_VK% -L%L_VK% -lvulkan-1

:: GLFW (update paths as needed for your Windows install)
set I_GLFW=C:\glfw\include
set L_GLFW=C:\glfw\lib
set GLFW=-I%I_GLFW% -L%L_GLFW% -lglfw3

:: Clang flags
set F_ERROR=-Wall -Wextra -Wpedantic
set F_DEBUG=-g3 -fno-omit-frame-pointer -fsanitize=address
set F_DISABLED=-Wno-writable-strings

unholyc -I"%I_UHC%" .\ .\out

echo CLANG:
clang++ -o %NAME% %SRC% %INCLUDE% %UHC% %VK% %GLFW% %F_ERROR% %F_DEBUG% %F_DISABLED%

if %ERRORLEVEL% neq 0 (
    echo FAILED
) else (
    echo OK
)

slangc shader\cube.slang -entry vertMain -stage vertex -target spirv -o shader\cubeVert.spv
slangc shader\cube.slang -entry fragMain -stage fragment -target spirv -o shader\cubeFrag.spv

rmdir /s /q out
