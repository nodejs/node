use std::{env, error::Error, fs, path::Path};

use milo_build_scripts::get_build_info;

fn prepend_version_and_method_map(header: &str) -> Result<String, Box<dyn Error>> {
  let header_matcher = "namespace milo_parser {";
  let info = get_build_info()?;
  let version = info.version;
  // Create the method map, required by Node.js.
  let methods = info
    .constants
    .iter()
    .filter_map(|(key, value)| {
      key
        .strip_prefix("METHOD_")
        .map(|method| format!("  EACH({value}, {method}, {method}) \\"))
    })
    .collect::<Vec<_>>()
    .join("\n");
  let updated_header = format!(
    r#"#define MILO_VERSION "{}"
#define MILO_VERSION_MAJOR {}
#define MILO_VERSION_MINOR {}
#define MILO_VERSION_PATCH {}
#define MILO_VERSION_PRERELEASE "{}"

#define MILO_METHODS_MAP(EACH) \
{methods}

namespace milo_parser {{

struct Parser;"#,
    version.raw, version.major, version.minor, version.patch, version.prerelease
  );

  // Replace the header with the new code.
  // Match the JavaScript normalization without changing other whitespace.
  let mut normalized = String::with_capacity(header.len());
  let mut newlines = 0;
  for character in header.chars() {
    if character == '\n' {
      newlines += 1;
    } else {
      newlines = 0;
    }
    if newlines <= 2 {
      normalized.push(character);
    }
  }
  Ok(normalized.replacen(header_matcher, &updated_header, 1))
}

fn main() -> Result<(), Box<dyn Error>> {
  let profile = env::args().nth(1).ok_or("Usage: postbuild-cpp <profile>")?;
  // Read the file.
  let header_path = Path::new(env!("CARGO_MANIFEST_DIR"))
    .join("../dist/cpp")
    .join(profile)
    .join("milo.h");
  let header = fs::read_to_string(&header_path)?;
  // Apply modifications.
  let header = prepend_version_and_method_map(&header)?;
  // Write the updated file.
  fs::write(header_path, header)?;
  Ok(())
}
