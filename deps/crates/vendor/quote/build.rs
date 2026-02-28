use std::env;
use std::process::Command;
use std::str;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let Some(minor) = rustc_minor_version() else {
        return;
    };

    if minor >= 77 {
        println!("cargo:rustc-check-cfg=cfg(no_diagnostic_namespace)");
    }

    // Support for the `#[diagnostic]` tool attribute namespace
    // https://blog.rust-lang.org/2024/05/02/Rust-1.78.0.html#diagnostic-attributes
    if minor < 78 {
        println!("cargo:rustc-cfg=no_diagnostic_namespace");
    }
}

fn rustc_minor_version() -> Option<u32> {
    let rustc = env::var_os("RUSTC")?;
    let output = Command::new(rustc).arg("--version").output().ok()?;
    let version = str::from_utf8(&output.stdout).ok()?;
    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return None;
    }
    pieces.next()?.parse().ok()
}
