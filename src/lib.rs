pub type DatOffset = u32;
pub type SymbolOffset = u32;

#[derive(Copy, Clone, PartialEq, Debug)]
pub struct Root {
    pub obj_offset: DatOffset,
    pub symbol_offset: SymbolOffset,
}

#[derive(Copy, Clone, PartialEq, Debug)]
pub struct Ref {
    pub obj_offset: DatOffset,
    pub symbol_offset: SymbolOffset,
}

#[derive(Clone, PartialEq, Debug)]
pub struct DatFile {
    pub data: Vec<u8>,
    pub relocation: Vec<u64>,
    pub roots: Vec<Root>,
    pub extern_refs: Vec<Ref>,
    pub symbols: Vec<u8>,
}

#[derive(Copy, Clone, PartialEq, Debug)]
pub enum DatError {
    InvalidFile,
}

pub type DatResult<T> = Result<T, DatError>;

#[inline(always)]
fn read_u32(bytes: &[u8], offset: u32) -> u32 { 
    u32::from_be_bytes(bytes[offset as usize..][..4].try_into().unwrap()) 
}

#[inline(always)]
fn push_u32(bytes: &mut Vec<u8>, n: u32) { 
    bytes.extend_from_slice(&n.to_be_bytes());
}

#[inline(always)]
fn write_u32(bytes: &mut [u8], offset: u32, data: u32) { 
    bytes[offset as usize..][..4].copy_from_slice(&data.to_be_bytes());
}

#[inline(always)]
fn read_u16(bytes: &[u8], offset: u32) -> u16 { 
    u16::from_be_bytes(bytes[offset as usize..][..2].try_into().unwrap()) 
}

#[inline(always)]
fn write_u16(bytes: &mut [u8], offset: u32, data: u16) { 
    bytes[offset as usize..][..2].copy_from_slice(&data.to_be_bytes());
}

impl DatFile {
    pub fn import(dat_file: &[u8]) -> DatResult<Self> {
        if dat_file.len() < 0x20 { return Err(DatError::InvalidFile); }

        // header ----------

        let file_size    = read_u32(dat_file, 0);
        let data_size    = read_u32(dat_file, 4);
        let reloc_count  = read_u32(dat_file, 8);
        let root_count   = read_u32(dat_file, 12);
        let ref_count    = read_u32(dat_file, 16);

        if file_size as usize != dat_file.len() { return Err(DatError::InvalidFile); }

        // data  ---------------------

        let data_cap = data_size.max(0x40000000);
        let mut data = Vec::with_capacity(data_cap as usize);
        data.extend_from_slice(&dat_file[0x20..][..data_size as usize]);

        // relocation table ----------

        // one bit per 4 byte word in data
        let mut relocation = vec![0u64; (data_size as usize).div_ceil(64 * 4)];
        let reloc_offset = 0x20 + data_size;
        let reloc_size = reloc_count*4;
        if file_size < reloc_offset + reloc_size { return Err(DatError::InvalidFile); }
        for i in 0..reloc_count {
            let offset = read_u32(dat_file, reloc_offset + i*4);
            let word_idx = offset / 4;
            let reloc_u64 = word_idx / 64;
            let bit_idx = word_idx % 64;
            relocation[reloc_u64 as usize] |= 1 << bit_idx;
        }

        // root table ----------

        let mut roots = Vec::with_capacity(root_count as usize * 4);
        let root_offset = reloc_offset + reloc_size;
        let root_size = root_count * 8;
        if file_size < root_offset + root_size { return Err(DatError::InvalidFile); }
        for i in 0..root_count {
            roots.push(Root {
                obj_offset: read_u32(dat_file, root_offset + i*8),
                symbol_offset: read_u32(dat_file, root_offset+ i*8 + 4),
            });
        }

        // external ref table ----------

        let mut extern_refs = Vec::with_capacity(ref_count as usize * 4);
        let ref_offset = root_offset + root_size;
        let ref_size = ref_count * 8;
        if file_size < ref_offset + ref_size { return Err(DatError::InvalidFile); }
        for i in 0..ref_count {
            extern_refs.push(Ref {
                obj_offset: read_u32(dat_file, ref_offset + i*8),
                symbol_offset: read_u32(dat_file, ref_offset+ i*8 + 4),
            });
        }

        // symbol table -----------------

        let symbol_offset = ref_offset + ref_size;
        if symbol_offset > file_size { return Err(DatError::InvalidFile); }
        let symbol_size = file_size - symbol_offset;
        let mut symbols = Vec::with_capacity(symbol_size as usize * 2);
        symbols.extend_from_slice(&dat_file[symbol_offset as usize..][..symbol_size as usize]);

        Ok(DatFile { data, relocation, roots, extern_refs, symbols })
    }

    fn count_relocation_entries(&self) -> u32 {
        let mut count = 0;
        for reloc_mask in self.relocation.iter().copied() {
            count += reloc_mask.count_ones();
        }
        count
    }

