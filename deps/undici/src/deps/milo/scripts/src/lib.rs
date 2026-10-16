use std::{error::Error, fs, path::Path};

use serde::{Deserialize, Serialize};
use serde_json::{Map, Value};

// Reuse the layout used by the macros without spawning Cargo at runtime.
// The macro generator and CLI entry point are unused in this crate.
#[allow(dead_code)]
#[path = "../../macros/src/parser_fields.rs"]
mod parser_fields;

#[derive(Serialize)]
pub struct Version {
  pub raw: String,
  pub major: u64,
  pub minor: u64,
  pub patch: u64,
  pub prerelease: String,
}

#[derive(Serialize)]
pub struct BuildInfo {
  pub version: Version,
  pub constants: Map<String, Value>,
}

#[derive(Deserialize)]
struct Package {
  version: String,
}

#[derive(Deserialize)]
struct Manifest {
  package: Package,
}

fn read_yaml_list(name: &str) -> Result<Vec<String>, Box<dyn Error>> {
  let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
  let raw = fs::read_to_string(root.join(format!("macros/constants/{name}.yml")))?;
  Ok(serde_yaml::from_str(&raw)?)
}

fn read_version() -> Result<Version, Box<dyn Error>> {
  let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
  let manifest: Manifest = toml::from_str(&fs::read_to_string(root.join("parser/Cargo.toml"))?)?;
  let parsed = semver::Version::parse(&manifest.package.version)?;
  Ok(Version {
    raw: manifest.package.version,
    major: parsed.major,
    minor: parsed.minor,
    patch: parsed.patch,
    prerelease: parsed.pre.to_string(),
  })
}

fn read_parser_fields() -> Map<String, Value> {
  // The JavaScript helper spawned Cargo; Rust can reuse the same offsets
  // directly.
  parser_fields::FIELDS
    .iter()
    .map(|&(name, offset)| (format!("PARSER_FIELD_{name}"), offset.into()))
    .collect()
}

/// Reads the current repository inputs, independently of the working directory.
pub fn get_build_info() -> Result<BuildInfo, Box<dyn Error>> {
  let version = read_version()?;
  let methods = read_yaml_list("methods")?;
  let errors = read_yaml_list("errors")?;
  let callbacks = read_yaml_list("callbacks")?;
  let states = read_yaml_list("states")?;
  let parser_fields = read_parser_fields();

  // Insertion order is part of the generated JSON and downstream bindings.
  let mut constants = Map::new();
  for (i, method) in methods.iter().enumerate() {
    constants.insert(format!("METHOD_{}", method.replace('-', "_")), i.into());
  }
  for (i, callback) in callbacks.iter().enumerate() {
    constants.insert(format!("CALLBACK_{}", callback.to_uppercase()), i.into());
  }
  constants.insert("EVENT_END".into(), 0.into());
  for (i, callback) in callbacks.iter().enumerate() {
    let name = callback.strip_prefix("on_").unwrap_or(callback).to_uppercase();
    constants.insert(format!("EVENT_{name}"), (i + 1).into());
  }

  let mut all = 0;
  constants.insert("CALLBACK_ACTIVE_NONE".into(), 0.into());
  constants.insert("EVENT_ACTIVE_NONE".into(), 0.into());
  for (i, callback) in callbacks.iter().enumerate() {
    // Match JavaScript's signed 32-bit shifts, including the masked shift count.
    let bit = 1_i32.wrapping_shl((i % 32) as u32);
    constants.insert(format!("CALLBACK_ACTIVE_{}", callback.to_uppercase()), bit.into());
    constants.insert(format!("EVENT_ACTIVE_{}", callback.to_uppercase()), bit.into());
    all |= bit;
  }
  constants.insert("CALLBACK_ACTIVE_ALL".into(), all.into());
  constants.insert("EVENT_ACTIVE_ALL".into(), all.into());
  for (i, error) in errors.iter().enumerate() {
    constants.insert(format!("ERROR_{error}"), i.into());
  }
  for (i, state) in states.iter().enumerate() {
    constants.insert(format!("STATE_{}", state.to_uppercase()), i.into());
  }
  constants.extend(parser_fields);

  Ok(BuildInfo { version, constants })
}
