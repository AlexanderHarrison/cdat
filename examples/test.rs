use cdat::*;

fn main() {
    let f = std::fs::read("lab.dat").unwrap();
    let dat = DatFile::import(f.as_slice()).unwrap();

    let out = dat.export().unwrap();
    std::fs::write("lab.dat.out", out).unwrap();
}
