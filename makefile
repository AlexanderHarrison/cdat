.PHONY: build run san debug

OUT := cdat
FILES := src/cdat.c
TRACK := src/cdat.c src/cdat.h
BASE_FLAGS := -std=c99 -O2

WARN_FLAGS := -Wall -Wextra -Wpedantic -Wuninitialized -Wcast-qual -Wdisabled-optimization -Winit-self -Wlogical-op -Wmissing-include-dirs -Wredundant-decls -Wshadow -Wundef -Wstrict-prototypes -Wpointer-to-int-cast -Wint-to-pointer-cast -Wconversion -Wduplicated-cond -Wduplicated-branches -Wformat=2 -Wshift-overflow=2 -Wint-in-bool-context -Wlong-long -Wvector-operation-performance -Wvla -Wdisabled-optimization -Wredundant-decls -Wmissing-parameter-type -Wold-style-declaration -Wlogical-not-parentheses -Waddress -Wmemset-transposed-args -Wmemset-elt-size -Wsizeof-pointer-memaccess -Wwrite-strings -Wbad-function-cast -Wtrampolines -Werror=implicit-function-declaration

PATH_FLAGS := -I/usr/local/lib -I/usr/local/include
LINK_FLAGS :=

export GCC_COLORS = warning=01;33

build: $(TRACK)
	gcc $(WARN_FLAGS) $(PATH_FLAGS) $(BASE_FLAGS) $(FILES) $(LINK_FLAGS)

test_main: $(TRACK) src/test_main.c
	gcc $(WARN_FLAGS) $(PATH_FLAGS) -g -fsanitize=undefined -fsanitize=address $(BASE_FLAGS) $(FILES) src/test_main.c $(LINK_FLAGS) -o test_main
	./test_main

run: build
	./$(OUT)
