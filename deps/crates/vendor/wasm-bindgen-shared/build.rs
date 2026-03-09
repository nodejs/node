use std::collections::hash_map::DefaultHasher;
use std::env;
use std::hash::Hasher;
use std::path::PathBuf;
use std::process::Command;

fn main() {
    set_schema_version_env_var();

    let rev = Command::new("git")
        .arg("rev-parse")
        .arg("HEAD")
        .output()
        .ok()
        .map(|s| s.stdout)
        .and_then(|s| String::from_utf8(s).ok());
    if let Some(rev) = rev {
        if rev.len() >= 9 {
            println!("cargo:rustc-env=WBG_VERSION={}", &rev[..9]);
        }
    }
}

fn set_schema_version_env_var() {
    let cargo_manifest_dir = env::var("CARGO_MANIFEST_DIR").expect(
        "The `CARGO_MANIFEST_DIR` environment variable is needed to locate the schema file",
    );
    let schema_file = PathBuf::from(cargo_manifest_dir).join("src/lib.rs");
    let schema_file = std::fs::read_to_string(schema_file).unwrap();
    #[cfg(windows)]
    let schema_file = schema_file.replace("\r\n", "\n");

    let mut hasher = DefaultHasher::new();
    hasher.write(schema_file.as_bytes());

    println!("cargo:rustc-env=SCHEMA_FILE_HASH={}", hasher.finish());
}
