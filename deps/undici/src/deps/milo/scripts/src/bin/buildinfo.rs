use std::error::Error;

use milo_build_scripts::get_build_info;

fn main() -> Result<(), Box<dyn Error>> {
  println!("{}", serde_json::to_string_pretty(&get_build_info()?)?);
  Ok(())
}
