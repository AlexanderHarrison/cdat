#include <libgen.h>
#include <stdarg.h>
#include <elf.h>

#include "utils.h"

#include "dat.h"
#include "dat.c"

#define MAX_CMD_LEN 8192
#define DEFAULT_GCC_FLAGS "-O1 -Wall -Wextra"

static const char *HELP = "\
USAGE:\n\
    hmex [flags]\n\
\n\
REQUIRED FLAGS:\n\
    -i <file.c file2.o ...>     : Input filepaths.\n\
    -T <melee.ld>               : Linker script symbol file.\n\
    -t <symbol-table.txt>       : Symbol table.\n\
    -o <output.dat>             : Output dat file.\n\
\n\
OPTIONAL FLAGS:\n\
    -h                          : Show hmex usage.\n\
    -dat <inputs.dat>           : Input dat file.\n\
                                    Is an empty dat file by default.\n\
    -f <gcc flags>              : Flags to pass to gcc. Optimization, warnings, etc.\n\
                                    Is '" DEFAULT_GCC_FLAGS "' by default.\n\
    -s <symbol name>            : Symbol name.\n\
                                    Is the symbol table filename (excluding extension) by default.\n\
";

enum ArgFlags {
    Arg_Help = (1ul << 0),
};

typedef struct Args {
    // env vars
    const char *devkitppc_path;     // DEVKITPPC
    
    // required arguments
    const char **input_filepaths;   // -i
    uint32_t input_filepaths_count;
    const char *linker_script_path; // -T
    const char *symbol_table_path;  // -t
    const char *output_dat_path;    // -o
    
    // optional arguments
    const char *input_dat_path;     // -dat
    const char *gcc_flags;          // -f
    const char *symbol_name;        // -s
    
    uint64_t flags;
} Args;

static Args args;
static NoArgFlag no_arg_flags[] = {
    { "-h", Arg_Help },
}; 
static SingleArgFlag single_arg_flags[] = { 
    { "-T", &args.linker_script_path },
    { "-t", &args.symbol_table_path },
    { "-o", &args.output_dat_path },
    { "-dat", &args.input_dat_path },
    { "-f", &args.gcc_flags },
    { "-s", &args.symbol_name },
};
static MultiArgFlag multi_arg_flags[] = {
    { "-i", &args.input_filepaths, &args.input_filepaths_count }
}; 

