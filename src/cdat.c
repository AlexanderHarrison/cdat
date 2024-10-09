#include "cdat.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <assert.h>

#define READ_U16(ptr) bswap_16(*(const uint16_t*)(ptr))
#define READ_I16(ptr) ((int16_t)bswap_16(*(const uint16_t*)(ptr)))
#define READ_U32(ptr) bswap_32(*(const uint32_t*)(ptr))
#define READ_I32(ptr) ((int32_t)bswap_32(*(const uint32_t*)(ptr)))

#define WRITE_U16(ptr, data) (*((uint16_t*)(ptr)) = bswap_16(data))
#define WRITE_I16(ptr, data) (*((int16_t*)(ptr)) = (int16_t)bswap_16(data))
#define WRITE_U32(ptr, data) (*((uint32_t*)(ptr)) = bswap_32(data))
#define WRITE_I32(ptr, data) (*((int32_t*)(ptr)) = (int32_t)bswap_32(data))

static inline int cmp32(uint32_t a, uint32_t b) { return (a > b) - (a < b); }

static int reloc_cmp(const void *a, const void *b) {
    return cmp32(*(const DatRef*)a, *(const DatRef*)b);
}
static int root_cmp(const void *a, const void *b) {
    return cmp32(((const DatRootInfo*)a)->data_offset, ((const DatRootInfo*)b)->data_offset);
}
static int extern_cmp(const void *a, const void *b) {
    return cmp32(((const DatExternInfo*)a)->data_offset, ((const DatExternInfo*)b)->data_offset);
}

static inline uint32_t align_forward(uint32_t ptr, uint32_t align) {
	uint32_t mod = ptr & (align-1);
	if (mod) ptr += align - mod;
	return ptr;
}

static DAT_RET realloc_arr(void **arr, uint32_t *prev_cap, uint32_t ele_size) {
    uint32_t new_cap = *prev_cap * ele_size * 2;
    if (*prev_cap < 2048) new_cap = 4096;
    void *new_arr = realloc(*arr, new_cap);

    if (new_arr == NULL)
        return DAT_ERR_ALLOCATION_FAILURE;

    *prev_cap = new_cap / ele_size;
    *arr = new_arr;

    return DAT_SUCCESS;
}

