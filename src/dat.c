#include "dat.h"

#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>

static uint32_t binary_search_refs(const DatRef *refs, uint32_t count, DatRef ref);

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
    if (new_cap < 4096) new_cap = 4096;
    void *new_arr = realloc(*arr, new_cap);

    if (new_arr == NULL)
        return DAT_ERR_ALLOCATION_FAILURE;

    *prev_cap = new_cap / ele_size;
    *arr = new_arr;

    return DAT_SUCCESS;
}

DAT_RET dat_file_import(const uint8_t *file, uint32_t buffer_size, DatFile *out) {
    if (file == NULL) return DAT_ERR_NULL_PARAM;
    if (out == NULL) return DAT_ERR_NULL_PARAM;
    dat_file_new(out);

    // header ----------

    uint32_t file_size    = READ_U32(file + 0);
    uint32_t data_size    = READ_U32(file + 4);
    uint32_t reloc_count  = READ_U32(file + 8);
    uint32_t root_count   = READ_U32(file + 12);
    uint32_t extern_count = READ_U32(file + 16);

    if (file_size > buffer_size)
        return DAT_ERR_INVALID_SIZE;

    // data  ---------------------

    out->data_size = data_size;
    // realloc would be expensive
    if (data_size < 0x40000)
        out->data_capacity = 0x40000; // 256 KB
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
        out->reloc_targets[i] = dat_be32u(out->reloc_targets[i]);
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
        out->root_info[i].data_offset = dat_be32u(out->root_info[i].data_offset);
        out->root_info[i].symbol_offset = dat_be32u(out->root_info[i].symbol_offset);
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
        out->extern_info[i].data_offset = dat_be32u(out->extern_info[i].data_offset);
        out->extern_info[i].symbol_offset = dat_be32u(out->extern_info[i].symbol_offset);
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
    
    // find objects -----------------

    // find all refs
    out->object_capacity = out->reloc_count + out->root_count + out->extern_count;  
    out->objects = malloc(sizeof(DatRef) * out->object_capacity);
    
    uint32_t object_i = 0;
    for (uint32_t i = 0; i < out->reloc_count; ++i)
        out->objects[object_i++] = READ_U32(&out->data[out->reloc_targets[i]]);
    for (uint32_t i = 0; i < out->root_count; ++i)
        out->objects[object_i++] = out->root_info[i].data_offset;
    for (uint32_t i = 0; i < out->extern_count; ++i)
        out->objects[object_i++] = out->extern_info[i].data_offset;
    qsort(out->objects, object_i, sizeof(DatRef), reloc_cmp);
    
    // deduplicate refs
    DatRef last_ref = 0xFFFFFFFF;
    uint32_t head_i = 0; 
    uint32_t base_i = 0;
    while (head_i < object_i) {
        DatRef obj = out->objects[head_i];
        if (obj != last_ref) {
            out->objects[base_i++] = obj;
            last_ref = obj;
        }
        head_i++; 
    }
    out->object_count = base_i; 

    return DAT_SUCCESS;
}

uint32_t dat_file_export_max_size(const DatFile *dat) {
    uint32_t size = 0x20;
    size += dat->data_size;
    size += (uint32_t)(dat->reloc_count * sizeof(DatRef));
    size += (uint32_t)(dat->root_count * sizeof(DatRootInfo));
    size += (uint32_t)(dat->extern_count * sizeof(DatExternInfo));
    size += dat->symbol_size;
    return size;
}

DAT_RET dat_file_export(const DatFile *dat, uint8_t *out, uint32_t *size) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (out == NULL) return DAT_ERR_NULL_PARAM;
    if (size == NULL) return DAT_ERR_NULL_PARAM;

    WRITE_U32(out+4,  dat->data_size);
    WRITE_U32(out+8,  dat->reloc_count);
    WRITE_U32(out+12, dat->root_count);
    WRITE_U32(out+16, dat->extern_count);
    memset(out+20, 0, 12); // hsdraw zeroes version and padding

    uint8_t *cursor = out + 0x20;

    uint32_t data_size = dat->data_size;
    if (dat->data != NULL) memcpy(cursor, dat->data, data_size);
    cursor += data_size;

    for (uint32_t i = 0; i < dat->reloc_count; ++i) {
        WRITE_U32(cursor, dat->reloc_targets[i]);
        cursor += sizeof(DatRef);
    }

    for (uint32_t i = 0; i < dat->root_count; ++i) {
        WRITE_U32(cursor, dat->root_info[i].data_offset);
        cursor += sizeof(uint32_t);
        WRITE_U32(cursor, dat->root_info[i].symbol_offset);
        cursor += sizeof(uint32_t);
    }

    for (uint32_t i = 0; i < dat->extern_count; ++i) {
        WRITE_U32(cursor, dat->extern_info[i].data_offset);
        cursor += sizeof(uint32_t);
        WRITE_U32(cursor, dat->extern_info[i].symbol_offset);
        cursor += sizeof(uint32_t);
    }

    uint32_t symbol_size = dat->symbol_size;
    if (dat->symbols != NULL) memcpy(cursor, dat->symbols, symbol_size);
    cursor += symbol_size;

    uint32_t file_size = (uint32_t)(cursor - out);
    WRITE_U32(out, file_size);

    *size = (size_t)file_size;
    return DAT_SUCCESS;
}