void parse_args(int argc, const char *argv[]) {
    // hardcode no arguments 
    if (argc == 1) {
        printf("%s", HELP);
        exit(0);
    }

    args.devkitppc_path = getenv("DEVKITPPC");
    
    read_args(
        argc, argv, &args.flags,
        no_arg_flags, countof(no_arg_flags),
        single_arg_flags, countof(single_arg_flags),
        multi_arg_flags, countof(multi_arg_flags)
    );
    
    // check arguments
    
    bool err = false;
    bool print_usage = false;
    
    if (args.flags & Arg_Help) {
        printf("%s", HELP);
        exit(0);
    }
    
    if (args.devkitppc_path == NULL) {
        fprintf(stderr, ERROR "$DEVKITPPC environment variable is not set! \
Please install devkitpro and the PPC/Gamecube package, \
and ensure the DEVKITPPC environment variable is set.\n");
        print_usage = true;
        err = true;
    } else {
        err |= check_path_access(args.devkitppc_path, R_OK);
    }
    
    if (args.input_filepaths_count == 0) {
        fprintf(stderr, ERROR "No input files passed! Use '-i' to pass input files.\n");
        print_usage = true;
        err = true;
    } else {
        for (uint32_t i = 0; i < args.input_filepaths_count; ++i) {
            const char *ip_path = args.input_filepaths[i];
            err |= check_path_access(ip_path, R_OK);
        }
    }
    
    if (args.linker_script_path == NULL) {
        fprintf(stderr, ERROR "No linker script passed! Use '-T' to pass a linker script.\n");
        print_usage = true;
        err = true;
    } else {
        err |= check_path_access(args.linker_script_path, R_OK);
    }
    
    if (args.symbol_table_path == NULL) {
        fprintf(stderr, ERROR "No symbol table passed! Use '-t' to pass a symbol table path.\n");
        print_usage = true;
        err = true;
    } else {
        err |= check_path_access(args.symbol_table_path, R_OK);
    }
    
    if (args.output_dat_path == NULL) {
        fprintf(stderr, ERROR "No output dat path passed! Use '-o' to pass an output dat path.\n");
        print_usage = true;
        err = true;
    }
    
    // optional arguments
    
    if (args.input_dat_path == NULL) {
        // Do nothing. This is allowed to be NULL.
    } else {
        err |= check_path_access(args.input_dat_path, R_OK);
    }
    
    if (args.gcc_flags == NULL)
        args.gcc_flags = DEFAULT_GCC_FLAGS;
        
    if (args.symbol_name == NULL && args.symbol_table_path != NULL) {
        // find filename of symbol table path and use that as the symbol name
        
        uint64_t len = strlen(args.symbol_table_path) + 1; 
        char *symbol_name = malloc(len);
        memcpy(symbol_name, args.symbol_table_path, len);
        
        // find last period (not including period as first character)
        uint64_t period = 0;
        for (uint64_t i = 1; i < len; ++i) {
            if (symbol_name[i] == '.')
                period = i;
        }
        // find last path separator
        uint64_t start = 0;
        for (uint64_t i = 0; i < len; ++i) {
            char c = symbol_name[i];  
            if (c == '/' || c == '\\')
                start = i+1;
        }
        // strip directory and extension
        if (period > 0)
            symbol_name[period] = 0;
        symbol_name = &symbol_name[start];
        
        args.symbol_name = symbol_name;
    }
    
    if (print_usage)
        fprintf(stderr, "\n%s", HELP);
    
    if (err)
        exit(1);
}

char *copy_arg(char *cmd, char *curcmd, const char *arg) {
    (void)cmd; // TODO - use for bounds checking
    // ensure quoted
    if (arg[0] == '"' || arg[0] == '\'') {
        curcmd = my_stpcpy(curcmd, arg);
    } else {
        curcmd = my_stpcpy(curcmd, "\"");
        curcmd = my_stpcpy(curcmd, arg);
        curcmd = my_stpcpy(curcmd, "\"");
    }
    curcmd = my_stpcpy(curcmd, " ");
    return curcmd;
}
 
char *copy_args(char *cmd, char *curcmd, const char *arg) {
    (void)cmd; // TODO - use for bounds checking
    // ensure NOT quoted - we want these to expand to multiple separate arguments,
    // for e.x. gcc flags.
    curcmd = my_stpcpy(curcmd, arg);
    curcmd = my_stpcpy(curcmd, " ");
    return curcmd;
}
    
typedef struct MEXReloc {
    uint32_t cmd_and_code_offset; // cmd in high byte, code_offset in low 3 bytes.
    uint32_t reloc_offset;
} MEXReloc;

typedef struct MEXSymbol {
    uint32_t symbol_idx;
    uint32_t elf_offset;
} MEXSymbol;

typedef struct CodeSection {
    uint32_t code_size;
    uint32_t elf_offset;
    DatRef dat_offset; 
} CodeSection;

bool match_section_name(const uint8_t *symbol, const uint8_t *section_name) {
    static const char header[] = ".text.";
    
    uint64_t i = 0;
    for (; i < 6; ++i) {
        if (section_name[i] != header[i])
            return false;
    }
    for (;; ++i) {
        uint8_t c = section_name[i];   
        if (c != symbol[i-6])
            return false;
        if (c == 0) break;
    }
    
    return true;
}