DAT_RET dat_file_import(const uint8_t *file, uint32_t size, DatFile *out) {
    if (file == NULL) return DAT_ERR_NULL_PARAM;
    if (out == NULL) return DAT_ERR_NULL_PARAM;
    dat_file_new(out);

    // header ----------

    uint32_t file_size    = READ_U32(file + 0);
    uint32_t data_size    = READ_U32(file + 4);
    uint32_t reloc_count  = READ_U32(file + 8);
    uint32_t root_count   = READ_U32(file + 12);
    uint32_t extern_count = READ_U32(file + 16);

    if (file_size != size) {
        printf("File size mismatch! found %u, expected %u!\n", file_size, size);
        return DAT_ERR_INVALID_SIZE;
    }

    // data  ---------------------

    out->data_size = data_size;
    // realloc would be expensive
    if (data_size < 0x40000000)
        out->data_capacity = 0x10000000; // 256 KB
    else
        out->data_capacity = data_size;
    out->data = malloc(out->data_capacity);
    if (out->data == NULL) { dat_file_destroy(out); return DAT_ERR_ALLOCATION_FAILURE; }
    memcpy(out->data, file + 0x20, data_size);

    // relocation table ----------

    uint32_t reloc_offset = 0x20 + data_size;
    uint32_t reloc_size = reloc_count * sizeof(DatRef);
    out->reloc_count = reloc_count;
    out->reloc_capacity = reloc_size * 2;
    out->reloc_targets = malloc(out->reloc_capacity * sizeof(DatRef));
    if (out->reloc_targets == NULL) { dat_file_destroy(out); return DAT_ERR_ALLOCATION_FAILURE; }
    memcpy(out->reloc_targets, file + reloc_offset, reloc_size);
    for (uint32_t i = 0; i < reloc_count; ++i)
        out->reloc_targets[i] = bswap_32(out->reloc_targets[i]);
    qsort(out->reloc_targets, reloc_count, sizeof(DatRef), reloc_cmp);

    // root table ----------

    uint32_t root_offset = reloc_offset + reloc_size;
    uint32_t root_size = root_count * sizeof(DatRootInfo);
    out->root_count = root_count;
    out->root_capacity = root_count * 4;
    out->root_info = malloc(out->root_capacity * sizeof(DatRootInfo));
    if (out->root_info == NULL) { dat_file_destroy(out); return DAT_ERR_ALLOCATION_FAILURE; }
    memcpy(out->root_info, file + root_offset, root_size);
    for (uint32_t i = 0; i < root_count; ++i) {
        out->root_info[i].data_offset = bswap_32(out->root_info[i].data_offset);
        out->root_info[i].symbol_offset = bswap_32(out->root_info[i].symbol_offset);
    }
    qsort(out->root_info, root_count, sizeof(DatRootInfo), root_cmp);

    // external ref table ----------

    uint32_t extern_offset = root_offset + root_size;
    uint32_t extern_size = extern_count * sizeof(DatExternInfo);
    out->extern_count = extern_count;
    out->extern_capacity = extern_count; // unlikely to increase
    out->extern_info = malloc(out->extern_capacity * sizeof(DatExternInfo));
    if (out->extern_info == NULL) { dat_file_destroy(out); return DAT_ERR_ALLOCATION_FAILURE; }
    memcpy(out->extern_info, file + extern_offset, extern_size);
    for (uint32_t i = 0; i < extern_count; ++i) {
        out->extern_info[i].data_offset = bswap_32(out->extern_info[i].data_offset);
        out->extern_info[i].symbol_offset = bswap_32(out->extern_info[i].symbol_offset);
    }
    qsort(out->extern_info, extern_count, sizeof(DatExternInfo), extern_cmp);

    // symbol table -----------------

    uint32_t symbol_offset = extern_offset + extern_size;
    uint32_t symbol_size = file_size - symbol_offset;
    out->symbol_size = symbol_size;
    out->symbol_capacity = symbol_size * 2;
    out->symbols = malloc(out->symbol_capacity);
    if (out->symbols == NULL) { dat_file_destroy(out); return DAT_ERR_ALLOCATION_FAILURE; }
    memcpy(out->symbols, file + symbol_offset, symbol_size);

    return DAT_SUCCESS;
}

uint32_t dat_file_export_max_size(const DatFile *dat) {
    assert(dat != NULL);
    uint32_t size = 0x20;
    size += dat->data_size;
    size += (uint32_t)(dat->reloc_count * sizeof(DatRef));
    size += (uint32_t)(dat->root_count * sizeof(DatRootInfo));
    size += (uint32_t)(dat->extern_count * sizeof(DatExternInfo));
    size += dat->symbol_size;
    return size;
}

DAT_RET dat_file_export(const DatFile *dat, uint8_t *out) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (out == NULL) return DAT_ERR_NULL_PARAM;

    WRITE_U32(out+4,  dat->data_size);
    WRITE_U32(out+8,  dat->reloc_count);
    WRITE_U32(out+12, dat->root_count);
    WRITE_U32(out+16, dat->extern_count);
    memset(out+20, 0, 12); // hsdraw zeroes version and padding

    uint8_t *cursor = out + 0x20;

    uint32_t data_size = dat->data_size;
    if (dat->data != NULL) memcpy(cursor, dat->data, data_size);
    cursor += data_size;

    uint32_t reloc_size = dat->reloc_count * sizeof(DatRef);
    if (dat->reloc_targets != NULL) memcpy(cursor, dat->reloc_targets, reloc_size);
    cursor += reloc_size;

    uint32_t root_size = dat->root_count * sizeof(DatRootInfo);
    if (dat->root_info != NULL) memcpy(cursor, dat->root_info, root_size);
    cursor += root_size;

    uint32_t extern_size = dat->extern_count * sizeof(DatExternInfo);
    if (dat->extern_info != NULL) memcpy(cursor, dat->extern_info, extern_size);
    cursor += extern_size;

    uint32_t symbol_size = dat->symbol_size;
    if (dat->symbols != NULL) memcpy(cursor, dat->symbols, symbol_size);
    cursor += symbol_size;

    uint32_t file_size = (uint32_t)(cursor - out);
    WRITE_U32(out, file_size);

    return DAT_SUCCESS;
}

