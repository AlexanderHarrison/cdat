# What's in this repo?

- **cdat** (src/dat.c src/dat.h): a simple, portable library for reading, modifying, and saving dat files.
- **dat_mod** (src/mod.c): a small command line utility for modifying dat files.
- **hmex** (src/hmex.c): a fast and portable reimplementation of MexTK.
Function patching and debug symbols have not yet been implemented.

## CDat

CDat is a small library for reading, modifying, and saving dat files.
This is the file format used to store assets in SSBM.
They contain models, fighter physics data, animations, effects, textures.
Notably, dat files do not contain code in SSBM - however, the m-ex dat format does.

## DatMod
A very small wrapper around cdat for simple modification of dat files.
```
USAGE:
    dat_mod debug <dat file>
        Print information about a dat file.
    dat_mod extract <dat file> <root name>
        Extract a root from a dat file into its own file.
    dat_mod insert <dat file> <input dat file>
        Copy roots from one dat file into another.
```

## Hmex
A partial reimplementation MexTK.

```
USAGE:
    hmex [flags]

REQUIRED FLAGS:
    -i <file.c file2.o ...>     : Input filepaths.
    -l <melee.link>             : File containing melee symbol addresses.
    -t <symbol-table.txt>       : Symbol table.
    -o <output.dat>             : Output dat file.

OPTIONAL FLAGS:
    -h                   : Show hmex usage.
    -c                   : Compile without linking into a dat file.
    -q                   : Do not print to stdout.
    -dat <inputs.dat>    : Input dat file.
                            Is an empty dat file by default.
    -f <gcc flags>       : Flags to pass to gcc. Optimization, warnings, etc.
                            Is '-O2 -Wall -Wextra' by default.
    -s <symbol name>     : Symbol name.
                            Is the symbol table filename (excluding extension) by default.
```

Hmex and MexTK at their core convert elf files to a custom executable format used by the m-ex framework.
This custom format is contained in dat files, the general object format used by SSBM.
They also provide a convenient way to compile c code into elf files and then convert those to dat files,
so in practice hmex and mextk are c code to dat executable converters.

To explain the m-ex format, you need to understand the dat format.


# Dat files

**Please note that everything in a dat file is big endian!**

Dat files contain six sections.

#### **Section 1: Header**:
The header is always the first 32 (0x20) bytes of the file.

```c
struct DatHeader {
    u32 file_size;      // The total size of the dat file in bytes.
    u32 data_size;      // The size of the data section.
    u32 reloc_count;    // The number of relocation entries.
    u32 root_count;     // The number of root entries.
    u32 extern_count;   // The number of external reference entries.
    u32 unused[3];
};
```

#### **Section 2: Data Section**
The data section contains the bulk of a dat file.
It is located right after the 32 byte header.
This section contains "objects" which can contain data and pointers to other objects.
Every pointer to another object will contain an entry in the relocation table.

Note that objects' sizes are not stored in the dat file.
You can mostly reconstruct the size of the objects by using the relocation
table to see where an object ends and another begins, but this is not guaranteed
to be correct.

#### **Section 3: Relocation Table**
The data section contains objects that can point to each other.
When this file is placed somewhere in memory,
adding that memory address to every entry given by this table will
convert every object reference into an easily used pointer.

This table is always placed at offset `0x20 + data_size` into the dat file.
Each entry is a single u32 offset into the data section.

```c
struct DatRelocEntry {
    u32 data_offset
};
```

For example, here is how you could update every object reference when placing a dat file in memory.
Note that this does not convert endianness.
```c
DatRelocEntry *reloc_table = (DatRelocEntry*)&dat_file[0x20 + data_size];
u8 *data = &dat_file[0x20];
for (u32 i = 0; i < reloc_count; ++i)
    *(u32*)&data[reloc_table[i].data_offset] += dat_file_ptr;
```

#### **Section 4: Root Table**
This table contains names and pointers for important objects in the data section.

This section is located right after the relocation table.
Each entry is two u32 words.

```c
struct DatRootEntry {
    u32 data_offset;    // where this root's object is located in the data section.
    u32 symbol_offset;  // where this root's name is located in the symbol table section.
};
```

#### **Section 5: Reference Table**
This section is very similar to the root table section,
and is placed immediately after it.
The entry format is exactly the same as the root table.

This section isn't used much and I don't know what purpose it serves.
I think these entries can be linked to other dat files somehow?
I believe pokemon stadium uses it for transformations.

#### **Section 6: Symbol Table**
Contains null-terminated strings for the root and external ref tables.
Lasts from the end of the external ref table until the end of the file.

## m-ex Executable Format

A mex executable starts with a custom root node.
When loading the executable, the loader will look for a specific root node name.
This name is set by the '-s' flag in hmex.
For example, TM-CE looks for a root node called 'evFunction' when loading events.

This root node points to a `MEXExe` object in the dat file: 
```c
struct MEXExe {
    u8 *code;                       // 0x0
    MEXReloc *reloc_table;          // 0x4    
    u32 reloc_count;                // 0x8
    MEXFunction *fn_table;          // 0xC
    u32 fn_table_num;               // 0x10
    u32 code_size;                  // 0x14
    
    // These are unimplemented in hmex and won't be discussed.
    u32 debug_symbol_count;         // 0x18
    MEXDebugSymbol *debug_symbols;  // 0x1c
};
```

#### MEXFunction

```c
struct MEXFunction {
    u32 index;       // 0x0
    u32 code_offset; // 0x4
};
```

When compiling with hmex or MexTK, you pass a symbol file with the -t flag.
For example, TM-CE events use evFunction.txt:
```
Event_Init
Event_Update
Event_Think
Event_Menu
```

These symbols correspond to `MEXFunctions`.
For example, the `MEXFunction` with index 2 is the function `Event_Think`.
The `code_offset` field offsets into the `code` field given in `MEXExe`,
giving the symbol's location.

#### MEXReloc

```c
struct MEXReloc {
    u32 cmd_and_code_offset; // 0x0
    u32 reloc;               // 0x4
};
```

The `MEXReloc` table contains relocation information for instructions.
Instructions cannot be relocated like pointers, they are more complex,
so a mex executable has its own table.

The `cmd_and_code_offset` is two fields mushed into one, `cmd` and `code_offset`.
The highest byte is cmd, and the lower 3 bytes is the code_offset.
The `cmd` field is the [relocation type](http://www.skyfree.org/linux/references/ELF_Format.pdf#page=28)
given by the elf file.
The `code_offset` field gives the offset of the instruction to relocate.

The `reloc` field can be two different types of values.
If the instruction is linked to an internal SSBM function or address,
then this will contain a raw RAM address (> 0x80000000).
If the instruction is linked to a function or address in the dat file,
then it is the code offset to the target address.

## [HSDRaw](https://github.com/Ploaj/HSDLib/) vs cdat
While MexTK and hmex serve similar purposes, HSDRaw and cdat serve different purposes.
HSDRaw is specifically tuned for melee's dat files and their object types.
CDat does not know or care about the object types contained inside dat files.
Another HSDRaw could be built on top of cdat, using cdat to read and modify dat files.

For example, if you want to modify some fighter attribute such as air speed,
then HSDRaw provides a super simple way to do this - just open up the character dat and set the air speed value.
In cdat, you will need to know exactly the root object and chain of 
references to get to the attribute you want to modify - it does not provide the path to the air speed value. 