int main(int argc, const char *argv[]) {
    parse_args(argc, argv);
    
    char *gcc_path = path_join(args.devkitppc_path, "bin", "powerpc-eabi-gcc", NULL);
    if (check_path_access(gcc_path, R_OK | X_OK))
        exit(1);
    
    // import input dat file
    DatFile dat;
    {
        if (args.input_dat_path != NULL) {
            uint8_t *input_dat_bytes;
            uint64_t size; 
            if (read_file(args.input_dat_path, &input_dat_bytes, &size))
                exit(1);
            
            if (dat_file_import(input_dat_bytes, (uint32_t)size, &dat) != DAT_SUCCESS) {
                fprintf(stderr,
                    ERROR "Could not import dat file '%s'. File is not a dat file or is malformed.\n",
                    args.input_dat_path
                );
                exit(1);
            }
            
            free(input_dat_bytes);
        } else {
            dat_expect(dat_file_new(&dat));
        }
    }
    
    // parse symbol table file
    uint8_t **symbol_table;
    {
        uint8_t *st;
        uint64_t st_size;
        if (read_file(args.symbol_table_path, &st, &st_size))
            exit(1);
        symbol_table = read_lines(st, st_size);
    }
    
    // compile c files
    {
        char *cmd = malloc(MAX_CMD_LEN);
        char *curcmd = cmd; 
       
        curcmd = copy_arg (cmd, curcmd, gcc_path);
        curcmd = copy_args(cmd, curcmd, "-DGEKKO -mogc -mcpu=750 -meabi -mhard-float -c -o hmex.o");
        curcmd = copy_args(cmd, curcmd, "-T");
        curcmd = copy_arg (cmd, curcmd, args.linker_script_path);
        curcmd = copy_args(cmd, curcmd, args.gcc_flags);
        
        for (uint32_t i = 0; i < args.input_filepaths_count; ++i) {
            const char *ip_path = args.input_filepaths[i];
            curcmd = copy_arg(cmd, curcmd, ip_path);
        }
        
        printf("executing: %s\n", cmd);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, ERROR "compilation failed\n");
            exit(1);
        }
    }
    
    // open and parse elf file
    uint8_t *hmex_o;
    uint64_t hmex_o_size;
     
    MEXReloc *reloc = malloc(4096); // TEMP
    uint64_t reloc_count = 0;
    MEXSymbol *fn_table = malloc(4096); // TEMP
    uint64_t fn_count = 0;
    {
        if (read_file("hmex.o", &hmex_o, &hmex_o_size))
            exit(1); // TODO better error - hmex.o is an implementation detail
        Elf32_Ehdr *header = (Elf32_Ehdr*)hmex_o;
        
        // ensure elf invariants
        expect(header->e_ident[0] == 0x7F);
        expect(header->e_ident[1] == 'E');
        expect(header->e_ident[2] == 'L');
        expect(header->e_ident[3] == 'F');
        expect(bswap_16(header->e_machine) == EM_PPC);
        expect(bswap_16(header->e_type) == ET_REL); 
        
        uint32_t shoff = bswap_32(header->e_shoff);         // section header table offset
        expect(shoff != 0);
         
        uint32_t shentsize = bswap_16(header->e_shentsize); // section header table entry size 
        uint32_t shnum     = bswap_16(header->e_shnum);     // section header table entry count
        uint32_t shstrndx  = bswap_16(header->e_shstrndx);  // section header table string section index   
        expect(shstrndx != 0);
        
        Elf32_Shdr *str_section = (Elf32_Shdr*)&hmex_o[shoff + shstrndx * shentsize]; 
        uint8_t *mex_o_symbols = &hmex_o[bswap_32(str_section->sh_offset)]; 
        
        for (uint32_t shdr_i = 0; shdr_i < shnum; ++shdr_i) {
            Elf32_Shdr *shdr = (Elf32_Shdr*)&hmex_o[shoff + shdr_i * shentsize];
            uint64_t type = bswap_32(shdr->sh_type); 
            uint32_t elf_offset = bswap_32(shdr->sh_offset); 
            
            // MexTK kinda uses kinda ignores this field, so we ensure it doesn't exist. 
            expect(type != SHT_REL);
            
            if (type == SHT_RELA) {
                // TODO
            } else if (type == SHT_PROGBITS) {
                // find sections matching symbols
                
                uint8_t *section_name = &mex_o_symbols[bswap_32(shdr->sh_name)]; 
                
                for (uint32_t symbol_i = 0; ; symbol_i++) {
                    uint8_t *export_symbol = symbol_table[symbol_i];
                    if (export_symbol == NULL) break;
                    
                    if (match_section_name(export_symbol, section_name))
                        fn_table[fn_count++] = (MEXSymbol) { symbol_i, elf_offset };
                }
            }
        }
    }
      
    // write dat file
    {
        // alloc and write code.
        // For now we just copy the entire elf into the dat file.
        DatRef code_offset;
        uint32_t code_size = (uint32_t)hmex_o_size;
        dat_expect(dat_obj_alloc(&dat, code_size, &code_offset));
        memcpy(&dat.data[code_offset], hmex_o, code_size);
        
        // TODO relocation
        
        // alloc and write fn pointer table
        DatRef fn_table_offset;
        uint32_t fn_table_count = (uint32_t)fn_count;
        uint32_t fn_table_size = (uint32_t)(sizeof(MEXSymbol) * fn_table_count);
        dat_expect(dat_obj_alloc(&dat, fn_table_size, &fn_table_offset));
        for (uint64_t i = 0; i < fn_count; ++i) {
            MEXSymbol *sym = &fn_table[i];
            DatRef entry_offset = fn_table_offset + (uint32_t)(sizeof(MEXSymbol)*i); 
            dat_expect(dat_obj_write_u32(&dat, entry_offset + 0, sym->symbol_idx));
            
            // Right now, the elf offset is the same as the code offset, because
            // we simply copy the entire elf in to the dat file.
            // This may change in the future.
            dat_expect(dat_obj_write_u32(&dat, entry_offset + 4, sym->elf_offset));
        }
        
        // alloc and write MEXFunction
        // https://github.com/akaneia/m-ex/blob/5661a833833f530389ba24cdbf9bd8a89d3d7c36/MexTK/include/mxdt.h#L295
        DatRef fn_obj;
        dat_expect(dat_obj_alloc(&dat, 0x20, &fn_obj));
        dat_expect(dat_root_add(&dat, dat.root_count, fn_obj, args.symbol_name));
        
        dat_expect(dat_obj_set_ref  (&dat, fn_obj + 0x00, code_offset));
        //dat_expect(dat_obj_set_ref  (&dat, fn_obj + 0x04, reloc_table_offset));
        //dat_expect(dat_obj_write_u32(&dat, fn_obj + 0x08, reloc_table_count));
        dat_expect(dat_obj_set_ref  (&dat, fn_obj + 0x0C, fn_table_offset));
        dat_expect(dat_obj_write_u32(&dat, fn_obj + 0x10, fn_table_count));
        dat_expect(dat_obj_write_u32(&dat, fn_obj + 0x14, code_size)); // (unused???)
        dat_expect(dat_obj_write_u32(&dat, fn_obj + 0x18, 0)); // debug symbol num
        dat_expect(dat_obj_write_u32(&dat, fn_obj + 0x1C, 0)); // debug symbol ptr
    }
    
    // export!
    {
        uint32_t dat_max_size = dat_file_export_max_size(&dat);
        uint8_t *outbuf = malloc(dat_max_size);
        uint32_t dat_size;
        dat_expect(dat_file_export(&dat, outbuf, &dat_size));
        
        if (write_file(args.output_dat_path, outbuf, dat_size))
            exit(1);
        printf("wrote %s\n", args.output_dat_path);
    }
    
    return 0;
}
