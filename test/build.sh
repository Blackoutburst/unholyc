#!/bin/bash
readonly NAME="unholyc"
readonly SRC="out/src/*.cc"
readonly INCLUDE="-Iout/include"

readonly I_UHC="../dist/include"
readonly L_UHC="../dist/.local/lib"
readonly UHC="-I$I_UHC -L$L_UHC -lunholyc"

readonly F_ERROR="-Wall -Wextra -Wpedantic"
readonly F_DEBUG="-g3 -fno-omit-frame-pointer -fsanitize=address -fsanitize-address-use-after-return=always"

unholyc ./ -r -o out/

printf "\e[94mCLANG\e[0m: "
clang++ -o $NAME $SRC $INCLUDE $UHC $F_ERROR $F_DEBUG
CLANG_EXIT=$?

if [ $CLANG_EXIT -ne 0 ]; then
    printf "\e[91mFAILED\e[0m\n"
else
    printf "\e[92mOK\e[0m\n"
fi
