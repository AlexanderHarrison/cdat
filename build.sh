#!/bin/bash

set -e

WARN_FLAGS="-Wall -Wextra -Wpedantic -Wuninitialized -Wcast-qual -Wdisabled-optimization -Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wundef -Wstrict-prototypes -Wpointer-to-int-cast -Wint-to-pointer-cast -Wconversion -Wduplicated-cond -Wduplicated-branches -Wformat=2 -Wshift-overflow=2 -Wint-in-bool-context -Wvector-operation-performance -Wvla -Wdisabled-optimization -Wredundant-decls -Wmissing-parameter-type -Wold-style-declaration -Wlogical-not-parentheses -Waddress -Wmemset-transposed-args -Wmemset-elt-size -Wsizeof-pointer-memaccess -Wwrite-strings -Wtrampolines -Werror=implicit-function-declaration"
if [ "$1" = 'release' ]; then
    BASE_FLAGS="-O2"
else
    BASE_FLAGS="-ggdb"
fi
PATH_FLAGS="-I/usr/include -I/usr/lib -I/usr/local/lib -I/usr/local/include"
LINK_FLAGS=""

if [[ -z $1 || $1 = 'release' || $1 = 'dat_mod' ]]; then
    /usr/bin/c99 ${WARN_FLAGS} ${PATH_FLAGS} ${BASE_FLAGS} src/mod.c ${LINK_FLAGS} -o build/dat_mod
fi
if [[ -z $1 || $1 = 'release' || $1 = 'hmex' ]]; then
    /usr/bin/c99 ${WARN_FLAGS} ${PATH_FLAGS} ${BASE_FLAGS} src/hmex.c ${LINK_FLAGS} -o build/hmex
fi

if [ "$1" = 'release' ]; then
    strip build/dat_mod build/hmex build/ml
fi
