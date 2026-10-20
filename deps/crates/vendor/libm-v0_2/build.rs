use std::env;

mod configure;

fn main() {
    let cfg = configure::Config::from_env();

    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-changed=configure.rs");
    println!("cargo:rustc-check-cfg=cfg(assert_no_panic)");

    // If set, enable `no-panic`. Requires LTO (`release-opt` profile).
    if env::var("ENSURE_NO_PANIC").is_ok() {
        println!("cargo:rustc-cfg=assert_no_panic");
    }

    configure::emit_libm_config(&cfg);
}