DAT_RET dat_file_new(DatFile *dat) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;

    *dat = (DatFile) {
        .data            = NULL,
        .reloc_targets   = NULL,
        .root_info       = NULL,
        .extern_info     = NULL,
        .symbols         = NULL,

        .data_size       = 0,
        .reloc_count     = 0,
        .root_count      = 0,
        .extern_count    = 0,
        .symbol_size     = 0,

        .data_capacity   = 0,
        .reloc_capacity  = 0,
        .root_capacity   = 0,
        .extern_capacity = 0,
        .symbol_capacity = 0
    };
    return DAT_SUCCESS;
}

DAT_RET dat_file_destroy(DatFile *dat) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;

    if (dat->data)          free(dat->data);
    if (dat->reloc_targets) free(dat->reloc_targets);
    if (dat->root_info)     free(dat->root_info);
    if (dat->extern_info)   free(dat->extern_info);
    if (dat->symbols)       free(dat->symbols);
    dat_file_new(dat);

    return DAT_SUCCESS;
}

DAT_RET dat_file_debug_print(DatFile *dat) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;

    printf("DEBUG DAT @ %p:\n", (void*)dat);

    printf("  data          %p\n", (void*)dat->data         );
    printf("  reloc_targets %p\n", (void*)dat->reloc_targets);
    printf("  root_info     %p\n", (void*)dat->root_info    );
    printf("  extern_info   %p\n", (void*)dat->extern_info  );
    printf("  symbols       %p\n", (void*)dat->symbols      );

    printf("  data_size       %u\n", dat->data_size      );
    printf("  reloc_count     %u\n", dat->reloc_count    );
    printf("  root_count      %u\n", dat->root_count     );
    printf("  extern_count    %u\n", dat->extern_count   );
    printf("  symbol_size     %u\n", dat->symbol_size    );
    printf("  data_capacity   %u\n", dat->data_capacity  );
    printf("  reloc_capacity  %u\n", dat->reloc_capacity );
    printf("  root_capacity   %u\n", dat->root_capacity  );
    printf("  extern_capacity %u\n", dat->extern_capacity);
    printf("  symbol_capacity %u\n", dat->symbol_capacity);

    printf("  ROOTS:\n");
    for (uint32_t i = 0; i < dat->root_count; ++i) {
        DatRootInfo info = dat->root_info[i];
        printf("    %06x %s\n", info.data_offset, &dat->symbols[info.symbol_offset]);
    }

    return DAT_SUCCESS;
}

uint32_t dat_file_reloc_idx(const DatFile *dat, DatRef ref) {
    uint32_t size = dat->reloc_count;
    DatRef *reloc_targets = dat->reloc_targets;

    uint32_t left = 0;
    uint32_t right = size;
    while (left < right) {
        uint32_t mid = left + size / 2;
        DatRef m = reloc_targets[mid];

        if (m < ref) left = mid + 1;
        else if (m > ref) right = mid;
        else return mid;

        size = right - left;
    }

    return left;
}

DAT_RET dat_obj_alloc(DatFile *dat, uint32_t size, DatRef *out) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (out == NULL) return DAT_ERR_NULL_PARAM;

    uint32_t obj_offset = align_forward(dat->data_size, 4);
    uint32_t new_data_size = obj_offset + size;

    uint32_t prev_cap = dat->data_capacity;
    while (new_data_size > prev_cap) {
        DAT_RET err = realloc_arr((void **)&dat->data, &dat->data_capacity, 1);
        if (err) return err;
    }

    dat->data_size = new_data_size;
    *out = obj_offset;

    return DAT_SUCCESS;
}

