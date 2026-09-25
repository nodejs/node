#![allow(unused)]
use cmake::Config;
use std::env;
use std::path::PathBuf;

#[cfg(target_os = "windows")]
fn main() {}

#[cfg(not(target_os = "windows"))]
fn main() {
    let out_dir = env::var("OUT_DIR").unwrap();
    let out_path = PathBuf::from(out_dir);

    #[cfg(not(feature = "force_32"))]
    {
        let _ittnotify_64 = Config::new("./")
            .generator("Unix Makefiles")
            .no_build_target(true)
            .build();

        println!("cargo:rustc-link-search={}/build/bin/", out_path.display());
        println!("cargo:rustc-link-lib=static=ittnotify");
    }

    #[cfg(feature = "force_32")]
    #[cfg(not(any(target_os = "ios", target_os = "macos")))]
    {
        let _ittnotify_32 = Config::new("./")
            .generator("Unix Makefiles")
            .define("FORCE_32", "ON")
            .no_build_target(true)
            .build();

        println!("cargo:rustc-link-search={}/build/bin/", out_path.display());
        println!("cargo:rustc-link-lib=static=ittnotify32");
    }
}
