#include "dat.h"
#include "dat.c"
#include "utils.h"
#include "ml.h"

void print_err(const char *test_name, int line, const char *cond) {
    fprintf(stderr, "TEST \"%s\" FAILED\n", test_name);
    fprintf(stderr, "line %i\n", line);
    fprintf(stderr, "expected '%s'\n", cond);
}

#define TEST_INNER(A, C) if ((A) C) print_err(test_name, __LINE__, #A)
#define DAT_TEST(A) TEST_INNER(A, != DAT_SUCCESS)
#define EXPECT(A) TEST_INNER(A, == false)
#define STR_EXPECT_FREE(A, B) do { char *_a; EXPECT(strcmp(_a = A, B) == 0); free(_a); } while (0)
#define STR_EXPECT(A, B) EXPECT(strcmp(A, B) == 0)

void test_ml(void);
void test_dat(void);
void test_map(void);
void test_path_utils(void);

int main(void) {
    // test_path_utils();
    // test_map();
    // test_dat();
    test_ml();

    return 0;
}

void pjobj(DatFile *grps, ML_JObjDesc *jobjdesc) {
    // float x = ML_ReadF32(jobjdesc->position.x);
    // float y = ML_ReadF32(jobjdesc->position.y);
    // float z = ML_ReadF32(jobjdesc->position.z);
    // printf("%.1f %.1f %.1f\n", x, y, z);
    if (ML_IsNonNull(jobjdesc->next)) {
        pjobj(grps, ML_ReadRef_JObjDesc(grps, jobjdesc->next));
    }
}

void test_ml(void) {
    const char *test_name = "";
    DatFile grps;
    
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
        
        DAT_TEST(dat_file_import(grps_buf, grps_size, &grps));
        free(grps_buf);
    }
    
    {
        DatRef map_head_offset;
        DAT_TEST(dat_root_find(&grps, "map_head", &map_head_offset));
        ML_MapHead *map_head = ML_ReadDatRef(&grps, map_head_offset);
        int map_head_count = ML_ReadI32(map_head->map_gobjdesc_count);
        
        for (int i = 0; i < map_head_count; ++i) {
            ML_MapGObjDesc *map_gobjdesc = ML_ReadRef_MapGObjDesc(&grps, map_head->map_gobjdescs) + i;
            ML_JObjDesc *jobjdesc = ML_ReadRef_JObjDesc(&grps, map_gobjdesc->jobjset.jobjdesc);
            // pjobj(&grps, jobjdesc);
        }
    }
    
    DAT_TEST(dat_file_destroy(&grps));
}

void test_path_utils(void) {
    const char *test_name = "";
    
    {
        test_name = "inner_name";
        STR_EXPECT_FREE(inner_name("test/abcd.xyz"), "abcd");
        STR_EXPECT_FREE(inner_name("abcd"), "abcd");
        STR_EXPECT_FREE(inner_name(""), "");
        STR_EXPECT_FREE(inner_name(".test.abc"), ".test");
        STR_EXPECT_FREE(inner_name("/temp/.test.abc"), ".test");
        STR_EXPECT_FREE(inner_name("/temp/.test"), ".test");
        STR_EXPECT_FREE(inner_name(".test"), ".test");
    }
    
    {
        test_name = "strip_ext";
        char buf[32];
        
        strcpy(buf, "test/abcd.xyz"); strip_ext(buf); STR_EXPECT(buf, "test/abcd");
        strcpy(buf, "test/abcd"); strip_ext(buf); STR_EXPECT(buf, "test/abcd");
        strcpy(buf, "test/abcd.a.xyz"); strip_ext(buf); STR_EXPECT(buf, "test/abcd.a");
        strcpy(buf, ".abcd"); strip_ext(buf); STR_EXPECT(buf, ".abcd");
        strcpy(buf, ".abcd.xyz"); strip_ext(buf); STR_EXPECT(buf, ".abcd");
        strcpy(buf, ""); strip_ext(buf); STR_EXPECT(buf, "");
    }
    
    {
        test_name = "strip_filename";
        char buf[32];
        
        strcpy(buf, "test/abcd.xyz"); strip_filename(buf); STR_EXPECT(buf, "test/");
        strcpy(buf, "/test/abcd.xyz"); strip_filename(buf); STR_EXPECT(buf, "/test/");
        strcpy(buf, "./"); strip_filename(buf); STR_EXPECT(buf, "./");
        strcpy(buf, "./testing/t"); strip_filename(buf); STR_EXPECT(buf, "./testing/");
        strcpy(buf, "./testing/"); strip_filename(buf); STR_EXPECT(buf, "./testing/");
        strcpy(buf, "~/.././testing"); strip_filename(buf); STR_EXPECT(buf, "~/.././");
        strcpy(buf, ""); strip_filename(buf); STR_EXPECT(buf, "");
    }
    
    {
        test_name = "filename";
        STR_EXPECT(filename("test.xyz"), "test.xyz");
        STR_EXPECT(filename("a/b/test.xyz"), "test.xyz");
        STR_EXPECT(filename("/a//test.xyz"), "test.xyz");
        STR_EXPECT(filename("/a//test"), "test");
        STR_EXPECT(filename("/a//"), "");
    }
}

