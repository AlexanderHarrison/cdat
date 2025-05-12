#include "utils.h"

#include "dat.h"
#include "dat.c"

#define MAX_CMD_LEN 8192
#define DEFAULT_GCC_FLAGS "-O2 -Wall -Wextra"

static const char *HELP = "\
USAGE:\n\
    hmex [flags]\n\
\n\
REQUIRED FLAGS:\n\
    -i <file.c file2.o ...>     : Input filepaths.\n\
    -l <melee.link>             : File containing melee symbol addresses.\n\
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

// ARGS --------------------------------------------------------------

enum ArgFlags {
    Arg_Help        = (1ul << 0),
};

typedef struct Args {
    // env vars
    const char *devkitppc_path;     // DEVKITPPC

    // required arguments
    const char **input_filepaths;   // -i
    uint32_t input_filepaths_count;
    const char *symbol_table_path;  // -t
    const char *output_dat_path;    // -o

    // optional arguments
    const char *input_dat_path;   // -dat
    const char *link_table_path;  // -l
    const char *gcc_flags;        // -f
    const char *symbol_name;      // -s

    uint64_t flags;
} Args;

static Args args;
static NoArgFlag no_arg_flags[] = {
    { "-h", Arg_Help },
};
static SingleArgFlag single_arg_flags[] = {
    { "-l", &args.link_table_path },
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
        fprintf(stderr, ERROR_STR "$DEVKITPPC environment variable is not set! \
Please install devkitpro and the PPC/Gamecube package, \
and ensure the DEVKITPPC environment variable is set.\n");
        print_usage = true;
        err = true;
    } else {
        err |= check_path_access(args.devkitppc_path, R_OK);
    }

    if (args.input_filepaths_count == 0) {
        fprintf(stderr, ERROR_STR "No input files passed! Use '-i' to pass input files.\n");
        print_usage = true;
        err = true;
    } else {
        for (uint32_t i = 0; i < args.input_filepaths_count; ++i) {
            const char *ip_path = args.input_filepaths[i];
            err |= check_path_access(ip_path, R_OK);
        }
    }

    if (args.symbol_table_path == NULL) {
        fprintf(stderr, ERROR_STR "No symbol table passed! Use '-t' to pass a symbol table path.\n");
        print_usage = true;
        err = true;
    } else {
        err |= check_path_access(args.symbol_table_path, R_OK);
    }

    if (args.output_dat_path == NULL) {
        fprintf(stderr, ERROR_STR "No output dat path passed! Use '-o' to pass an output dat path.\n");
        print_usage = true;
        err = true;
    }

    // optional arguments

    if (args.input_dat_path)
        err |= check_path_access(args.input_dat_path, R_OK);

    if (args.link_table_path)
        err |= check_path_access(args.link_table_path, R_OK);

    if (args.gcc_flags == NULL)
        args.gcc_flags = DEFAULT_GCC_FLAGS;
    printf("args %s\n", args.gcc_flags);

    if (args.symbol_name == NULL && args.symbol_table_path != NULL)
        args.symbol_name = inner_name(args.symbol_table_path);

    if (print_usage)
        fprintf(stderr, "\n%s", HELP);

    if (err)
        exit(1);
}

// STRUCTS --------------------------------------------------------------

typedef struct MEXReloc {
    // cmd in high byte, code_offset in low 3 bytes.
    uint32_t cmd_and_code_offset;

    // - If this is a melee symbol, this is the address of the symbol.
    // - If this is an internal symbol, this is the offset of the symbol from the start of the data.
    // For now, it means it is the elf offset.
    //
    // I guess if location > 0x80000000 then it's handled differently within mex?
    uint32_t location;
} MEXReloc;

typedef struct MEXSymbol {
    uint32_t symbol_idx;
    uint32_t elf_offset;
} MEXSymbol;

// entry in melee.link
typedef struct LinkEntry {
    uint8_t *symbol;
    uint32_t address;
} LinkEntry;

// ELF STUFF ---------------------------------------------------------
// copied here for use on non-unix systems where elf.h isn't available.

