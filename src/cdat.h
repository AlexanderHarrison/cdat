#ifndef CDAT_H
#define CDAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <byteswap.h>

// TYPES ##########################################################

typedef uint32_t DAT_RET;
enum DAT_RET_VARIANTS {
    DAT_SUCCESS = 0,

    DAT_ERR_NULL_PARAM,
    DAT_ERR_ALLOCATION_FAILURE,
    DAT_ERR_INVALID_SIZE,
    DAT_ERR_INVALID_ALIGNMENT,
    DAT_ERR_OUT_OF_BOUNDS
};

typedef uint32_t DatRef;
typedef uint32_t SymbolRef;

typedef struct DatRootInfo {
    DatRef data_offset;
    SymbolRef symbol_offset;
} DatRootInfo;

typedef struct DatExternInfo {
    DatRef data_offset;
    SymbolRef symbol_offset;
} DatExternInfo;

typedef struct DatFile {
    // everything in here is big endian
    uint8_t *data;

    // These are sorted by increasing data offset. Little endian.
    DatRef *reloc_targets;
    DatRootInfo *root_info;
    DatExternInfo *extern_info;

    char *symbols;

    uint32_t data_size;
    uint32_t reloc_count;
    uint32_t root_count;
    uint32_t extern_count;
    uint32_t symbol_size;

    uint32_t data_capacity;
    uint32_t reloc_capacity;
    uint32_t root_capacity;
    uint32_t extern_capacity;
    uint32_t symbol_capacity;
} DatFile;

// FUNCTIONS ##########################################################

// dat files io -----------------------------------------

// Creates an empty dat file. Does not allocate.
DAT_RET dat_file_new(DatFile *dat);

DAT_RET dat_file_destroy(DatFile *dat);

// `file` can be safely freed after this. All data is copied to internal allocations.
DAT_RET dat_file_import(const uint8_t *file, uint32_t size, DatFile *out);

// `dat` must not be NULL.
uint32_t dat_file_export_max_size(const DatFile *dat);

DAT_RET dat_file_export(const DatFile *dat, uint8_t *out);

DAT_RET dat_file_debug_print(DatFile *dat);

// dat files modification -----------------------------------------

// Returns either the matching idx or insertion idx. Does not check for errors.
uint32_t dat_file_reloc_idx(const DatFile *dat, DatRef ref);

// Allocated object is uninitialized.
DAT_RET dat_obj_alloc(DatFile *dat, uint32_t size, DatRef *out);
DAT_RET dat_obj_set_ref(DatFile *dat, DatRef from, DatRef to);
DAT_RET dat_obj_remove_ref(DatFile *dat, DatRef from);

// read/writes assert invariants
uint32_t dat_obj_read_u32(DatFile *dat, DatRef ptr);
void dat_obj_write_u32(DatFile *dat, DatRef ptr, uint32_t num);

// Inserts the object as a root at the specified index. 
// Appends if `index` == `root_count`.
DAT_RET dat_root_add(DatFile *dat, uint32_t index, DatRef root_obj, const char *symbol);

// Removes the root at the specified index.
DAT_RET dat_root_remove(DatFile *dat, uint32_t root_index);

#endif
