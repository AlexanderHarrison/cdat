#include "dat.h"
#include "dat.c"

#include <stdio.h>

#define USAGE "\
USAGE:\n\
    dat_mod tree <dat file>\n\
    dat_mod extract <dat file> <root name> [out root name]\n\
"

typedef struct File { char *ptr; size_t size; } File;
File read_file(const char* filepath);
bool write_file(const char* filepath, uint8_t *buf, size_t size);

char *strdup(const char *src) {
    char *str = malloc(strlen(src)+1);
    strcpy(str, src);
    return str;
}

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        printf(USAGE);
        return 0;
    }
    
    bool parse_err = false;
    bool list_roots = false;
    bool copy_root = false;
    bool save = false;
    char *dat_path_in = NULL;
    char *dat_path_out = NULL;
    char *root_name_in = NULL;
    char *root_name_out = NULL;
    
    // Parse arguments ------------------------------

    const char *arg1 = argv[1];
    if (strcmp(arg1, "tree") == 0) {
        list_roots = true;
        if (argc < 3) {
            parse_err = true;
            fprintf(stderr, USAGE);
        } else {
            dat_path_in = strdup(argv[2]);
        }
    } else if (strcmp(arg1, "extract") == 0) {
        copy_root = true;
        save = true;
        if (argc < 4) {
            parse_err = true;
            fprintf(stderr, USAGE);
        } else {
            dat_path_in = strdup(argv[2]); 
            root_name_in = strdup(argv[3]); 
        }
        
        root_name_out = strdup(argc < 5 ? root_name_in : argv[4]);
        dat_path_out = malloc(strlen(root_name_in) + 5);
        char *end = strcpy(dat_path_out, root_name_out);
        strcpy(end, ".dat"); 
    }
    
    if (parse_err)
        return 1;

    // Run IO --------------------------------------

    bool io_err = false;
    DatFile *dat_in = NULL;
    DatFile *dat_out = NULL;
    DatRootInfo *root_in = NULL;
        
    if (dat_path_in) {
        dat_in = malloc(sizeof(DatFile));
        File file = read_file(dat_path_in);
        if (file.ptr == NULL) {
            io_err = true;
            fprintf(stderr, "Error: dat file '%s' not found.\n", dat_path_in);
        } else { 
            dat_file_import((uint8_t*)file.ptr, (uint32_t)file.size, dat_in);
        }
    }
    
    if (dat_path_out) {
        dat_out = malloc(sizeof(DatFile));
        File file = read_file(dat_path_out);
        if (file.ptr == NULL)
            dat_file_new(dat_out);
        else 
            dat_file_import((uint8_t*)file.ptr, (uint32_t)file.size, dat_out);
    }
    
    if (root_name_in) {
        for (int64_t root_i = 0; root_i < dat_in->root_count; ++root_i) {
            DatRootInfo *root = &dat_in->root_info[root_i];
            char *root_name = dat_in->symbols + root->symbol_offset;
            if (strcmp(root_name, root_name_in) == 0) {
                root_in = root;
                break;
            }
        }
        
        if (root_in == NULL) {
            printf("Error: root '%s' not found in '%s'.\n", root_name_in, dat_path_in);
            io_err = true;
        }
    }
    
    if (io_err)
        return 1;
    
    // Run Commands --------------------------------------
    
    if (list_roots) {
        for (int64_t root_i = 0; root_i < dat_in->root_count; ++root_i) {
            DatRootInfo *root = &dat_in->root_info[root_i];
            char *root_name = dat_in->symbols + root->symbol_offset;
            printf("%s\n", root_name);
        }
    }
    
    if (copy_root) {
        DatRef copied_root;
        dat_obj_copy(dat_out, dat_in, root_in->data_offset, &copied_root);
        dat_root_add(dat_out, dat_out->root_count, copied_root, root_name_out);
    }
    
    if (save) {
        uint32_t max_size = dat_file_export_max_size(dat_out);
        uint8_t *buf = malloc(max_size);
        uint32_t export_size;
        dat_file_export(dat_out, buf, &export_size);
        write_file(dat_path_out, buf, export_size);
    }
    
    return 0;
}

File read_file(const char* filepath) {
    FILE *f = NULL;
    char *ptr = NULL;

    f = fopen(filepath, "rb");
    if (f == NULL) goto ERR;

    if (fseek(f, 0, SEEK_END) < 0) goto ERR;
    long size_or_err = ftell(f);
    if (size_or_err < 0) goto ERR;
    size_t size = (size_t)size_or_err;
    if (fseek(f, 0, SEEK_SET) < 0) goto ERR;

    ptr = malloc(size);
    if (ptr == NULL) goto ERR;

    if (size != 0 && fread(ptr, size, 1, f) != 1) goto ERR;

    if (fclose(f) != 0) goto ERR;

    return (File) { ptr, size };

ERR:
    if (f) fclose(f);
    if (ptr) free(ptr);
    return (File) { NULL, 0 };
}

bool write_file(const char* filepath, uint8_t *buf, size_t size) {
    FILE *f = NULL;

    f = fopen(filepath, "wb+");
    if (f == NULL) goto ERR;

    if (size != 0 && fwrite(buf, size, 1, f) != 1) goto ERR;
    if (fclose(f) != 0) goto ERR;
    return false;

ERR:
    if (f) fclose(f);
    return true;
}