#define EM_PPC		20	/* PowerPC */
#define ET_REL		1		/* Relocatable file */
#define STB_GLOBAL	1		/* Global symbol */
#define SHN_UNDEF	0		/* Undefined section */
#define SHN_ABS		0xfff1		/* Associated symbol is absolute */
#define SHN_COMMON	0xfff2		/* Associated symbol is common */
#define SHN_UNDEF	0		/* Undefined section */
#define SHT_RELA	  4		/* Relocation entries with addends */
#define SHT_REL		  9		/* Relocation entries, no addends */

typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Addr;
typedef uint32_t Elf32_Off;
typedef uint16_t Elf32_Section;

#define EI_NIDENT (16)
typedef struct {
  unsigned char e_ident[EI_NIDENT]; /* Magic number and other info */
  Elf32_Half e_type;                /* Object file type */
  Elf32_Half e_machine;             /* Architecture */
  Elf32_Word e_version;             /* Object file version */
  Elf32_Addr e_entry;               /* Entry point virtual address */
  Elf32_Off e_phoff;                /* Program header table file offset */
  Elf32_Off e_shoff;                /* Section header table file offset */
  Elf32_Word e_flags;               /* Processor-specific flags */
  Elf32_Half e_ehsize;              /* ELF header size in bytes */
  Elf32_Half e_phentsize;           /* Program header table entry size */
  Elf32_Half e_phnum;               /* Program header table entry count */
  Elf32_Half e_shentsize;           /* Section header table entry size */
  Elf32_Half e_shnum;               /* Section header table entry count */
  Elf32_Half e_shstrndx;            /* Section header string table index */
} Elf32_Ehdr;

typedef struct {
  Elf32_Word sh_name;               /* Section name (string tbl index) */
  Elf32_Word sh_type;               /* Section type */
  Elf32_Word sh_flags;              /* Section flags */
  Elf32_Addr sh_addr;               /* Section virtual addr at execution */
  Elf32_Off sh_offset;              /* Section file offset */
  Elf32_Word sh_size;               /* Section size in bytes */
  Elf32_Word sh_link;               /* Link to another section */
  Elf32_Word sh_info;               /* Additional section information */
  Elf32_Word sh_addralign;          /* Section alignment */
  Elf32_Word sh_entsize;            /* Entry size if section holds table */
} Elf32_Shdr;

typedef struct {
  Elf32_Word st_name;               /* Symbol name (string tbl index) */
  Elf32_Addr st_value;              /* Symbol value */
  Elf32_Word st_size;               /* Symbol size */
  unsigned char st_info;            /* Symbol type and binding */
  unsigned char st_other;           /* Symbol visibility */
  Elf32_Section st_shndx;           /* Section index */
} Elf32_Sym;

#define ELF32_ST_BIND(val)  (((unsigned char) (val)) >> 4)
#define ELF32_ST_TYPE(val)  ((val) & 0xf)

typedef struct {
  Elf32_Addr r_offset;              /* Address */
  Elf32_Word r_info;                /* Relocation type and symbol index */
  Elf32_Sword r_addend;             /* Addend */
} Elf32_Rela;

#define ELF32_R_SYM(val)  ((val) >> 8)
#define ELF32_R_TYPE(val)  ((val) & 0xff)

// FNS ---------------------------------------------------------------

// copies and quotes arg as a single argument to cmd, returning new curcmd
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

