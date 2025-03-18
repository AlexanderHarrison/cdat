#ifndef CDAT_H
#define CDAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <byteswap.h>

#define READ_U16(ptr) bswap_16(*(const uint16_t*)(ptr))
#define READ_I16(ptr) ((int16_t)bswap_16(*(const uint16_t*)(ptr)))
#define READ_U32(ptr) bswap_32(*(const uint32_t*)(ptr))
#define READ_I32(ptr) ((int32_t)bswap_32(*(const uint32_t*)(ptr)))

#define WRITE_U16(ptr, data) (*((uint16_t*)(ptr)) = bswap_16(data))
#define WRITE_I16(ptr, data) (*((int16_t*)(ptr)) = (int16_t)bswap_16(data))
#define WRITE_U32(ptr, data) (*((uint32_t*)(ptr)) = bswap_32(data))
#define WRITE_I32(ptr, data) (*((int32_t*)(ptr)) = (int32_t)bswap_32(data))

// TYPES ##########################################################

typedef uint32_t DAT_RET;
enum DAT_RET_VARIANTS {
    DAT_SUCCESS = 0,
    DAT_NOT_FOUND,

    DAT_ERR_NULL_PARAM,
    DAT_ERR_ALLOCATION_FAILURE,
    DAT_ERR_INVALID_SIZE,
    DAT_ERR_INVALID_ALIGNMENT,
    DAT_ERR_OUT_OF_BOUNDS,
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

typedef struct DatSlice {
    DatRef offset;
    uint32_t size;
} DatSlice;

typedef struct DatFile {
    // everything in here is big endian
    uint8_t *data;

    // These are sorted by increasing data offset. Little endian.
    DatRef *reloc_targets;
    DatRootInfo *root_info;
    DatExternInfo *extern_info;
    char *symbols;
    DatRef *objects; // TODO change to slice

    uint32_t data_size;
    uint32_t reloc_count;
    uint32_t root_count;
    uint32_t extern_count;
    uint32_t symbol_size;
    uint32_t object_count;

    uint32_t data_capacity;
    uint32_t reloc_capacity;
    uint32_t root_capacity;
    uint32_t extern_capacity;
    uint32_t symbol_capacity;
    uint32_t object_capacity;
} DatFile;

// FUNCTIONS ##########################################################

const char *dat_return_string(DAT_RET ret);

// dat files io -----------------------------------------

// Creates an empty dat file. Does not allocate.
DAT_RET dat_file_new(DatFile *dat);

DAT_RET dat_file_destroy(DatFile *dat);

// `file` can be safely freed after this. All data is copied to internal allocations.
DAT_RET dat_file_import(const uint8_t *file, uint32_t size, DatFile *out);

// `dat` must not be NULL.
uint32_t dat_file_export_max_size(const DatFile *dat);
DAT_RET dat_file_export(const DatFile *dat, uint8_t *out, uint32_t *size);

DAT_RET dat_file_debug_print(DatFile *dat);

// dat files modification -----------------------------------------

// Returns either the matching idx or insertion idx into reloc_targets.
// Does not check for errors.
uint32_t dat_file_reloc_idx(const DatFile *dat, DatRef ref);

// Allocated object is uninitialized.
DAT_RET dat_obj_alloc(DatFile *dat, uint32_t size, DatRef *out);
DAT_RET dat_obj_set_ref(DatFile *dat, DatRef from, DatRef to);
DAT_RET dat_obj_remove_ref(DatFile *dat, DatRef from);
DAT_RET dat_obj_read_ref(DatFile *dat, DatRef ptr, DatRef *out);

DAT_RET dat_obj_read_u32(DatFile *dat, DatRef ptr, uint32_t *out);
DAT_RET dat_obj_read_u16(DatFile *dat, DatRef ptr, uint16_t *out);
DAT_RET dat_obj_read_u8(DatFile *dat, DatRef ptr, uint8_t *out);

DAT_RET dat_obj_write_u32(DatFile *dat, DatRef ptr, uint32_t num);
DAT_RET dat_obj_write_u16(DatFile *dat, DatRef ptr, uint16_t num);
DAT_RET dat_obj_write_u8(DatFile *dat, DatRef ptr, uint8_t num);

// Places the ptr and size of the object containing the passed ptr in out.
// Returns DAT_NOT_FOUND if a surrounding object is not found.
DAT_RET dat_obj_location(const DatFile *dat, DatRef ptr, DatSlice *out);

// Inserts the object as a root at the specified index. 
// Appends if `index` == `root_count`.
DAT_RET dat_root_add(DatFile *dat, uint32_t index, DatRef root_obj, const char *symbol);

// Removes the root at the specified index.
DAT_RET dat_root_remove(DatFile *dat, uint32_t root_index);

// Recursively copies the source object to the reference destination dat file.
// Puts a reference to the copied object in dst_out, ignored if NULL.
//
// The source object must not be recursive (for now)
DAT_RET dat_obj_copy(DatFile *dst, DatFile *src, DatRef src_ref, DatRef *dst_out);

#endif
