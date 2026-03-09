use std::env;
use std::fs;
use std::io::Read;
use std::path::Path;

use swc_sourcemap::{decode, DecodedMap, RewriteOptions, SourceMap};

fn test(sm: &SourceMap) {
    for (src_id, source) in sm.sources().enumerate() {
        let path = Path::new(source);
        if path.is_file() {
            let mut f = fs::File::open(path).unwrap();
            let mut contents = String::new();
            if f.read_to_string(&mut contents).ok().is_none() {
                continue;
            }
            if Some(contents.as_str()) != sm.get_source_contents(src_id as u32).map(|v| &**v) {
                println!("  !!! {source}");
            }
        }
    }
}

fn load_from_reader<R: Read>(mut rdr: R) -> SourceMap {
    match decode(&mut rdr).unwrap() {
        DecodedMap::Regular(sm) => sm,
        DecodedMap::Index(idx) => idx
            .flatten_and_rewrite(&RewriteOptions {
                load_local_source_contents: true,
                ..Default::default()
            })
            .unwrap(),
        _ => panic!("unexpected sourcemap format"),
    }
}

fn main() {
    let args: Vec<_> = env::args().collect();
    let mut f = fs::File::open(&args[1]).unwrap();
    let sm = load_from_reader(&mut f);
    println!("before dump");
    test(&sm);

    println!("after dump");
    let mut json: Vec<u8> = vec![];
    sm.to_writer(&mut json).unwrap();
    let sm = load_from_reader(json.as_slice());
    test(&sm);
}
