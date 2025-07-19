#ifndef UTILS_H_
#define UTILS_H_

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

#ifdef _WIN32
#define WIN32
#endif

#ifdef WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    
    // TODO: check path permissions on windows
    #define R_OK 0
    #define W_OK 0
    #define X_OK 0
    #define F_OK 0
#else
    #include <unistd.h>
#endif

#define countof(A) (sizeof(A)/sizeof(*(A)))
#define align_up(p, align) (((uint64_t)(p) + ((uint64_t)align) - 1) & ~(((uint64_t)align) - 1))

#define RED_CODE "\033[31m"
#define GREEN_CODE "\033[32m"
#define YELLOW_CODE "\033[33m"
#define RESET_CODE "\033[0m"

#define ERROR_STR RED_CODE "ERROR: " RESET_CODE
#define WARNING_STR YELLOW_CODE "WARNING: " RESET_CODE

#define expect(A) do {\
    if (!(A)) {\
        fprintf(stderr, "expect failed - " __FILE__ ":%u: '" #A "'\n", __LINE__);\
        exit(1);\
    }\
} while (0)

#define dat_expect(A) expect(A == DAT_SUCCESS)

// UINT MAP ------------------------------------------------------

typedef struct Map {
    uint32_t *hashes;
    uint32_t *values;
    uint32_t log_size;
} Map;

Map map_alloc(uint32_t log_size) {
    uint64_t size = 1ul << log_size;
    uint32_t *alloc = calloc(2, size * sizeof(uint32_t));
    uint32_t *hashes = alloc;
    uint32_t *values = alloc + size;
    return (Map) { hashes, values, log_size };
}

Map map_alloc_n(uint32_t ele_count) {
    // alloc space for 2x elements
    uint32_t log_size;
    if (ele_count == 0)
        log_size = 1;
    else
        log_size = (uint32_t)32 - (uint32_t)__builtin_clzl(ele_count);
    return map_alloc(log_size + 1);
}

void map_free(Map *map) {
    free(map->hashes);
}

void map_clear(Map *map) {
    uint64_t size = 1ul << map->log_size;
    memset(map->hashes, 0, size * sizeof(uint32_t));
}

// returns idx
uint32_t *map_find(Map *map, uint32_t hash) {
    uint32_t size = 1ul << map->log_size;
    uint32_t mask = size - 1;
    uint32_t orig_idx = hash & mask;
    uint32_t idx = orig_idx;
    
    while (1) {
        uint32_t idx_hash = map->hashes[idx];
        if (idx_hash == hash)
            return &map->values[idx];
        if (idx_hash == 0)
            return NULL;
        idx = (idx + 1) & mask;
        if (idx == orig_idx)
            return NULL;
    }
}

// returns true on full
bool map_insert(Map *map, uint32_t hash, uint32_t value) {
    uint32_t size = 1ul << map->log_size;
    uint32_t mask = size - 1;
    uint32_t orig_idx = hash & mask;
    uint32_t idx = orig_idx;
    
    while (1) {
        uint32_t idx_hash = map->hashes[idx];
        if (idx_hash < 2) {
            map->hashes[idx] = hash;
            map->values[idx] = value;
            return false;
        } 
        idx = (idx + 1) & mask;
        if (idx == orig_idx)
            return true;
    }
}

// returns true if doesn't exist
bool map_remove(Map *map, uint32_t hash) {
    uint32_t size = 1ul << map->log_size;
    uint32_t mask = size - 1;
    uint32_t orig_idx = hash & mask;
    uint32_t idx = orig_idx;
    
    while (1) {
        uint32_t idx_hash = map->hashes[idx];
        if (idx_hash == hash) {
            map->hashes[idx] = 1;
            return false;
        }
        if (idx_hash == 0)
            return true;
        idx = (idx + 1) & mask;
        if (idx == orig_idx)
            return true;
    }
}

// Thanks to Andriy Makukha - https://stackoverflow.com/a/69812981
uint32_t map_hash_str(const char* str) {
    uint32_t h = 1234;
    for (; *str; ++str) {
        h = (uint32_t)h ^ (uint32_t)*str;
        h *= (uint32_t)0x5bd1e995;
        h ^= h >> 15;
    }
    if (h < 2)
        h += 2;
    return h;
}

uint32_t map_hash_str_len(const char* str, uint32_t len) {
    uint32_t h = 1234;
    for (; len; --len) {
        h = (uint32_t)h ^ (uint32_t)*str;
        h *= (uint32_t)0x5bd1e995;
        h ^= h >> 15;
        ++str;
    }
    if (h < 2)
        h += 2;
    return h;
}

// PATH AND IO STUFF ---------------------------------------------------

// windows doesn't like strerror :(
// I'm fine with the leak here, computation has probably ended if we are printing an error.
char *strerror_portable(int err) {
    #ifdef WIN32
        __declspec(thread) static char *buf = NULL;
        if (buf == NULL)
            buf = malloc(256);

        strerror_s(buf, 256, err);
        return buf;
    #else
        return strerror(err);
    #endif
}

bool path_exists(const char *path) {
    #ifdef WIN32
        DWORD fileAttr = GetFileAttributesA(path);
        return fileAttr != INVALID_FILE_ATTRIBUTES;
    #else
        return access(path, F_OK) == 0;
    #endif
}

