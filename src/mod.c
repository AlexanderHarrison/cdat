#include "utils.h"
#include "dat.h"
#include "dat.c"

#include <stdio.h>

#define USAGE "\
USAGE:\n\
    dat_mod debug <dat file>\n\
        Print information about a dat file.\n\
    dat_mod extract <dat file> <root name>\n\
        Extract a root from a dat file into its own file.\n\
    dat_mod insert <dat file> <input dat file>\n\
        Copy roots from one dat file into another.\n\
"

DatFile read_dat(const char *path) {
    DatFile dat;
    uint8_t *file;
    uint64_t file_size;
    if (read_file(path, &file, &file_size)) {
        exit(1);
    } else {
        dat_file_import(file, (uint32_t)file_size, &dat);
    }
    
    return dat;
}

void write_dat(DatFile *dat, const char *path) {
    uint32_t max_size = dat_file_export_max_size(dat);
    uint8_t *buf = malloc(max_size);
    uint32_t export_size;
    dat_file_export(dat, buf, &export_size);
    if (write_file(path, buf, export_size))
        exit(1);
}

DatRootInfo *find_root(DatFile *dat, const char *root_name) {
    for (int64_t root_i = 0; root_i < dat->root_count; ++root_i) {
        DatRootInfo *r = &dat->root_info[root_i];
        if (strcmp(root_name, dat->symbols + r->symbol_offset) == 0)
            return r;
    }
    
    fprintf(stderr, ERROR_STR "root '%s' not found.\n", root_name);
    exit(1);
}

void usage_exit(void) {
    fprintf(stderr, USAGE);
    exit(1);
}

int main(int argc, const char *argv[]) {
    if (argc < 2) {
        printf(USAGE);
        return 0;
    }
    
    // Parse arguments ------------------------------

    const char *arg1 = argv[1];
    if (strcmp(arg1, "debug") == 0) {
        if (argc < 3)
            usage_exit();
        
        DatFile dat = read_dat(argv[2]);
        dat_file_debug_print(&dat);
    } else if (strcmp(arg1, "extract") == 0) {
        if (argc < 4)
            usage_exit(); 
        
        // read args
        DatFile dat_in = read_dat(argv[2]);
        const char *root_name = argv[3];
        DatRootInfo *root_in = find_root(&dat_in, root_name);
        
        // add extension 
        char *dat_path_out = malloc(strlen(root_name) + 4 + 1);
        char *ext = push_str(dat_path_out, root_name);
        push_str(ext, ".dat");
        
        // copy root
        DatFile out;
        dat_file_new(&out);
        DatRef copied_root;
        dat_obj_copy(&out, &dat_in, root_in->data_offset, &copied_root);
        dat_root_add(&out, 0, copied_root, root_name);
        
        write_dat(&out, dat_path_out);
    } else if (strcmp(arg1, "insert") == 0) {
        // TODO fix (try copy map_head into map_head, roots are same offset)
        DatFile dat_dst = read_dat(argv[2]);
        DatFile dat_src = read_dat(argv[3]);
        
        uint32_t root_count = dat_src.root_count; 
        for (uint32_t i = 0; i < root_count; ++i) {
            DatRootInfo *info = &dat_src.root_info[i];
            
            DatRef copied_root;
            dat_obj_copy(&dat_dst, &dat_src, info->data_offset, &copied_root);
            char *root_name = dat_src.symbols + info->symbol_offset;
            dat_root_add(&dat_dst, dat_dst.root_count, copied_root, root_name);
        }
        
        write_dat(&dat_dst, argv[2]);
    }
    
    return 0;
}
