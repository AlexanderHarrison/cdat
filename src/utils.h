#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <errno.h>
#include <alloca.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>

#define ERROR "\033[31mERROR:\033[0m "

#if defined(WIN32) || defined(_WIN32) 
#define PATH_SEPARATOR "\\"
#else
#define PATH_SEPARATOR "/"
#endif

#define expect(A) do {\
    if ((A) == false) {\
        fprintf(stderr, "expect failed - " __FILE__ ":%u: '" #A "'\n", __LINE__);\
        exit(1);\
    }\
} while (0)

#define dat_expect(A) expect(A == DAT_SUCCESS)

// Returns true and prints an error if either
// the path does not exist, or it does not have the required permissions. 
bool check_path_access(const char *path, int permissions) {
    if (access(path, permissions) != 0) {
        fprintf(stderr,
            ERROR "Could not access '%s': %s\n",
            path,
            strerror(errno)
        );
        return true;
    }
    return false;
}

// Returns true and prints an error if the 
// file could not be written or created.
bool write_file(const char *path, uint8_t *buf, uint64_t bufsize) {
    FILE *f = fopen(path, "w+");
    if (f == NULL) {
        fprintf(stderr,
            ERROR "Could not open or create file '%s': %s\n",
            path,
            strerror(errno)
        );
        return true;
    }
    
    if (fwrite(buf, bufsize, 1, f) != 1) {
        fprintf(stderr,
            ERROR "Could not write file '%s': %s\n",
            path,
            strerror(errno)
        );
        return true;
    }
    
    return false;
}

// Returns true and prints an error if the file could not be read.
// 
// The returned buffer is allocated with malloc.
bool read_file(const char *path, uint8_t **out_buf, uint64_t *out_size) {
    struct stat stats;
    if (stat(path, &stats) != 0) {
        fprintf(stderr,
            ERROR "Could not determine the size of '%s': %s\n",
            path,
            strerror(errno)
        );
        return true;
    }
    
    *out_size = (uint64_t)stats.st_size;
    *out_buf = malloc((uint64_t)stats.st_size);
    if (*out_buf == NULL) {
        fprintf(stderr,
            ERROR "File '%s' is too large to fit in memory\n",
            path
        );
        return true;
    }
    
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr,
            ERROR "Could not open file '%s': %s\n",
            path,
            strerror(errno)
        );
        return true;
    }
    
    if (fread(*out_buf, (size_t)stats.st_size, 1, f) != 1) {
        fprintf(stderr,
            ERROR "Could not read file '%s': %s\n",
            path,
            strerror(errno)
        );
        return true;
    }
    
    return false;
}

// Returns a null terminated array of pointers to lines.
// Lines will not contain the newline and will be null terminated.
uint8_t **read_lines(uint8_t *file, uint64_t file_size) {
    static uint8_t *static_nullptr = NULL;
    if (file_size == 0) return &static_nullptr;
    
    uint64_t line_count = 0;
    for (uint64_t i = 0; i < file_size; ++i) {
        uint8_t c = file[i];
        if (c == '\n')
            line_count++;
    }
    // bump count if not newline terminated
    if (file_size != 0 && file[file_size-1] != '\n')
        line_count++;
    
    // alloc and copy over symbols
    uint8_t **line_table = malloc((line_count+1) * sizeof(uint8_t*));
    uint64_t line_table_i = 0;
    uint64_t line_start = 0;
    while (1) {
        // parse line
        
        uint64_t line_i = 0;
        while (1) {
            if (line_start + line_i >= file_size) break;
            uint8_t c = file[line_start + line_i];
            if (c == '\n') break;
            line_i++;
        }
        
        uint8_t *line = malloc(line_i+1); 
        memcpy(line, &file[line_start], line_i);
        line[line_i] = 0;
        line_table[line_table_i++] = line;
        
        line_start += line_i + 1;
        if (line_start >= file_size) break;
    }
    
    line_table[line_count] = NULL;
    return line_table;
}

