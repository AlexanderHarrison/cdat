# What's in this repo?

- **cdat** (src/dat.c src/dat.h): a simple, portable library for reading, modifying, and saving dat files.
- **hmex** (src/hmex.c): a fast and portable reimplementation of MexTK.
Currently mostly working, but needs more testing.
Function patching (a useful but undocumented feature of m-ex) is not yet implemented.



## CDat

CDat is a small library for reading, modifying, and saving dat files.
This is the file format used to store assets in SSBM.
They contain models, fighter physics data, animations, effects, textures.
Notably, dat files do not contain code in SSBM - however, the m-ex dat format does.



## Hmex

Hmex and MexTK at their core convert elf files to a custom executable format used by the m-ex framework.
This custom format is contained in dat files, the general object format used by SSBM.
They also provide a convenient way to compile c code into elf files and then convert those to dat files,
so in practice hmex and mextk are c code to dat executable converters.

To explain the m-ex format, you need to understand the dat format.


### Dat files
Dat files contain four sections.

**Please note that everything in a dat file is big endian!**

#### **Header**:
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

#### **Data Section**
The data section contains the bulk of a dat file.
It is located right after the 32 byte header.
This section contains "objects" which can contain data and pointers to other objects.
Every pointer to another object will contain an entry in the relocation table.

Note that objects' sizes are not stored in the dat file.
You can mostly reconstruct the size of the objects by using the relocation
table to see where an object ends and another begins, but this is not guaranteed
to be correct.

#### **Relocation Table**
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

#### **Root Table**
This table contains names and pointers for important objects in the data section.

This section is located right after the relocation table.
Each entry is two u32 words.

```c
struct DatRootEntry {
    u32 data_offset;    // where this root's object is located in the data section.
    u32 symbol_offset;  // where this root's name is located in the symbol table section.
};
```

#### **External Reference Table**
This section is very similar to the root table section,
and is placed immediately after it.
The entry format is exactly the same as the root table.

This section isn't used much and I don't know what purpose it serves.
I think these entries can be linked to other dat files somehow?
I believe pokemon stadium uses it for transformations.

#### **Symbol Table**
Contains null-terminated strings for the root and external ref tables.
Lasts from the end of the external ref table until the end of the file.


### m-ex Executable Format




## [HSDRaw](https://github.com/Ploaj/HSDLib/) vs cdat
HSDRaw and cdat serve different purposes.
HSDRaw is specifically tuned for melee's dat files and their object types.
CDat does not know or care about the object types contained inside dat files.
Another HSDRaw could be built on top of cdat, using cdat to read and modify dat files.

For example, if you want to modify some fighter attribute such as air speed,
then HSDRaw provides a super simple way to do this - just open up the character dat and set the air speed value.
In cdat, you will need to know exactly the root object and chain of 
references to get to the attribute you want to modify - it does not provide the path to the air speed value. 
