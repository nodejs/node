use std::{
  env,
  error::Error,
  fs,
  path::Path,
  process::{Command, Stdio},
};

use regex::{Captures, Regex};

fn main() -> Result<(), Box<dyn Error>> {
  let folders = ["macros", "parser", "references/rust", "parser/wasm/src"];
  let version = env::args().nth(1).ok_or("Usage: sync-versions <version>")?;
  let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("..");

  for folder in folders {
    let path = root.join(folder);
    env::set_current_dir(path)?;
    // Like execFileSync, wait for each update and discard its stdout.
    let output = Command::new("cambi")
      .args(["update", &version])
      .stderr(Stdio::inherit())
      .output()?;
    if !output.status.success() {
      return Err(format!("cambi update failed in {folder}: {}", output.status).into());
    }
  }

  // Dependency of milo-macros.
  // Update it without reformatting the manifest.
  let main_cargo = root.join("parser/Cargo.toml");
  let main_cargo_content = fs::read_to_string(&main_cargo)?;
  let matcher = Regex::new(r#"(?m)^(\s+milo-macros = \{ version = ")([^"]+)(")"#)?;
  let main_cargo_content = matcher.replace(&main_cargo_content, |captures: &Captures<'_>| {
    format!("{}{version}{}", &captures[1], &captures[3])
  });
  fs::write(main_cargo, main_cargo_content.as_bytes())?;
  Ok(())
}