DAT_RET dat_obj_set_ref(DatFile *dat, DatRef from, DatRef to) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (from & 3) return DAT_ERR_INVALID_ALIGNMENT;
    if (from+4 > dat->data_size) return DAT_ERR_OUT_OF_BOUNDS;
    if (to >= dat->data_size) return DAT_ERR_OUT_OF_BOUNDS;

    uint32_t reloc_idx = dat_file_reloc_idx(dat, from);

    if (reloc_idx == dat->reloc_count || dat->reloc_targets[reloc_idx] != from) {
        uint32_t count = dat->reloc_count;
        if (count >= dat->reloc_capacity) {
            DAT_RET err = realloc_arr((void **)&dat->reloc_targets, &dat->reloc_capacity, sizeof(DatRef));
            if (err) return err;
        }

        memmove(&dat->reloc_targets[reloc_idx+1], &dat->reloc_targets[reloc_idx], count-reloc_idx);

        dat->reloc_targets[reloc_idx] = from;
        dat->reloc_count++;
    }
    
    WRITE_U32(&dat->data[from], to);

    return DAT_SUCCESS;
}

DAT_RET dat_obj_remove_ref(DatFile *dat, DatRef from) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (from & 3) return DAT_ERR_INVALID_ALIGNMENT;

    uint32_t reloc_idx = dat_file_reloc_idx(dat, from);
    memmove(&dat->reloc_targets[reloc_idx], &dat->reloc_targets[reloc_idx+1], dat->reloc_count-reloc_idx-1);
    dat->reloc_count--;

    return DAT_SUCCESS;
}

uint32_t dat_obj_read_u32(DatFile *dat, DatRef ptr) {
    assert(dat != NULL);
    assert((ptr & 3) == 0);
    assert(ptr+4 <= dat->data_size);

    return READ_U32(&dat->data[ptr]);
}

void dat_obj_write_u32(DatFile *dat, DatRef ptr, uint32_t num) {
    assert(dat != NULL);
    assert((ptr & 3) == 0);
    assert(ptr+4 <= dat->data_size);

    WRITE_U32(&dat->data[ptr], num);
}

DAT_RET dat_root_add(DatFile *dat, uint32_t index, DatRef root_obj, const char *symbol) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (symbol == NULL) return DAT_ERR_NULL_PARAM;
    if (root_obj & 3) return DAT_ERR_INVALID_ALIGNMENT;
    uint32_t root_count = dat->root_count;
    if (index > root_count) return DAT_ERR_OUT_OF_BOUNDS;

    uint32_t symbol_start = dat->symbol_size;
    uint32_t symbol_end = symbol_start + (uint32_t)strlen(symbol) + 1;
    while (symbol_end >= dat->symbol_capacity) {
        DAT_RET err = realloc_arr((void **)&dat->symbols, &dat->symbol_capacity, 1);
        if (err) return err;
    }
    strcpy(&dat->symbols[symbol_start], symbol);
    dat->symbol_size = symbol_end;

    if (root_count == dat->root_capacity) {
        DAT_RET err = realloc_arr((void **)&dat->root_info, &dat->root_capacity, sizeof(DatRootInfo));
        if (err) return err;
    }

    memmove(&dat->root_info[index+1], &dat->root_info[index], root_count-index);

    dat->root_info[index] = (DatRootInfo) {
        .data_offset = root_obj,
        .symbol_offset = symbol_start,
    };
    dat->root_count++;

    return DAT_SUCCESS;
}

DAT_RET dat_root_remove(DatFile *dat, uint32_t index) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    uint32_t root_count = dat->root_count;
    if (index >= root_count) return DAT_ERR_OUT_OF_BOUNDS;

    memmove(&dat->reloc_targets[index], &dat->reloc_targets[index+1], dat->root_count-index-1);
    dat->root_count--;

    return DAT_SUCCESS;
}
