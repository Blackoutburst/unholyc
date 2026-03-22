#!/bin/bash

# Source
readonly NAME="unholyc-graphics"
readonly SRC="out/src/*.cc out/src/window/*.cc out/src/core/*.cc out/src/debug/*.cc"
readonly INCLUDE="-Iout/include"

# UnholyC std
readonly I_UHC="$HOME/.local/include"
readonly L_UHC="$HOME/.local/lib"
readonly UHC="-I$I_UHC -L$L_UHC -lunholyc"

# Vulkan
readonly I_VK="/opt/homebrew/include"
readonly L_VK="/opt/homebrew/lib"
readonly VK="-isystem $I_VK -L$L_VK -lvulkan -Wl,-rpath,$L_VK"

# GLFW
readonly I_GLFW="/opt/homebrew/include"
readonly L_GLFW="/opt/homebrew/lib"
readonly GLFW="-I$I_GLFW -L$L_GLFW -lglfw3"

# Clang flags
readonly F_ERROR="-Wall -Wextra -Wpedantic"
readonly F_DEBUG="-g3 -fno-omit-frame-pointer -fsanitize=address -fsanitize-address-use-after-return=always"
readonly F_DISABLED="-Wno-writable-strings"
readonly FRAMEWORKS="-framework Cocoa -framework IOKit"

unholyc ./ -r -o out/

printf "\e[94mCLANG\e[0m: "
clang++ -o $NAME $ENGINE_SRC $SRC $INCLUDE $UHC $VK $GLFW $F_ERROR $F_DEBUG $F_DISABLED $FRAMEWORKS
CLANG_EXIT=$?

if [ $CLANG_EXIT -ne 0 ]; then
    printf "\e[91mFAILED\e[0m\n"
else
    printf "\e[92mOK\e[0m\n"
fi

rm -rf out/