void test_map(void) {
    const char *test_name = "";
    Map map = map_alloc(2);
    
    {
        test_name = "insert lookup";
        
        EXPECT(!map_insert(&map, map_hash_str("testing"), 1234));
        uint32_t *res = map_find(&map, map_hash_str("testing"));
        EXPECT(res != NULL);
        EXPECT(*res == 1234);
        
        EXPECT(map_find(&map, map_hash_str("123")) == NULL);
        
        EXPECT(map_remove(&map, map_hash_str("123")));
        EXPECT(!map_remove(&map, map_hash_str("testing")));
        EXPECT(map_find(&map, map_hash_str("testing")) == NULL);
    }
    
    {
        test_name = "fill";
        
        EXPECT(!map_insert(&map, map_hash_str("1"), 1));
        EXPECT(!map_insert(&map, map_hash_str("2"), 2));
        EXPECT(!map_insert(&map, map_hash_str("3"), 3));
        EXPECT(!map_insert(&map, map_hash_str("4"), 4));
        
        EXPECT(*map_find(&map, map_hash_str("1")) == 1);
        EXPECT(*map_find(&map, map_hash_str("2")) == 2);
        EXPECT(*map_find(&map, map_hash_str("3")) == 3);
        EXPECT(*map_find(&map, map_hash_str("4")) == 4);
        
        EXPECT(map_insert(&map, map_hash_str("5"), 5));
        EXPECT(!map_remove(&map, map_hash_str("2")));
        EXPECT(map_find(&map, map_hash_str("2")) == NULL);
        EXPECT(!map_insert(&map, map_hash_str("5"), 5));
        
        EXPECT(*map_find(&map, map_hash_str("1")) == 1);
        EXPECT(map_find(&map, map_hash_str("2")) == NULL);
        EXPECT(*map_find(&map, map_hash_str("3")) == 3);
        EXPECT(*map_find(&map, map_hash_str("4")) == 4);
        EXPECT(*map_find(&map, map_hash_str("5")) == 5);
    }
    
    {
        test_name = "hashing";
        
        EXPECT(map_hash_str("") == map_hash_str_len("", 0));
        EXPECT(map_hash_str("a") == map_hash_str_len("a", 1));
        EXPECT(map_hash_str("testing") == map_hash_str_len("testing", 7));
    }
        
    
    map_free(&map);
}

void test_dat(void) {
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
        
        // find
        
        DatRef found_ref1, found_ref2, found_ref3;
        
        DAT_TEST(dat_root_find(&dat, root1, &found_ref1));
        DAT_TEST(dat_root_find(&dat, root2, &found_ref2));
        DAT_TEST(dat_root_find(&dat, root3, &found_ref3));
        EXPECT(found_ref1 == root1_ref);
        EXPECT(found_ref2 == root2_ref);
        EXPECT(found_ref3 == root3_ref);
        
        EXPECT(dat_root_find(&dat, "asdhaksjdh", &found_ref1) != DAT_SUCCESS);
        
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
        
        for (uint32_t i = 0; i < grps.object_count; ++i) {
            DatRef object = grps.objects[i];
            EXPECT(object < grps.data_size);
        }
         
        for (uint32_t i = 0; i < grps.root_count; ++i) {
            DatRootInfo root = grps.root_info[i];
            EXPECT(root.data_offset < grps.data_size);
            EXPECT(root.symbol_offset < grps.symbol_size);
        }
         
        for (uint32_t i = 0; i < grps.extern_count; ++i) {
            DatExternInfo ext = grps.extern_info[i];
            EXPECT(ext.data_offset < grps.data_size);
            EXPECT(ext.symbol_offset < grps.symbol_size);
        } 
        
        DAT_TEST(dat_file_destroy(&grps));
        free(grps_buf);
    }
    
    DAT_TEST(dat_file_destroy(&dat));
}