    pub fn export(&self) -> DatResult<Vec<u8>> {
        let relocation_entries = self.count_relocation_entries();
        let mut file_size = 0x20;
        file_size += self.data.len();
        file_size += (relocation_entries as usize) * 4;
        file_size += self.roots.len() * 8;
        file_size += self.extern_refs.len() * 8;
        file_size += self.symbols.len();

        // header -------------------

        let mut file = Vec::with_capacity(file_size);

        push_u32(&mut file, file_size as u32);
        push_u32(&mut file, self.data.len() as u32);
        push_u32(&mut file, relocation_entries);
        push_u32(&mut file, self.roots.len() as u32);
        push_u32(&mut file, self.extern_refs.len() as u32);

        push_u32(&mut file, 0u32); // version (hsdraw zeroes this for some reason)
        push_u32(&mut file, 0u32); // padding
        push_u32(&mut file, 0u32); // padding

        // data --------------------

        file.extend_from_slice(self.data.as_slice());

        // relocation --------------------

        for i in 0..(self.relocation.len() as u32) {
            let reloc_mask = self.relocation[i as usize];

            for b in 0..64 {
                if (1 << b) & reloc_mask != 0 {
                    let target = (i*64 + b)*4;
                    push_u32(&mut file, target);
                }
            }
        }

        // root table ------------------

        for r in self.roots.iter() {
            push_u32(&mut file, r.obj_offset);
            push_u32(&mut file, r.symbol_offset);
        }

        // external ref table ------------------

        for r in self.extern_refs.iter() {
            push_u32(&mut file, r.obj_offset);
            push_u32(&mut file, r.symbol_offset);
        }

        // symbols ------------------

        file.extend_from_slice(self.symbols.as_slice());

        Ok(file)
    }

    pub fn new() -> Self {
        DatFile {
            data: Vec::new(),
            relocation: Vec::new(),
            roots: Vec::new(),
            extern_refs: Vec::new(),
            symbols: Vec::new(),
        }
    }

    pub fn object_alloc(&mut self, size: u32) -> DatOffset {
        if self.data.len() & 3 != 0 {
            let aligned_len = (self.data.len() & 3) + 4;
            self.data.resize(aligned_len, 0);
        }

        let offset = self.data.len() as u32;
        self.data.resize(self.data.len() + size as usize, 0);
        offset
    }

    /// Panics if `at` is not 4 byte aligned.
    pub fn ref_set(&mut self, at: DatOffset, to: DatOffset) {
        assert!(at % 4 == 0);

        write_u32(&mut self.data, at, to);

        let word_idx = at / 4;
        let reloc_idx = word_idx as usize / 64;
        let bit_idx = word_idx % 64;
        if self.relocation.len() <= reloc_idx {
            self.relocation.resize(reloc_idx+1, 0u64);
        }

        self.relocation[reloc_idx] |= 1 << bit_idx;
    }

    /// Panics if `at` is not 4 byte aligned.
    pub fn ref_remove(&mut self, at: DatOffset) {
        assert!(at % 4 == 0);

        let word_idx = at / 4;
        let reloc_idx = word_idx as usize / 64;
        let bit_idx = word_idx % 64;
        if self.relocation.len() <= reloc_idx {
            self.relocation.resize(reloc_idx+1, 0u64);
        }

        let prev_mask = self.relocation[reloc_idx];
        self.relocation[reloc_idx] = prev_mask & !(1 << bit_idx);
    }

    /// Panics if `at` is not 4 byte aligned.
    pub fn ref_check(&self, at: DatOffset) -> bool {
        assert!(at % 4 == 0);

        let word_idx = at / 4;
        let reloc_idx = word_idx / 64;
        let bit_idx = word_idx % 64;

        if self.relocation.len() as u32 <= reloc_idx { return false; }
        self.relocation[reloc_idx as usize] & (1 << bit_idx) != 0
    }

    /// Panics if `symbol` is not ascii.
    pub fn symbol_add(&mut self, symbol: &str) -> SymbolOffset {
        assert!(symbol.is_ascii());
        let offset = self.symbols.len() as u32;
        self.symbols.extend_from_slice(symbol.as_bytes());
        if symbol.as_bytes().last().copied() != Some(0) { self.symbols.push(0); }
        offset
    }

    /// Panics if `at >= symbols.len()`, or if the symbol is not null terminated.
    ///
    /// returns None if the symbol is not ascii.
    pub fn symbol_read(&mut self, at: SymbolOffset) -> Option<&str> {
        let rest = &self.symbols[at as usize..];
        let null = rest.iter().position(|b| *b == 0).unwrap();
        let bytes = &rest[..null];
        let s = std::str::from_utf8(bytes).ok()?;
        if s.is_ascii() { Some(s) } else { None }
    }
}

impl DatFile {
    /// Panics if `at` is not 4 byte aligned.
    pub fn read_u32(&mut self, at: DatOffset) -> u32 {
        assert!(at % 4 == 0);
        read_u32(self.data.as_slice(), at)
    }

    /// Panics if `at` is not 4 byte aligned.
    pub fn write_u32(&mut self, at: DatOffset, data: u32) {
        assert!(at % 4 == 0);
        write_u32(self.data.as_mut_slice(), at, data);
    }

    /// Panics if `at` is not 2 byte aligned.
    pub fn read_u16(&mut self, at: DatOffset) -> u16 {
        assert!(at % 2 == 0);
        read_u16(self.data.as_slice(), at)
    }

    /// Panics if `at` is not 2 byte aligned.
    pub fn write_u16(&mut self, at: DatOffset, data: u16) {
        assert!(at % 2 == 0);
        write_u16(self.data.as_mut_slice(), at, data);
    }
}
