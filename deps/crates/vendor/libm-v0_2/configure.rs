// Configuration shared with both libm and libm-test

use std::env;
use std::path::PathBuf;

#[derive(Debug)]
#[allow(dead_code)]
pub struct Config {
    pub manifest_dir: PathBuf,
    pub out_dir: PathBuf,
    pub opt_level: String,
    pub cargo_features: Vec<String>,
    pub target_triple: String,
    pub target_arch: String,
    pub target_env: String,
    pub target_families: Vec<String>,
    pub target_os: String,
    pub target_string: String,
    pub target_vendor: String,
    pub target_features: Vec<String>,
    pub reliable_f128: bool,
    pub reliable_f16: bool,
}

impl Config {
    pub fn from_env() -> Self {
        let target_triple = env::var("TARGET").unwrap();
        let target_families = env::var("CARGO_CFG_TARGET_FAMILY")
            .map(|feats| feats.split(',').map(ToOwned::to_owned).collect())
            .unwrap_or_default();
        let target_features = env::var("CARGO_CFG_TARGET_FEATURE")
            .map(|feats| feats.split(',').map(ToOwned::to_owned).collect())
            .unwrap_or_default();
        let cargo_features = env::vars()
            .filter_map(|(name, _value)| name.strip_prefix("CARGO_FEATURE_").map(ToOwned::to_owned))
            .map(|s| s.to_lowercase().replace("_", "-"))
            .collect();

        Self {
            target_triple,
            manifest_dir: PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap()),
            out_dir: PathBuf::from(env::var("OUT_DIR").unwrap()),
            opt_level: env::var("OPT_LEVEL").unwrap(),
            cargo_features,
            target_arch: env::var("CARGO_CFG_TARGET_ARCH").unwrap(),
            target_env: env::var("CARGO_CFG_TARGET_ENV").unwrap(),
            target_families,
            target_os: env::var("CARGO_CFG_TARGET_OS").unwrap(),
            target_string: env::var("TARGET").unwrap(),
            target_vendor: env::var("CARGO_CFG_TARGET_VENDOR").unwrap(),
            target_features,
            // Note that these are unstable options, so only show up with the nightly compiler or
            // with `RUSTC_BOOTSTRAP=1` (which is required to use the types anyway).
            reliable_f128: env::var_os("CARGO_CFG_TARGET_HAS_RELIABLE_F128").is_some(),
            reliable_f16: env::var_os("CARGO_CFG_TARGET_HAS_RELIABLE_F16").is_some(),
        }
    }
}

/// Libm gets most config options made available.
#[allow(dead_code)]
pub fn emit_libm_config(cfg: &Config) {
    emit_intrinsics_cfg();
    emit_arch_cfg();
    emit_optimization_cfg(cfg);
    emit_cfg_shorthands(cfg);
    emit_cfg_env(cfg);
    emit_f16_f128_cfg(cfg);
}

/// Tests don't need most feature-related config.
#[allow(dead_code)]
pub fn emit_test_config(cfg: &Config) {
    emit_optimization_cfg(cfg);
    emit_cfg_shorthands(cfg);
    emit_cfg_env(cfg);
    emit_f16_f128_cfg(cfg);
}

/// Simplify the feature logic for enabling intrinsics so code only needs to use
/// `cfg(intrinsics_enabled)`.
fn emit_intrinsics_cfg() {
    println!("cargo:rustc-check-cfg=cfg(intrinsics_enabled)");

    // Disabled by default; `unstable-intrinsics` enables again; `force-soft-floats` overrides
    // to disable.
    if cfg!(feature = "unstable-intrinsics") && !cfg!(feature = "force-soft-floats") {
        println!("cargo:rustc-cfg=intrinsics_enabled");
    }
}

/// Simplify the feature logic for enabling arch-specific features so code only needs to use
/// `cfg(arch_enabled)`.
fn emit_arch_cfg() {
    println!("cargo:rustc-check-cfg=cfg(arch_enabled)");

    // Enabled by default via the "arch" feature, `force-soft-floats` overrides to disable.
    if cfg!(feature = "arch") && !cfg!(feature = "force-soft-floats") {
        println!("cargo:rustc-cfg=arch_enabled");
    }
}

/// Some tests are extremely slow. Emit a config option based on optimization level.
fn emit_optimization_cfg(cfg: &Config) {
    println!("cargo:rustc-check-cfg=cfg(optimizations_enabled)");

    if !matches!(cfg.opt_level.as_str(), "0" | "1") {
        println!("cargo:rustc-cfg=optimizations_enabled");
    }
}

/// Provide an alias for common longer config combinations.
fn emit_cfg_shorthands(cfg: &Config) {
    println!("cargo:rustc-check-cfg=cfg(x86_no_sse)");
    if cfg.target_arch == "x86" && !cfg.target_features.iter().any(|f| f == "sse") {
        // Shorthand to detect i586 targets
        println!("cargo:rustc-cfg=x86_no_sse");
    }
}

/// Reemit config that we make use of for test logging.
fn emit_cfg_env(cfg: &Config) {
    println!(
        "cargo:rustc-env=CFG_CARGO_FEATURES={:?}",
        cfg.cargo_features
    );
    println!("cargo:rustc-env=CFG_OPT_LEVEL={}", cfg.opt_level);
    println!(
        "cargo:rustc-env=CFG_TARGET_FEATURES={:?}",
        cfg.target_features
    );
}

/// Configure whether or not `f16` and `f128` support should be enabled.
fn emit_f16_f128_cfg(cfg: &Config) {
    println!("cargo:rustc-check-cfg=cfg(f16_enabled)");
    println!("cargo:rustc-check-cfg=cfg(f128_enabled)");

    // `unstable-float` enables these features.
    if !cfg!(feature = "unstable-float") {
        return;
    }

    /* See the compiler-builtins configure file for info about the meaning of these options */

    // If the feature is set, disable both of these types.
    let no_f16_f128 = cfg.cargo_features.iter().any(|s| s == "no-f16-f128");

    println!("cargo:rustc-check-cfg=cfg(f16_enabled)");
    if cfg.reliable_f16 && !no_f16_f128 {
        println!("cargo:rustc-cfg=f16_enabled");
    }

    println!("cargo:rustc-check-cfg=cfg(f128_enabled)");
    if cfg.reliable_f128 && !no_f16_f128 {
        println!("cargo:rustc-cfg=f128_enabled");
    }
}
