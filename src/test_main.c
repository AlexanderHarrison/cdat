#include "cdat.h"

#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

uint8_t *read_file(const char *filename, uint32_t *size) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) return NULL;

    fseek(file, 0, SEEK_END);
    uint32_t file_size = (uint32_t)ftell(file);
    *size = file_size;
    rewind(file);
    uint8_t *buffer = malloc(file_size);
    uint64_t bytes_read = fread(buffer, 1, file_size, file);

    if (bytes_read != file_size) {
        free(buffer);
        fclose(file);
        return NULL;
    }

    fclose(file);
    return buffer;
}

void write_file(const char *filename, uint8_t *buf, uint32_t size) {
    FILE *file = fopen(filename, "wb+");
    assert(file != NULL);
    uint64_t bytes_read = fwrite(buf, 1, size, file);
    assert(bytes_read == size);
    fclose(file);
}

int main(void) {
    uint32_t size;
    uint8_t *dat_file = read_file("lab.dat", &size);
    assert(dat_file);

    DatFile dat;
    assert(dat_file_import(dat_file, size, &dat) == DAT_SUCCESS);

    DatRef test_obj;
    assert(dat_obj_alloc(&dat, 0x4, &test_obj) == DAT_SUCCESS);
    dat_obj_write_u32(&dat, test_obj+0, 54);
    assert(dat_obj_read_u32(&dat, test_obj+0) == 54);
    assert(dat_root_add(&dat, dat.root_count, test_obj, "wowee!") == DAT_SUCCESS);

    dat_file_debug_print(&dat);
    free(dat_file);
    assert(dat_file_destroy(&dat) == DAT_SUCCESS);

    return 0;
}
