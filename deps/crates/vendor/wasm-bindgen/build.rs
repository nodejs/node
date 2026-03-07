// Required so that `[package] links = ...` works in `Cargo.toml`.

use rustversion_compat as rustversion;
use std::env;

macro_rules! deprecated_crate_feature {
    ($name:literal) => {
        #[cfg(feature = $name)]
        {
            println!("cargo:warning=The `{}` feature is deprecated and will be removed in the next major version.", $name);
        }
    };
}

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    deprecated_crate_feature!("msrv");
    deprecated_crate_feature!("rustversion");
    deprecated_crate_feature!("xxx_debug_only_print_generated_code");

    println!("cargo:rustc-check-cfg=cfg(wbg_diagnostic)");

    if rustversion::cfg!(since(1.78)) {
        println!("cargo:rustc-cfg=wbg_diagnostic");
    }

    let target_arch = env::var_os("CARGO_CFG_TARGET_ARCH").unwrap();
    let target_os = env::var_os("CARGO_CFG_TARGET_OS").unwrap();

    let target_features = env::var("CARGO_CFG_TARGET_FEATURE").unwrap_or_default();
    let target_features: Vec<_> = target_features.split(',').map(str::trim).collect();

    println!("cargo:rustc-check-cfg=cfg(wbg_reference_types)");

    if target_features.contains(&"reference-types")
        || (target_arch == "wasm32"
            && target_os == "unknown"
            && rustversion::cfg!(all(since(1.82), before(1.84))))
    {
        println!("cargo:rustc-cfg=wbg_reference_types");
    }
}