// Returns true and prints an error if either
// the path does not exist, or it does not have the required permissions. 
bool check_path_access(const char *path, int permissions) {
    #ifdef WIN32
        // TODO - check path permissions on windows
        (void)permissions;
        return path_exists(path);
    #else
        if (access(path, permissions) != 0) {
            fprintf(stderr,
                ERROR_STR "Could not access '%s': %s\n",
                path,
                strerror_portable(errno)
            );
            return true;
        }
        return false;
    #endif
}

// Returns true and prints an error if the 
// file could not be written or created.
bool write_file(const char *path, uint8_t *buf, uint64_t bufsize) {
    FILE *f = fopen(path, "wb");
    if (f == NULL) {
        fprintf(stderr,
            ERROR_STR "Could not open or create file '%s': %s\n",
            path,
            strerror_portable(errno)
        );
        return true;
    }
    
    if (fwrite(buf, bufsize, 1, f) != 1) {
        fprintf(stderr,
            ERROR_STR "Could not write file '%s': %s\n",
            path,
            strerror_portable(errno)
        );
        return true;
    }
    
    return false;
}

// Returns true and prints an error if the file could not be read.
// Allocates.
bool read_file(const char *path, uint8_t **out_buf, uint64_t *out_size) {
    struct stat stats;
    if (stat(path, &stats) != 0) {
        fprintf(stderr,
            ERROR_STR "Could not determine the size of '%s': %s\n",
            path,
            strerror_portable(errno)
        );
        return true;
    }
    
    *out_size = (uint64_t)stats.st_size;
    *out_buf = malloc((uint64_t)stats.st_size);
    if (*out_buf == NULL) {
        fprintf(stderr,
            ERROR_STR "File '%s' is too large to fit in memory\n",
            path
        );
        return true;
    }
    
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        fprintf(stderr,
            ERROR_STR "Could not open file '%s': %s\n",
            path,
            strerror_portable(errno)
        );
        return true;
    }
    
    if (fread(*out_buf, (size_t)stats.st_size, 1, f) != 1) {
        fprintf(stderr,
            ERROR_STR "Could not read file '%s': %s\n",
            path,
            strerror_portable(errno)
        );
        return true;
    }
    
    return false;
}

// Returns a null terminated array of pointers to lines.
// Lines will not contain the newline and will be null terminated.
// Allocates.
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
                if (arg_i >= argc) {
                    fprintf(stderr, ERROR_STR "No argument passed for '%s' flag.\n", flag->name);
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
                    const char *input = argv[arg_i];
                    if (input[0] == '-') {
                        break;
                    } else if (*flag->target_count == MAX_INPUT_FILES) {
                        fprintf(stderr, ERROR_STR "Max number of arguments exceeded. Skipping '%s'.\n", input);
                        arg_i++;
                    } else {
                        (*flag->target)[(*flag->target_count)++] = input;
                        arg_handled = true;
                        arg_i++;
                    }
                }
                
                goto next_arg;
            }
        }
        
        if (!arg_handled)
            fprintf(stderr, WARNING_STR "Unknown flag '%s'.\n", arg);
        
        next_arg: NULL;
    }
}

static inline bool is_path_separator(char c) {
    #ifdef WIN32
        return c == '/' || c == '\\';
    #else
        return c == '/';
    #endif
}

// stpcpy isn't portable in c99.
char *push_str(char *buf, const char *str) {
    while (1) {
        char c = *str;
        *buf = c;
        if (c == 0)
            return buf;
        buf++;
        str++;
    }
}

// strdup isn't portable in c99.
// Allocates.
char *dup_str(const char *src) {
    char *str = malloc(strlen(src)+1);
    strcpy(str, src);
    return str;
}

// returns the filename without extension or the path.
// Allocates.
char *inner_name(const char *path) {
    // find last path separator
    uint64_t start = 0;
    uint64_t len = strlen(path);
    for (uint64_t i = 0; i < len; ++i) {
        char c = path[i];  
        if (is_path_separator(c))
            start = i+1;
    }
    
    // Find last period.
    // Don't count period if first character of filename.
    uint64_t period = 0;
    for (uint64_t i = start+1; i < len; ++i) {
        if (path[i] == '.')
            period = i;
    }
    
    char *inner = dup_str(&path[start]);
    
    // strip extension
    if (period != 0)
        inner[period - start] = 0;
        
    return inner;
}

// strips extension, returning ptr to added null terminator
char *strip_ext(char *path) {
    char *curpath = path;
    while (*curpath) curpath++;
    char *end = curpath;
    while (curpath > path) {
        if (*curpath == '.') {
            *curpath = 0;
            return curpath;
        }
        curpath--;
    }
    return end;
}

// returns ptr to filename in path
const char *filename(const char *path) {
    const char *curpath = path;
    while (*curpath) curpath++;
    while (curpath > path) {
        if (is_path_separator(curpath[-1]))
            return curpath;
        curpath--;
    }
    return path;
}

// strips file, returning ptr to added null terminator
char *strip_filename(char *path) {
    char *curpath = path;
    while (*curpath) curpath++;
    while (curpath > path) {
        if (is_path_separator(curpath[-1])) {
            *curpath = 0;
            return curpath;
        }
        curpath--;
    }
    *path = 0;
    return path;
}

// Allocates.
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
    char *curpath = push_str(path, first);
    
    // iter args copying strings
    va_start(argp, first);
    while (1) {
        const char *arg = va_arg(argp, const char*);
        if (arg == NULL) break;
        
        if (curpath > path && !is_path_separator(curpath[-1]))
            curpath = push_str(curpath, "/");

        curpath = push_str(curpath, arg);
    }
    va_end(argp);
    
    return path;
}

#endif