DAT_RET dat_file_new(DatFile *dat) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    *dat = (DatFile) {0};
    return DAT_SUCCESS;
}

DAT_RET dat_file_destroy(DatFile *dat) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;

    free(dat->data);
    free(dat->reloc_targets);
    free(dat->root_info);
    free(dat->extern_info);
    free(dat->symbols);
    free(dat->objects);
    dat_file_new(dat);

    return DAT_SUCCESS;
}

DAT_RET dat_file_debug_print(DatFile *dat) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;

    printf("DEBUG DAT @ %p:\n", (void*)dat);
    printf("MEMBER data          %p\n", (void*)dat->data         );
    printf("MEMBER reloc_targets %p\n", (void*)dat->reloc_targets);
    printf("MEMBER root_info     %p\n", (void*)dat->root_info    );
    printf("MEMBER extern_info   %p\n", (void*)dat->extern_info  );
    printf("MEMBER symbols       %p\n", (void*)dat->symbols      );
    printf("MEMBER objects       %p\n", (void*)dat->objects      );

    printf("MEMBER data_size       %u\n", dat->data_size      );
    printf("MEMBER reloc_count     %u\n", dat->reloc_count    );
    printf("MEMBER root_count      %u\n", dat->root_count     );
    printf("MEMBER extern_count    %u\n", dat->extern_count   );
    printf("MEMBER symbol_size     %u\n", dat->symbol_size    );
    printf("MEMBER object_count    %u\n", dat->object_count   );
    
    printf("MEMBER data_capacity   %u\n", dat->data_capacity  );
    printf("MEMBER reloc_capacity  %u\n", dat->reloc_capacity );
    printf("MEMBER root_capacity   %u\n", dat->root_capacity  );
    printf("MEMBER extern_capacity %u\n", dat->extern_capacity);
    printf("MEMBER symbol_capacity %u\n", dat->symbol_capacity);
    printf("MEMBER object_capacity %u\n", dat->object_capacity);

    for (uint32_t i = 0; i < dat->root_count; ++i) {
        DatRootInfo info = dat->root_info[i];
        printf("ROOT %06x %s\n", info.data_offset, &dat->symbols[info.symbol_offset]);
    }
    
    for (uint32_t i = 0; i < dat->extern_count; ++i) {
        DatExternInfo info = dat->extern_info[i];
        printf("EXTERN %06x %s\n", info.data_offset, &dat->symbols[info.symbol_offset]);
    }
    
    for (uint32_t i = 0; i < dat->object_count; ++i) {
        DatRef object = dat->objects[i];
        DatRef end;
        if (i+1 < dat->object_count) 
            end = dat->objects[i+1];
        else
            end = dat->data_size;
        printf("OBJECT %06x (%u)\n", object, end-object);
    }

    return DAT_SUCCESS;
}

uint32_t dat_file_reloc_idx(const DatFile *dat, DatRef ref) {
    return binary_search_refs(dat->reloc_targets, dat->reloc_count, ref);
}

DAT_RET dat_obj_alloc(DatFile *dat, uint32_t size, DatRef *out) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (out == NULL) return DAT_ERR_NULL_PARAM;

    uint32_t obj_offset = align_forward(dat->data_size, 4);
    uint32_t new_data_size = obj_offset + size;

    while (new_data_size > dat->data_capacity) {
        DAT_RET err = realloc_arr((void **)&dat->data, &dat->data_capacity, 1);
        if (err) return err;
    }
    
    while (dat->object_count >= dat->object_capacity) {
        DAT_RET err = realloc_arr((void **)&dat->objects, &dat->object_capacity, 1);
        if (err) return err;
    }
    dat->objects[dat->object_count++] = obj_offset;

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

        memmove(
            &dat->reloc_targets[reloc_idx+1],
            &dat->reloc_targets[reloc_idx],
            (count-reloc_idx) * sizeof(*dat->reloc_targets)
        );

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
    memmove(
        &dat->reloc_targets[reloc_idx],
        &dat->reloc_targets[reloc_idx+1],
        (dat->reloc_count-reloc_idx-1) * sizeof(*dat->reloc_targets)
    );
    dat->reloc_count--;

    return DAT_SUCCESS;
}

