#![deny(warnings)]

extern crate swc_malloc;

use std::{env, path::PathBuf, time::Instant};

use codspeed_criterion_compat::black_box;
use swc_common::{errors::HANDLER, GLOBALS};
use swc_ecma_parser::parse_file_as_module;
use walkdir::WalkDir;

fn main() {
    let dirs = env::args().skip(1).collect::<Vec<_>>();
    let files = expand_dirs(dirs);
    eprintln!("Using {} files", files.len());

    let start = Instant::now();
    testing::run_test2(false, |cm, handler| {
        GLOBALS.with(|globals| {
            HANDLER.set(&handler, || {
                let _ = files
                    .into_iter()
                    .map(|path| -> Result<(), ()> {
                        GLOBALS.set(globals, || {
                            let fm = cm.load_file(&path).expect("failed to load file");

                            let program = parse_file_as_module(
                                &fm,
                                Default::default(),
                                Default::default(),
                                None,
                                &mut Vec::new(),
                            )
                            .map_err(|err| {
                                err.into_diagnostic(&handler).emit();
                            })
                            .unwrap();

                            black_box(program);

                            Ok(())
                        })
                    })
                    .collect::<Vec<_>>();

                Ok(())
            })
        })
    })
    .unwrap();

    eprintln!("Took {:?}", start.elapsed());
}

/// Return the whole input files as abolute path.
fn expand_dirs(dirs: Vec<String>) -> Vec<PathBuf> {
    dirs.into_iter()
        .map(PathBuf::from)
        .map(|dir| dir.canonicalize().unwrap())
        .flat_map(|dir| {
            WalkDir::new(dir)
                .into_iter()
                .filter_map(Result::ok)
                .filter_map(|entry| {
                    if entry.metadata().map(|v| v.is_file()).unwrap_or(false) {
                        Some(entry.into_path())
                    } else {
                        None
                    }
                })
                .filter(|path| path.extension().map(|ext| ext == "js").unwrap_or(false))
                .collect::<Vec<_>>()
        })
        .collect()
}