// copies every argument in arg to cmd, returning new curcmd
char *copy_args(char *cmd, char *curcmd, const char *arg) {
    (void)cmd; // TODO - use for bounds checking
    // ensure NOT quoted - we want these to expand to multiple separate arguments,
    // for e.x. gcc flags.
    curcmd = my_stpcpy(curcmd, arg);
    curcmd = my_stpcpy(curcmd, " ");
    return curcmd;
}

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
                    ERROR_STR "Could not import dat file '%s'. File is not a dat file or is malformed.\n",
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

    // parse melee link table file
    LinkEntry *link_table = NULL;
    uint32_t link_table_entry_count = 0;
    uint8_t *link_table_symbols = NULL;
    if (args.link_table_path) {
        uint8_t *lt;
        uint64_t lt_size;
        if (read_file(args.link_table_path, &lt, &lt_size))
            exit(1);

        uint32_t link_table_entry_count_max = 0;
        for (uint64_t i = 0; i < lt_size; ++i) {
            if (lt[i] == '\n')
                link_table_entry_count_max++;
        }
        link_table_entry_count_max++; // add room for last line, which may not be null terminated

        link_table = malloc(sizeof(LinkEntry) * link_table_entry_count_max);
        link_table_symbols = malloc(lt_size);
        uint8_t *link_table_symbols_head = link_table_symbols;

        // parse lines
        uint64_t i = 0;
        uint64_t line = 1;
        link_table_entry_count = 0;
        bool err = false;
        for (; i < lt_size; ++i) {
            // parse address
            uint32_t addr = 0;
            for (; i < lt_size; ++i) {
                uint8_t c = lt[i];
                if ('0' <= c && c <= '9') {
                    addr <<= 4;
                    addr += (uint32_t)(c - '0');
                } else if ('a' <= c && c <= 'f') {
                    addr <<= 4;
                    addr += (uint32_t)(c - 'a' + 10);
                } else if ('A' <= c && c <= 'F') {
                    addr <<= 4;
                    addr += (uint32_t)(c - 'A' + 10);
                } else if (c == ':') {
                    ++i;
                    break;
                } else {
                    err = true;
                    break;
                }
            }

            // parse symbol
            uint8_t *symbol = link_table_symbols_head;
            uint32_t symbol_len = 0;
            for (; i < lt_size; ++i) {
                uint8_t c = lt[i];

                // \r\n
                if (c == '\r') {
                    ++i;
                    break;
                }

                // \n
                if (c == '\n')
                    break;

                symbol_len++;
                *(link_table_symbols_head++) = c;
            }
            *(link_table_symbols_head++) = 0;

            if (symbol_len == 0) err = true;
            if (addr < 0x80000000) err = true;

            if (err) {
                fprintf(
                    stderr,
                    WARNING_STR "%s:%llu Malformed entry in melee link table\n",
                    args.link_table_path,
                    line
                );
            } else {
                link_table[link_table_entry_count++] = (LinkEntry) { symbol, addr };
            }

            line++;
        }

        free(lt);
    }

    // compile c files
    char *cmd = malloc(MAX_CMD_LEN);
    {
        char *curcmd = cmd;

        curcmd = copy_arg (cmd, curcmd, gcc_path);
        curcmd = copy_args(cmd, curcmd, "-DGEKKO -mogc -mcpu=750 -meabi -mhard-float -fno-asynchronous-unwind-tables -c -o hmex.o");
        //curcmd = copy_args(cmd, curcmd, "-T");
        //curcmd = copy_arg (cmd, curcmd, args.linker_script_path);
        curcmd = copy_args(cmd, curcmd, args.gcc_flags);

        for (uint32_t i = 0; i < args.input_filepaths_count; ++i) {
            const char *ip_path = args.input_filepaths[i];
            curcmd = copy_arg(cmd, curcmd, ip_path);
        }

        printf("executing: %s\n", cmd);
        int ret = system(cmd);
        if (ret != 0) {
            fprintf(stderr, ERROR_STR "compilation failed\n");
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
        uint8_t *hmex_o_symbols = &hmex_o[bswap_32(str_section->sh_offset)];

        // find table sections through section header table
        uint8_t *symtab = NULL;
        uint8_t *strtab = NULL;
        uint32_t symtab_count = 0;
        uint32_t symtab_entsize = 0;
        for (uint32_t shdr_i = 0; shdr_i < shnum; ++shdr_i) {
            Elf32_Shdr *shdr = (Elf32_Shdr*)&hmex_o[shoff + shdr_i * shentsize];

            uint8_t *section_name = &hmex_o_symbols[bswap_32(shdr->sh_name)];
            if (strcmp((char*)section_name, ".symtab") == 0) {
                expect(symtab == NULL); // ELFs should only contain one symbol table section
                symtab = &hmex_o[bswap_32(shdr->sh_offset)];
                symtab_entsize = bswap_32(shdr->sh_entsize);
                symtab_count = bswap_32(shdr->sh_size) / symtab_entsize;
            }

            if (strcmp((char*)section_name, ".strtab") == 0) {
                expect(strtab == NULL); // ELFs should only contain one string table section
                strtab = &hmex_o[bswap_32(shdr->sh_offset)];
            }
        }

        expect(symtab);
        expect(strtab);

        // match internal symbols with mex symbols in passed symbol table file
        for (uint32_t symtab_i = 1; symtab_i < symtab_count; ++symtab_i) {
            Elf32_Sym *sym = (Elf32_Sym*)(symtab + symtab_i * symtab_entsize);
            uint8_t *sym_name = &strtab[bswap_32(sym->st_name)];
            uint32_t shndx = bswap_16(sym->st_shndx);
            uint8_t info = sym->st_info;
            if (ELF32_ST_BIND(info) != STB_GLOBAL) continue;

            for (uint32_t symbol_i = 0; ; symbol_i++) {
                uint8_t *export_symbol = symbol_table[symbol_i];
                if (export_symbol == NULL) break;

                if (strcmp((char*)sym_name, (char*)export_symbol) == 0) {
                    if (shndx == SHN_COMMON)
                        printf("IDK WHAT TO DO WITH THIS %s\n", sym_name);
                    if (shndx == SHN_ABS)
                        printf("IDK WHAT TO DO WITH THIS %s\n", sym_name);

                    Elf32_Shdr *shdr = (Elf32_Shdr*)&hmex_o[shoff + shndx * shentsize];
                    uint32_t elf_offset = bswap_32(shdr->sh_offset) + bswap_32(sym->st_value);

                    fn_table[fn_count++] = (MEXSymbol) { symbol_i, elf_offset };
                    break;
                }
            }
        }

        // print matched symbols
        for (uint32_t symbol_i = 0; ; symbol_i++) {
            uint8_t *symbol = symbol_table[symbol_i];
            if (symbol == NULL) break;

            bool matched = false;
            for (uint32_t fn_i = 0; fn_i < fn_count; ++fn_i) {
                if (fn_table[fn_i].symbol_idx == symbol_i)
                    matched = true;
            }

            if (matched) {
                printf(GREEN_CODE "O | %s" RESET_CODE "\n", symbol);
            } else {
                printf("X | %s\n", symbol);
            }
        }

        // get relocation for each section
        bool link_err = false;
        for (uint32_t shdr_i = 0; shdr_i < shnum; ++shdr_i) {
            Elf32_Shdr *shdr = (Elf32_Shdr*)&hmex_o[shoff + shdr_i * shentsize];

            uint64_t type = bswap_32(shdr->sh_type);
            if (type != SHT_RELA && type != SHT_REL) continue;

            // find the location of the section where the relocation takes place
            uint32_t src_shdr_i = bswap_32(shdr->sh_info);
            Elf32_Shdr *src_shdr = (Elf32_Shdr*)&hmex_o[shoff + src_shdr_i * shentsize];
            uint32_t rel_src = bswap_32(src_shdr->sh_offset);

            uint32_t rel_entsize = bswap_32(shdr->sh_entsize);
            uint32_t rel_count = bswap_32(shdr->sh_size) / rel_entsize;
            uint8_t *rel_table = &hmex_o[bswap_32(shdr->sh_offset)];

            for (uint32_t rel_i = 0; rel_i < rel_count; ++rel_i) {
                Elf32_Rela* rel = (Elf32_Rela*)(rel_table + rel_i * rel_entsize);
                uint32_t rel_symtab_i = ELF32_R_SYM(bswap_32(rel->r_info));
                uint32_t rel_type = ELF32_R_TYPE(bswap_32(rel->r_info));
                uint32_t rel_offset = bswap_32(rel->r_offset);

                Elf32_Sym *sym = (Elf32_Sym*)(symtab + rel_symtab_i * symtab_entsize);
                uint32_t shndx = bswap_16(sym->st_shndx);
                uint8_t *sym_name = &strtab[bswap_32(sym->st_name)];

                // where the relocation points to. Either ram address or elf offset.
                uint32_t location = 0;

                if (shndx == SHN_UNDEF) {
                    // Not defined in elf file, so this is an internal melee symbol.
                    // Location is ram address.

                    // Look in link table file for symbol address in ram.
                    if (link_table != NULL) {
                        // TODO use hashmap instead of linear lookup
                        for (uint32_t link_i = 0; link_i < link_table_entry_count; ++link_i) {
                            LinkEntry *link_entry = &link_table[link_i];
                            if (strcmp((char*)sym_name, (char*)link_entry->symbol) == 0) {
                                location = link_entry->address;
                                break;
                            }
                        }
                    }

                    if (location == 0) {
                        fprintf(stderr, ERROR_STR "Undefined symbol: %s\n", sym_name);
                        link_err = true;
                    }
                } else {
                    // Is defined in elf file. location is offset from start of elf.
                    if (shndx == SHN_COMMON)
                        printf("IDK WHAT TO DO WITH THIS %s\n", sym_name);
                    if (shndx == SHN_ABS)
                        printf("IDK WHAT TO DO WITH THIS %s\n", sym_name);

                    Elf32_Shdr *target_shdr = (Elf32_Shdr*)&hmex_o[shoff + shndx * shentsize];
                    location = bswap_32(target_shdr->sh_offset) + bswap_32(sym->st_value);

                    if (type == SHT_RELA) {
                        int32_t offset = (int32_t)bswap_32((uint32_t)rel->r_addend);
                        location = (uint32_t)((int32_t)location + offset); // IDK IF THIS IS RIGHT!!!
                    }
                }

                reloc[reloc_count++] = (MEXReloc) {
                    .cmd_and_code_offset = (rel_type << 24) | (rel_src + rel_offset),
                    .location = location,
                };
            }
        }

        if (link_err) {
            if (args.link_table_path) {
                fprintf(
                    stderr,
                    "Implement or add the above symbols to to your link file in %s.\n",
                    args.link_table_path
                );
            } else {
                fprintf(
                    stderr,
                    "Pass a link table file (usually 'melee.link') with the -l flag.\n"
                );
            }
            exit(1);
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

        // alloc and write relocation table.
        DatRef reloc_table_offset;
        uint32_t reloc_table_count = (uint32_t)reloc_count;
        uint32_t reloc_table_size = (uint32_t)(sizeof(MEXReloc) * reloc_count);
        dat_expect(dat_obj_alloc(&dat, reloc_table_size, &reloc_table_offset));
        for (uint64_t i = 0; i < reloc_count; ++i) {
            MEXReloc *reloc_entry = &reloc[i];
            DatRef entry_offset = reloc_table_offset + (uint32_t)(sizeof(MEXReloc)*i);
            dat_expect(dat_obj_write_u32(&dat, entry_offset + 0, reloc_entry->cmd_and_code_offset));
            dat_expect(dat_obj_write_u32(&dat, entry_offset + 4, reloc_entry->location));
        }

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
            // This will change in the future.
            dat_expect(dat_obj_write_u32(&dat, entry_offset + 4, sym->elf_offset));
        }

        // alloc and write MEXFunction
        // https://github.com/akaneia/m-ex/blob/5661a833833f530389ba24cdbf9bd8a89d3d7c36/MexTK/include/mxdt.h#L295
        DatRef fn_obj;
        dat_expect(dat_obj_alloc(&dat, 0x20, &fn_obj));
        dat_expect(dat_root_add(&dat, dat.root_count, fn_obj, args.symbol_name));

        dat_expect(dat_obj_set_ref  (&dat, fn_obj + 0x00, code_offset));
        dat_expect(dat_obj_set_ref  (&dat, fn_obj + 0x04, reloc_table_offset));
        dat_expect(dat_obj_write_u32(&dat, fn_obj + 0x08, reloc_table_count));
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
