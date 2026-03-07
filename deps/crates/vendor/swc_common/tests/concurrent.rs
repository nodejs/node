#![cfg(feature = "concurrent")]

use std::{env, path::PathBuf, sync::Arc};

use par_iter::prelude::*;
use swc_common::{sync::Lrc, FilePathMapping, SourceFile, SourceMap};

#[test]
fn no_overlap() {
    let cm = Lrc::new(SourceMap::new(FilePathMapping::empty()));

    let files: Vec<Lrc<SourceFile>> = (0..100000)
        .into_par_iter()
        .map(|_| {
            cm.load_file(
                &PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap())
                    .join("tests")
                    .join("concurrent.js"),
            )
            .unwrap()
        })
        .collect::<Vec<_>>();

    // This actually tests if there is overlap

    let mut start = files.clone();
    start.sort_by_key(|f| f.start_pos);
    let mut end = files;
    end.sort_by_key(|f| f.end_pos);

    start
        .into_par_iter()
        .zip(end)
        .for_each(|(start, end): (Arc<SourceFile>, Arc<SourceFile>)| {
            assert_eq!(start.start_pos, end.start_pos);
            assert_eq!(start.end_pos, end.end_pos);
            assert!(start.start_pos < start.end_pos);

            cm.lookup_char_pos(start.start_pos);
        });
}
