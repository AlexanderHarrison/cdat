#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include "dat.h"
#include "dat.c"
#include "ml.h"

uint8_t dat_buffer[1<<21];

#define dat_expect(E) do {\
    if (E != DAT_SUCCESS) {\
        printf("expect failed: " __FILE__ ":%u\n", __LINE__);\
        exit(1);\
    }\
} while (0)

int main(void) {
    fread(dat_buffer, 1, sizeof(dat_buffer), fopen("GrPs.dat", "rb"));
    DatFile grps;
    dat_expect(dat_file_import(dat_buffer, sizeof(dat_buffer), &grps));
    
    return 0;
}

void test(DatFile *f, ML_JObjDescRef jobjdesc_ref) {
    ML_JObjDesc *desc = ML_ReadRef_JObjDesc(f, jobjdesc_ref);
    desc = ML_ReadRef_JObjDesc(f, desc->child);
}