DAT_RET dat_obj_read_ref(DatFile *dat, DatRef ptr, DatRef *out) {
    return dat_obj_read_u32(dat, ptr, out);
}

DAT_RET dat_obj_read_u32(DatFile *dat, DatRef ptr, uint32_t *out) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (ptr & 3) return DAT_ERR_INVALID_ALIGNMENT;
    if (ptr + 4 > dat->data_size) return DAT_ERR_OUT_OF_BOUNDS;

    *out = READ_U32(&dat->data[ptr]);
    return DAT_SUCCESS;
}

DAT_RET dat_obj_read_u16(DatFile *dat, DatRef ptr, uint16_t *out) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (ptr & 1) return DAT_ERR_INVALID_ALIGNMENT;
    if (ptr + 2 > dat->data_size) return DAT_ERR_OUT_OF_BOUNDS;

    *out = READ_U16(&dat->data[ptr]);
    return DAT_SUCCESS;
}

DAT_RET dat_obj_read_u8(DatFile *dat, DatRef ptr, uint8_t *out) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (ptr + 1 > dat->data_size) return DAT_ERR_OUT_OF_BOUNDS;

    *out = dat->data[ptr];
    return DAT_SUCCESS;
}

DAT_RET dat_obj_write_u32(DatFile *dat, DatRef ptr, uint32_t num) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (ptr & 3) return DAT_ERR_INVALID_ALIGNMENT;
    if (ptr + 4 > dat->data_size) return DAT_ERR_OUT_OF_BOUNDS;
    
    WRITE_U32(&dat->data[ptr], num);
    return DAT_SUCCESS;
}

DAT_RET dat_obj_write_u16(DatFile *dat, DatRef ptr, uint16_t num) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (ptr & 1) return DAT_ERR_INVALID_ALIGNMENT;
    if (ptr + 2 > dat->data_size) return DAT_ERR_OUT_OF_BOUNDS;

    WRITE_U16(&dat->data[ptr], num);
    return DAT_SUCCESS;
}

DAT_RET dat_obj_write_u8(DatFile *dat, DatRef ptr, uint8_t num) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (ptr + 1 > dat->data_size) return DAT_ERR_OUT_OF_BOUNDS;

    dat->data[ptr] = num;
    return DAT_SUCCESS;
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

    memmove(
        &dat->root_info[index+1],
        &dat->root_info[index],
        (root_count-index) * sizeof(*dat->root_info)
    );

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

    memmove(
        &dat->root_info[index],
        &dat->root_info[index+1],
        (root_count-index-1) * sizeof(*dat->root_info)
    );
    dat->root_count--;

    return DAT_SUCCESS;
}

DAT_RET dat_root_find(DatFile *dat, const char *root_name, DatRef *out) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (root_name == NULL) return DAT_ERR_NULL_PARAM;
    if (out == NULL) return DAT_ERR_NULL_PARAM;
    
    for (uint32_t root_i = 0; root_i < dat->root_count; ++root_i) {
        DatRootInfo info = dat->root_info[root_i];
        char *root_name_to_check = &dat->symbols[info.symbol_offset];
        
        for (uint64_t i = 0; ; ++i) {
            char a = root_name_to_check[i];
            char b = root_name[i];
            
            if (a != b) break;
            if (a == 0) {
                *out = info.data_offset;
                return DAT_SUCCESS;
            };
        }
    }
    
    return DAT_NOT_FOUND;
}

DAT_RET dat_obj_location(const DatFile *dat, DatRef ptr, DatSlice *out) {
    if (dat == NULL) return DAT_ERR_NULL_PARAM;
    if (dat->object_count == 0) return DAT_NOT_FOUND;
    
    uint32_t object_idx = binary_search_refs(dat->objects, dat->object_count, ptr);
    
    if (dat->object_count == object_idx)
        object_idx--;
    
    if (dat->objects[object_idx] > ptr) {
        if (object_idx == 0) return DAT_NOT_FOUND;
        object_idx--;
    }
    
    if (dat->objects[object_idx] > ptr)
        return DAT_NOT_FOUND;
        
    uint32_t offset = dat->objects[object_idx];
    uint32_t end; 
    if (object_idx+1 < dat->object_count)
        end = dat->objects[object_idx+1];
    else
        end = dat->data_size;
    
    *out = (DatSlice) { offset, end-offset };
    return DAT_SUCCESS;
}

