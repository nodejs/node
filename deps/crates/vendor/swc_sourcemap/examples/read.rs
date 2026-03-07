use std::env;
use std::fs;
use std::io::Read;

use swc_sourcemap::{decode, DecodedMap, RewriteOptions, SourceMap};

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

    let line = if args.len() > 2 {
        args[2].parse::<u32>().unwrap()
    } else {
        0
    };
    let column = if args.len() > 3 {
        args[3].parse::<u32>().unwrap()
    } else {
        0
    };

    let token = sm.lookup_token(line, column).unwrap(); // line-number and column
    println!("token: {token}");
}