#define MAX_INPUT_FILES 512
typedef struct NoArgFlag {
    const char *name;
    uint64_t bit; 
} NoArgFlag;

typedef struct SingleArgFlag {
    const char *name; 
    const char **target;
} SingleArgFlag;

typedef struct MultiArgFlag {
    const char *name; 
    const char ***target;
    uint32_t *target_count;  
} MultiArgFlag;

void read_args(
    int argc, const char *argv[],
    uint64_t *flags,
    NoArgFlag *no_arg_flags, uint64_t no_arg_flag_count,
    SingleArgFlag *single_arg_flags, uint64_t single_arg_flag_count,
    MultiArgFlag *multi_arg_flags, uint64_t multi_arg_flag_count
) {
    for (int arg_i = 1; arg_i < argc;) {
        const char *arg = argv[arg_i++];
        bool arg_handled = false;
        
        for (uint64_t flag_i = 0; flag_i < no_arg_flag_count; ++flag_i) {
            NoArgFlag *flag = &no_arg_flags[flag_i];
            if (strcmp(arg, flag->name) == 0) {
                *flags |= flag->bit;
                arg_handled = true;
                goto next_arg;
            }
        }
        
        for (uint64_t flag_i = 0; flag_i < single_arg_flag_count; ++flag_i) {
            SingleArgFlag *flag = &single_arg_flags[flag_i];
            if (strcmp(arg, flag->name) == 0) {
                if (arg_i >= argc || argv[arg_i][0] == '-') {
                    fprintf(stderr, ERROR "No argument passed for '%s' flag.\n", flag->name);
                } else {
                    *flag->target = argv[arg_i++]; 
                    arg_handled = true;
                    goto next_arg;
                }
            }
        }
        
        for (uint64_t flag_i = 0; flag_i < multi_arg_flag_count; ++flag_i) {
            MultiArgFlag *flag = &multi_arg_flags[flag_i];
            if (strcmp(arg, flag->name) == 0) {
                *flag->target = malloc(MAX_INPUT_FILES * sizeof(const char *));
                
                // take while next argument doesn't start with '-'
                while (arg_i < argc) {
                    const char *input = argv[arg_i++];
                    if (input[0] == '-') {
                        break;
                    } else if (*flag->target_count == MAX_INPUT_FILES) {
                        fprintf(stderr, ERROR "Max number of arguments exceeded. Skipping '%s'.\n", input);
                    } else {
                        (*flag->target)[(*flag->target_count)++] = input;
                        arg_handled = true;
                        goto next_arg;
                    }
                }
            }
        }
        
        if (!arg_handled)
            fprintf(stderr, ERROR "Unknown flag '%s'.\n", arg);
        
        next_arg: NULL;
    }
}

// stpcpy is portable in c99, so I wrote my own.
char *my_stpcpy(char *buf, const char *str) {
    while (1) {
        char c = *str;
        *buf = c;
        if (c == 0)
            return buf;
        buf++;
        str++;
    }
}

char *path_join(const char *first, ...) {
    // iter args to count length
    uint64_t len = strlen(first);
    va_list argp;
    va_start(argp, first);
    while (1) {
        const char *arg = va_arg(argp, const char*);
        if (arg == NULL) break;
        len += 1;           // space for path separator
        len += strlen(arg); // space for path
    }
    va_end(argp);
    
    char *path = malloc(len + 1); // space for terminator
    char *curpath = my_stpcpy(path, first);
    
    // iter args copying strings
    va_start(argp, first);
    while (1) {
        const char *arg = va_arg(argp, const char*);
        if (arg == NULL) break;
        if (curpath > path && curpath[-1] != PATH_SEPARATOR[0])
            curpath = my_stpcpy(curpath, PATH_SEPARATOR);
        curpath = my_stpcpy(curpath, arg);
    }
    va_end(argp);
    
    return path;
}

#define countof(A) (sizeof(A)/sizeof(*A))

#endif
