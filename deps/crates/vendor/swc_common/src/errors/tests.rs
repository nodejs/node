use std::{
    io,
    path::{Path, PathBuf},
};

use sync::Lrc;
use BytePos;
use Span;

use super::*;
use crate::{FileLoader, FilePathMapping, SourceMap};

struct MyFileLoader;
impl FileLoader for MyFileLoader {
    /// Query the existence of a file.
    fn file_exists(&self, path: &Path) -> bool {
        println!("File exists?: {}", path.display());
        true
    }

    /// Return an absolute path to a file, if possible.
    fn abs_path(&self, _path: &Path) -> Option<PathBuf> {
        Some("/tmp.js".into())
    }

    /// Read the contents of an UTF-8 file into memory.
    fn read_file(&self, _path: &Path) -> io::Result<String> {
        Ok("
function foo() {
    with (window) {

    }
}"
        .into())
    }
}

#[test]
fn test() {
    let cm = SourceMap::with_file_loader(Box::new(MyFileLoader), FilePathMapping::empty());
    let file_map = cm
        .load_file(Path::new("tmp.js").into())
        .expect("failed to load tmp.js");
    println!(
        "File (start={},end={})",
        file_map.start_pos.0, file_map.end_pos.0
    );
    let start_pos = file_map.start_pos + BytePos(1);
    let end_pos = file_map.end_pos - BytePos(1);
    let full = Span::new(start_pos, end_pos);

    let handler = Handler::with_tty_emitter(ColorConfig::Always, false, false, Some(Arc::new(cm)));

    ::syntax_pos::GLOBALS.set(&::syntax_pos::Globals::new(), || {
        DiagnosticBuilder::new_with_code(
            &handler,
            Error,
            Some(DiagnosticId::Error("ABCDE".into())),
            "Test span_label",
        )
        .span(full)
        .emit();

        DiagnosticBuilder::new_with_code(
            &handler,
            super::Warning,
            Some(DiagnosticId::Lint("WITH_STMT".into())),
            "Lint: With statement",
        )
        .span(Span::new(start_pos + BytePos(21), start_pos + BytePos(25)))
        .emit();
    })
}
