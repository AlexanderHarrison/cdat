#!/bin/bash

WARN_FLAGS="-Wall -Wextra -Wpedantic -Wuninitialized -Wcast-qual -Wdisabled-optimization -Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wundef -Wstrict-prototypes -Wpointer-to-int-cast -Wint-to-pointer-cast -Wconversion -Wduplicated-cond -Wduplicated-branches -Wformat=2 -Wshift-overflow=2 -Wint-in-bool-context -Wvector-operation-performance -Wvla -Wdisabled-optimization -Wredundant-decls -Wmissing-parameter-type -Wold-style-declaration -Wlogical-not-parentheses -Waddress -Wmemset-transposed-args -Wmemset-elt-size -Wsizeof-pointer-memaccess -Wwrite-strings -Wtrampolines -Werror=implicit-function-declaration"
BASE_FLAGS="-O1 -ggdb"
PATH_FLAGS="-I/usr/local/lib -I/usr/local/include"
SAN_FLAGS="-fsanitize=address -fsanitize=undefined"
LINK_FLAGS=""

export GCC_COLORS="warning=01;33"

/usr/bin/c99 ${WARN_FLAGS} ${PATH_FLAGS} ${BASE_FLAGS} ${SAN_FLAGS} src/tests.c ${LINK_FLAGS} -o build/tests && build/tests
