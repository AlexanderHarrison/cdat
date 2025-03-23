#include "dat.h"
#include "dat.c"

void print_err(const char *test_name, int line, const char *cond) {
    fprintf(stderr, "TEST \"%s\" FAILED\n", test_name);
    fprintf(stderr, "line %i\n", line);
    fprintf(stderr, "expected '%s'\n", cond);
}

#define TEST_INNER(A, C) if ((A) C) print_err(test_name, __LINE__, #A)
#define DAT_TEST(A) TEST_INNER(A, != DAT_SUCCESS)
#define EXPECT(A) TEST_INNER(A, == false)

int main(void) {
    const char *test_name = "";
    DatFile dat;
    
    {
        test_name = "create dat";
        DAT_TEST(dat_file_new(&dat));
    }
    
    {
        test_name = "allcate objects";
        DAT_TEST(dat_file_new(&dat));
        DatRef obj1, obj2, obj3, obj4;
        DAT_TEST(dat_obj_alloc(&dat, 256, &obj1));
        DAT_TEST(dat_obj_alloc(&dat, 33, &obj2));
        DAT_TEST(dat_obj_alloc(&dat, 0, &obj3));
        DAT_TEST(dat_obj_alloc(&dat, 8, &obj4));
        EXPECT(obj1 == 0);
        EXPECT(obj2 == 256);
        EXPECT(obj3 == 292);
        EXPECT(obj4 == 292);
        EXPECT(dat.object_count == 4);
        
        EXPECT(dat.objects[0] == obj1);
        EXPECT(dat.objects[1] == obj2);
        EXPECT(dat.objects[2] == obj3);
        EXPECT(dat.objects[3] == obj4);
    }
    
    {
        test_name = "add and remove root nodes";
        
        // add
        
        const char *root1 = "root1";
        const char *root2 = "root2";
        const char *root3 = "root3";
        DatRef root1_ref, root2_ref, root3_ref;
        DAT_TEST(dat_obj_alloc(&dat, 128, &root1_ref));
        DAT_TEST(dat_obj_alloc(&dat, 128, &root2_ref));
        DAT_TEST(dat_obj_alloc(&dat, 128, &root3_ref));
        DAT_TEST(dat_root_add(&dat, 0, root2_ref, root2));
        DAT_TEST(dat_root_add(&dat, 1, root3_ref, root3));
        DAT_TEST(dat_root_add(&dat, 0, root1_ref, root1));
        
        EXPECT(dat.root_count == 3);
        
        DatRootInfo info = dat.root_info[0];
        EXPECT(info.data_offset == root1_ref);
        EXPECT(strcmp(dat.symbols + info.symbol_offset, root1) == 0);
        
        info = dat.root_info[1];
        EXPECT(info.data_offset == root2_ref);
        EXPECT(strcmp(dat.symbols + info.symbol_offset, root2) == 0);
        
        info = dat.root_info[2];
        EXPECT(info.data_offset == root3_ref);
        EXPECT(strcmp(dat.symbols + info.symbol_offset, root3) == 0);
        
        // remove
        
        DAT_TEST(dat_root_remove(&dat, 1));
        EXPECT(dat.root_count == 2);
        
        info = dat.root_info[0];
        EXPECT(info.data_offset == root1_ref);
        EXPECT(strcmp(dat.symbols + info.symbol_offset, root1) == 0);
        
        info = dat.root_info[1];
        EXPECT(info.data_offset == root3_ref);
        EXPECT(strcmp(dat.symbols + info.symbol_offset, root3) == 0);
    }
    
    {
        test_name = "read/writes";
        DatRef ref1;
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref1));
        
        DAT_TEST(dat_obj_write_u32(&dat, ref1 + 0x0, 0x12345678));
        DAT_TEST(dat_obj_write_u16(&dat, ref1 + 0x4, 0x1234));
        DAT_TEST(dat_obj_write_u8(&dat, ref1  + 0x6, 0x12));
        
        EXPECT(dat.data[ref1+0x0] == 0x12);
        EXPECT(dat.data[ref1+0x1] == 0x34);
        EXPECT(dat.data[ref1+0x2] == 0x56);
        EXPECT(dat.data[ref1+0x3] == 0x78);
        
        EXPECT(dat.data[ref1+0x4] == 0x12);
        EXPECT(dat.data[ref1+0x5] == 0x34);
        
        EXPECT(dat.data[ref1+0x6] == 0x12);
    }
    
    {
        test_name = "references";
        
        // add
        
        DatRef ref1, ref2, ref3, ref4;
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref1));
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref2));
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref3));
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref4));
        
        DAT_TEST(dat_obj_set_ref(&dat, ref1 + 0x0, ref2));
        DAT_TEST(dat_obj_set_ref(&dat, ref2 + 0x4, ref3));
        DAT_TEST(dat_obj_set_ref(&dat, ref2 + 0x8, ref4));
        
        EXPECT(dat.reloc_targets[0] == ref1 + 0x0);
        EXPECT(dat.reloc_targets[1] == ref2 + 0x4);
        EXPECT(dat.reloc_targets[2] == ref2 + 0x8);
        
        DatRef reloc1, reloc2, reloc3;
        DAT_TEST(dat_obj_read_ref(&dat, dat.reloc_targets[0], &reloc1));
        DAT_TEST(dat_obj_read_ref(&dat, dat.reloc_targets[1], &reloc2));
        DAT_TEST(dat_obj_read_ref(&dat, dat.reloc_targets[2], &reloc3));
        
        EXPECT(reloc1 == ref2);
        EXPECT(reloc2 == ref3);
        EXPECT(reloc3 == ref4);
        
        // remove
        
        DAT_TEST(dat_obj_remove_ref(&dat, ref2 + 0x4));
        
        EXPECT(dat.reloc_targets[0] == ref1 + 0x0);
        EXPECT(dat.reloc_targets[1] == ref2 + 0x8);
        
        DAT_TEST(dat_obj_read_ref(&dat, dat.reloc_targets[0], &reloc1));
        DAT_TEST(dat_obj_read_ref(&dat, dat.reloc_targets[1], &reloc2));
        
        EXPECT(reloc1 == ref2);
        EXPECT(reloc2 == ref4);
    }
    
    {
        test_name = "copy object";
         
        DatRef ref1, ref2, ref3, ref4;
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref1));
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref2));
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref3));
        DAT_TEST(dat_obj_alloc(&dat, 64, &ref4));
        
        DAT_TEST(dat_obj_set_ref(&dat, ref1 + 0x0, ref2));
        DAT_TEST(dat_obj_set_ref(&dat, ref2 + 0x0, ref2));
        DAT_TEST(dat_obj_set_ref(&dat, ref2 + 0x4, ref3));
        DAT_TEST(dat_obj_set_ref(&dat, ref2 + 0x8, ref4));
        DAT_TEST(dat_obj_set_ref(&dat, ref4 + 0x0, ref1));
        
        DatFile dst;
        DatRef dst_ref1;
        DAT_TEST(dat_file_new(&dst));
        DAT_TEST(dat_obj_copy(&dst, &dat, ref1, &dst_ref1));
        
        EXPECT(dst.object_count == 4);
        EXPECT(dst.reloc_count == 5);
        
        EXPECT(dst.objects[0] == 0);
        EXPECT(dst.objects[1] == 64);
        EXPECT(dst.objects[2] == 128);
        EXPECT(dst.objects[3] == 192);
        
        DatRef reloc1, reloc2, reloc3, reloc4, reloc5;
        DAT_TEST(dat_obj_read_ref(&dst, dst.reloc_targets[0], &reloc1));
        DAT_TEST(dat_obj_read_ref(&dst, dst.reloc_targets[1], &reloc2));
        DAT_TEST(dat_obj_read_ref(&dst, dst.reloc_targets[2], &reloc3));
        DAT_TEST(dat_obj_read_ref(&dst, dst.reloc_targets[3], &reloc4));
        DAT_TEST(dat_obj_read_ref(&dst, dst.reloc_targets[4], &reloc5));
        
        EXPECT(reloc1 == dst.objects[1]);
        EXPECT(reloc2 == dst.objects[1]);
        EXPECT(reloc3 == dst.objects[2]);
        EXPECT(reloc4 == dst.objects[3]);
        EXPECT(reloc5 == dst.objects[0]);
        
        DAT_TEST(dat_file_destroy(&dst));
    }
    
    {
        test_name = "import / export";
        
        uint32_t export_max_size = dat_file_export_max_size(&dat);
        uint8_t *data = malloc(export_max_size);
        uint32_t export_size;
        DAT_TEST(dat_file_export(&dat, data, &export_size));
        EXPECT(export_size <= export_max_size);
        
        DatFile new;
        DAT_TEST(dat_file_import(data, export_size, &new));
        
        EXPECT(new.data_size == dat.data_size);
        EXPECT(new.reloc_count == dat.reloc_count);
        EXPECT(new.root_count == dat.root_count);
        EXPECT(new.extern_count == dat.extern_count);
        EXPECT(new.symbol_size == dat.symbol_size);
        
        EXPECT(memcmp(new.data, dat.data, new.data_size) == 0);
        EXPECT(memcmp(new.reloc_targets, dat.reloc_targets, new.reloc_count*sizeof(*new.reloc_targets)) == 0);
        EXPECT(memcmp(new.root_info, dat.root_info, new.root_count*sizeof(*new.root_info)) == 0);
        // These are null at the moment, memcmp doesn't like that
        //EXPECT(memcmp(new.extern_info, dat.extern_info, new.extern_count*sizeof(*new.extern_info)) == 0);
        EXPECT(memcmp(new.symbols, dat.symbols, new.symbol_size) == 0);
        
        DAT_TEST(dat_file_destroy(&new));
        free(data);
    }
    
    {
        test_name = "import ssbm grps dat";
        
        // file io
        FILE *grps_f = fopen("GrPs.dat", "r");
        EXPECT(fseek(grps_f, 0, SEEK_END) >= 0);
        int64_t ftell_ret = ftell(grps_f);
        EXPECT(ftell_ret > 0);
        EXPECT(fseek(grps_f, 0, SEEK_SET) >= 0);
        uint32_t grps_size = (uint32_t)ftell_ret; 
        uint8_t *grps_buf = malloc(grps_size);
        EXPECT(grps_buf != NULL);
        EXPECT(fread(grps_buf, grps_size, 1, grps_f) == 1);
        
        DatFile grps;
        DAT_TEST(dat_file_import(grps_buf, grps_size, &grps));
        
        DAT_TEST(dat_file_destroy(&grps));
        free(grps_buf);
    }
    
    DAT_TEST(dat_file_destroy(&dat));
    
    return 0;
}
