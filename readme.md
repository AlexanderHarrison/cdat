# What's in this repo?

- **cdat** (src/dat.c src/dat.h): a simple, portable library for reading, modifying, and saving dat files.
- **hmex** (src/hmex.c): a fast, portable reimplementation of MexTK. Currently mostly working, just needs mex relocation to be implemented.

## [HSDRaw](https://github.com/Ploaj/HSDLib/) vs cdat
HSDRaw and cdat serve different purposes.
HSDRaw is specifically tuned for melee's dat files and their object types.
cdat does not care or know about the object types contained inside dat files.
Another HSDRaw could be built on top of cdat, using cdat to read and modify dat files.

For example, if you want to modify some fighter attribute such as air speed,
then HSDRaw provides a super simple way to do this - just open up the character dat and set the air speed value.
In cdat, you will need to know exactly the root object and chain of 
references to get to the attribute you want to modify - it does not provide the path to the air speed value. 
