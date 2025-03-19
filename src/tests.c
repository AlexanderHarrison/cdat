#include "dat.h"
#include "dat.c"

#define TEST_INNER(A, C) do {\
    if ((A) C) {\
        fprintf(stderr, "TEST \"%s\" FAILED\n", test_name);\
        fprintf(stderr, "line %i\n", __LINE__);\
        fprintf(stderr, "expected '" #A "'\n");\
    }\
} while (0)
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
        DAT_TEST(dat_obj_set_ref(&dat, ref2 + 0x4, ref3));
        DAT_TEST(dat_obj_set_ref(&dat, ref2 + 0x8, ref4));
        
        DatFile dst;
        DatRef dst_ref1;
        DAT_TEST(dat_file_new(&dst));
        DAT_TEST(dat_obj_copy(&dst, &dat, ref1, &dst_ref1));
        
        EXPECT(dst.object_count == 4);
        EXPECT(dst.reloc_count == 3);
        
        EXPECT(dst.objects[0] == 0);
        EXPECT(dst.objects[1] == 64);
        EXPECT(dst.objects[2] == 128);
        EXPECT(dst.objects[3] == 192);
        
        DAT_TEST(dat_file_destroy(&dst));
    }
    
    DAT_TEST(dat_file_destroy(&dat));
}
