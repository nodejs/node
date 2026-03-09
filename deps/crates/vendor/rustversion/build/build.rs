#![allow(
    clippy::elidable_lifetime_names,
    clippy::enum_glob_use,
    clippy::must_use_candidate,
    clippy::single_match_else
)]

mod rustc;

use std::env;
use std::ffi::OsString;
use std::fmt::{self, Debug, Display};
use std::fs;
use std::iter;
use std::path::Path;
use std::process::{self, Command};

fn main() {
    println!("cargo:rerun-if-changed=build/build.rs");

    let rustc = env::var_os("RUSTC").unwrap_or_else(|| OsString::from("rustc"));
    let rustc_wrapper = env::var_os("RUSTC_WRAPPER").filter(|wrapper| !wrapper.is_empty());
    let wrapped_rustc = rustc_wrapper.iter().chain(iter::once(&rustc));

    let mut is_clippy_driver = false;
    let mut is_mirai = false;
    let version = loop {
        let mut command;
        if is_mirai {
            command = Command::new(&rustc);
        } else {
            let mut wrapped_rustc = wrapped_rustc.clone();
            command = Command::new(wrapped_rustc.next().unwrap());
            command.args(wrapped_rustc);
        }
        if is_clippy_driver {
            command.arg("--rustc");
        }
        command.arg("--version");

        let output = match command.output() {
            Ok(output) => output,
            Err(e) => {
                let rustc = rustc.to_string_lossy();
                eprintln!("Error: failed to run `{} --version`: {}", rustc, e);
                process::exit(1);
            }
        };

        let string = match String::from_utf8(output.stdout) {
            Ok(string) => string,
            Err(e) => {
                let rustc = rustc.to_string_lossy();
                eprintln!(
                    "Error: failed to parse output of `{} --version`: {}",
                    rustc, e,
                );
                process::exit(1);
            }
        };

        break match rustc::parse(&string) {
            rustc::ParseResult::Success(version) => version,
            rustc::ParseResult::OopsClippy if !is_clippy_driver => {
                is_clippy_driver = true;
                continue;
            }
            rustc::ParseResult::OopsMirai if !is_mirai && rustc_wrapper.is_some() => {
                is_mirai = true;
                continue;
            }
            rustc::ParseResult::Unrecognized
            | rustc::ParseResult::OopsClippy
            | rustc::ParseResult::OopsMirai => {
                eprintln!(
                    "Error: unexpected output from `rustc --version`: {:?}\n\n\
                    Please file an issue in https://github.com/dtolnay/rustversion",
                    string
                );
                process::exit(1);
            }
        };
    };

    if version.minor < 38 {
        // Prior to 1.38, a #[proc_macro] is not allowed to be named `cfg`.
        println!("cargo:rustc-cfg=cfg_macro_not_allowed");
    }

    if version.minor >= 80 {
        println!("cargo:rustc-check-cfg=cfg(cfg_macro_not_allowed)");
        println!("cargo:rustc-check-cfg=cfg(host_os, values(\"windows\"))");
    }

    let version = format!("{:#}\n", Render(&version));
    let out_dir = env::var_os("OUT_DIR").expect("OUT_DIR not set");
    let out_file = Path::new(&out_dir).join("version.expr");
    fs::write(out_file, version).expect("failed to write version.expr");

    let host = env::var_os("HOST").expect("HOST not set");
    if let Some("windows") = host.to_str().unwrap().split('-').nth(2) {
        println!("cargo:rustc-cfg=host_os=\"windows\"");
    }
}

// Shim Version's {:?} format into a {} format, because {:?} is unusable in
// format strings when building with `-Zfmt-debug=none`.
struct Render<'a>(&'a rustc::Version);

impl<'a> Display for Render<'a> {
    fn fmt(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
        Debug::fmt(self.0, formatter)
    }
}