DAT_RET dat_obj_copy_inner(
    DatFile *dst, DatFile *src, DatRef src_ref, DatRef *dst_out,
    DatRef *src_lookup, DatRef *dst_lookup, uint32_t *lookup_count 
) {
    // find src object location
    DatSlice obj_location;
    DAT_RET err = dat_obj_location(src, src_ref, &obj_location);
    if (err) return err;
    
    // alloc and copy object to dst
    DatRef dst_ref;
    dat_obj_alloc(dst, obj_location.size, &dst_ref);
    *dst_out = dst_ref;
    memcpy(&dst->data[dst_ref], &src->data[src_ref], obj_location.size);
            
    src_lookup[*lookup_count] = src_ref;
    dst_lookup[*lookup_count] = dst_ref;
    (*lookup_count)++;
    
    // Recursively copy child objects
    DatRef obj_end = obj_location.offset + obj_location.size;
    uint32_t reloc_i = dat_file_reloc_idx(src, obj_location.offset);
    while (1) {
        // find next child ref
        if (reloc_i == src->reloc_count) break;
        DatRef src_child_ref_offset = src->reloc_targets[reloc_i];
        if (src_child_ref_offset >= obj_end) break;
        
        // read child object offset
        DatRef dst_child_ref, src_child_ref;
        err = dat_obj_read_ref(src, src_child_ref_offset, &src_child_ref);
        if (err) return err;
        
        // check if already copied
        bool found = false;
        for (uint32_t i = 0; i < *lookup_count; ++i) {
            if (src_lookup[i] == src_child_ref) {
                found = true;
                dst_child_ref = dst_lookup[i];
                break;
            }
        }
        
        if (!found) {
            // copy child object
            err = dat_obj_copy_inner(
                dst, src, src_child_ref, &dst_child_ref,
                src_lookup, dst_lookup, lookup_count
            );
            if (err) return err;
        }
        
        // replace with new child pointer
        DatRef dst_child_ref_offset = dst_ref + src_child_ref_offset - obj_location.offset;  
        err = dat_obj_set_ref(dst, dst_child_ref_offset, dst_child_ref);
        if (err) return err;
        
        reloc_i++;
    }
    
    return DAT_SUCCESS;
}

DAT_RET dat_obj_copy(DatFile *dst, DatFile *src, DatRef src_ref, DatRef *dst_out) {
    if (dst == NULL) return DAT_ERR_NULL_PARAM;
    if (src == NULL) return DAT_ERR_NULL_PARAM;
    if (src_ref >= src->data_size) return DAT_ERR_OUT_OF_BOUNDS;
    
    // Create object list for deduplicating children
    // TODO: change to hash table or sort - something not O(n)
    uint32_t object_array_size = src->object_count * sizeof(DatRef);  
    uint8_t *buf = malloc(object_array_size * 2);
    DatRef *src_lookup = (DatRef*)buf;
    DatRef *dst_lookup = (DatRef*)(buf + object_array_size);
    uint32_t lookup_count = 0;
    
    DAT_RET ret = dat_obj_copy_inner(
        dst, src, src_ref, dst_out,
        src_lookup, dst_lookup, &lookup_count
    );
    
    free(buf); 
    
    return ret;
}

const char *dat_return_string(DAT_RET ret) {
    switch (ret) {
        case DAT_SUCCESS:
            return "success";
        case DAT_NOT_FOUND:
            return "not found";
        case DAT_ERR_NULL_PARAM:
            return "null parameter passed";
        case DAT_ERR_ALLOCATION_FAILURE:
            return "allocation failed";
        case DAT_ERR_INVALID_SIZE:
            return "size is invalid";
        case DAT_ERR_INVALID_ALIGNMENT:
            return "alignment is invalid";
        case DAT_ERR_OUT_OF_BOUNDS:
            return "out of bounds";
    }

    return "unknown return value";
}

// Returns either the matching index or the insertion index.
static uint32_t binary_search_refs(const DatRef *refs, uint32_t count, DatRef ref) {
    uint32_t left = 0;
    uint32_t right = count;
    while (left < right) {
        uint32_t mid = left + count / 2;
        DatRef m = refs[mid];

        if (m < ref) left = mid + 1;
        else if (m > ref) right = mid;
        else return mid;

        count = right - left;
    }
    
    return right;
}
