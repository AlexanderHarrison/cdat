#ifndef CDAT_H
#define CDAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
    #if defined(WIN32) || defined(_WIN32)
        #define dat_be16u(x) _byteswap_ushort(x)
        #define dat_be32u(x) _byteswap_ulong(x)
    #else
        #include <byteswap.h>
        #define dat_be16u(x) bswap_16(x)
        #define dat_be32u(x) bswap_32(x)
    #endif
#elif defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    #define dat_be16u(x) (x)
    #define dat_be32u(x) (x)
#else 
    #error "Please define __BYTE_ORDER__ to either __ORDER_LITTLE_ENDIAN__ or __ORDER_BIG_ENDIAN__"
#endif

#define READ_U16(ptr) dat_be16u(*(const uint16_t*)(ptr))
#define READ_I16(ptr) ((int16_t)dat_be16u(*(const uint16_t*)(ptr)))
#define READ_U32(ptr) dat_be32u(*(const uint32_t*)(ptr))
#define READ_I32(ptr) ((int32_t)dat_be32u(*(const uint32_t*)(ptr)))

#define WRITE_U16(ptr, data) (*((uint16_t*)(ptr)) = dat_be16u(data))
#define WRITE_I16(ptr, data) (*((int16_t*)(ptr)) = (int16_t)dat_be16u(data))
#define WRITE_U32(ptr, data) (*((uint32_t*)(ptr)) = dat_be32u(data))
#define WRITE_I32(ptr, data) (*((int32_t*)(ptr)) = (int32_t)dat_be32u(data))

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

// Frees allocations and zeros struct.
DAT_RET dat_file_destroy(DatFile *dat);

// `file` can be safely freed after this. All data is copied to internal allocations.
// The size parameter is the size of the buffer containing the dat file, which must be larger
// than the internal file size listed in the dat file header.
// If it is smaller, DAT_ERR_INVALID_SIZE will be returned.
//
// You may pass an uninitialized `out` ptr.
DAT_RET dat_file_import(const uint8_t *file, uint32_t buffer_size, DatFile *out);

// `dat` must not be NULL or this will crash.
uint32_t dat_file_export_max_size(const DatFile *dat);

// First call `dat_file_export_max_size`, then pass a buffer of at least that size to `dat_file_export`.
//
// UB if size is smaller than what `dat_file_export_max_size` returns!
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

// Returns DAT_NOT_FOUND if the dat file does not contain a root with this name.
DAT_RET dat_root_find(DatFile *dat, const char *root_name, DatRef *out);

// Copies the source object and all its children to the reference destination dat file.
// Puts a reference to the copied object in dst_out.
DAT_RET dat_obj_copy(DatFile *dst, DatFile *src, DatRef src_ref, DatRef *dst_out);

#endif
