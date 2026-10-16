use std::{env, error::Error, fs, io::ErrorKind, path::Path};

use base64::{Engine, engine::general_purpose::STANDARD};
use milo_build_scripts::{BuildInfo, get_build_info};
use serde_json::Value;

const ENUMS: &[(&str, &str)] = &[
  ("ERROR_", "Errors"),
  ("METHOD_", "Methods"),
  ("CALLBACK_", "Callbacks"),
  ("CALLBACK_ACTIVE_", "CallbackActives"),
  ("EVENT_", "Events"),
  ("EVENT_ACTIVE_", "EventActives"),
  ("STATE_", "States"),
  ("PARSER_FIELD_", "ParserFields"),
];

enum GetterType {
  Bool,
  Number,
  BigInt,
  String,
}

const GETTERS: &[(&str, GetterType, &str)] = &[
  ("isAutodetect", GetterType::Bool, "is_autodetect"),
  ("isRequest", GetterType::Bool, "is_request"),
  ("isPaused", GetterType::Bool, "is_paused"),
  ("shouldManageUnconsumed", GetterType::Bool, "should_manage_unconsumed"),
  (
    "shouldSuspendAfterHeaders",
    GetterType::Bool,
    "should_suspend_after_headers",
  ),
  ("getMaxStartLineLength", GetterType::Number, "get_max_start_line_length"),
  ("getMaxHeaderLength", GetterType::Number, "get_max_header_length"),
  ("getMaxBodyPayload", GetterType::BigInt, "get_max_body_payload"),
  (
    "shouldContinueWithoutData",
    GetterType::Bool,
    "should_continue_without_data",
  ),
  ("isConnect", GetterType::Bool, "is_connect"),
  ("isDebug", GetterType::Bool, "is_debug"),
  ("shouldSkipBody", GetterType::Bool, "should_skip_body"),
  ("getState", GetterType::Number, "get_state"),
  ("getPosition", GetterType::Number, "get_position"),
  ("getParsed", GetterType::BigInt, "get_parsed"),
  ("getErrorCode", GetterType::Number, "get_error_code"),
  ("getMethod", GetterType::Number, "get_method"),
  ("getStatus", GetterType::Number, "get_status"),
  ("hasConnectionClose", GetterType::Bool, "has_connection_close"),
  ("hasConnectionUpgrade", GetterType::Bool, "has_connection_upgrade"),
  ("getContentLength", GetterType::BigInt, "get_content_length"),
  ("getChunkSize", GetterType::BigInt, "get_chunk_size"),
  (
    "getRemainingContentLength",
    GetterType::BigInt,
    "get_remaining_content_length",
  ),
  ("getRemainingChunkSize", GetterType::BigInt, "get_remaining_chunk_size"),
  ("hasContentLength", GetterType::Bool, "has_content_length"),
  ("hasTransferEncoding", GetterType::Bool, "has_transfer_encoding"),
  (
    "hasChunkedTransferEncoding",
    GetterType::Bool,
    "has_chunked_transfer_encoding",
  ),
  ("hasUpgrade", GetterType::Bool, "has_upgrade"),
  ("hasTrailers", GetterType::Bool, "has_trailers"),
  ("getErrorDescription", GetterType::String, "get_error_description_raw"),
];

const SETTERS: &[(&str, &str)] = &[
  ("setShouldAutodetect", "set_should_autodetect"),
  ("setShouldContinueWithoutData", "set_should_continue_without_data"),
  ("setIsRequest", "set_is_request"),
  ("setIsConnect", "set_is_connect"),
  ("setDebug", "set_debug"),
  ("setShouldManageUnconsumed", "set_should_manage_unconsumed"),
  ("setShouldSuspendAfterHeaders", "set_should_suspend_after_headers"),
  ("setMaxStartLineLength", "set_max_start_line_length"),
  ("setMaxHeaderLength", "set_max_header_length"),
  ("setMaxBodyPayload", "set_max_body_payload"),
  ("setShouldSkipBody", "set_should_skip_body"),
  ("setActiveCallbacks", "set_active_callbacks"),
  ("setActiveEvents", "set_active_events"),
];

fn get_callbacks(info: &BuildInfo) -> Vec<(&str, &Value)> {
  info
    .constants
    .iter()
    .filter(|(key, _)| key.starts_with("CALLBACK_") && !key.starts_with("CALLBACK_ACTIVE"))
    .map(|(key, value)| (key.as_str(), value))
    .collect()
}

fn generate_enums(info: &BuildInfo) -> String {
  let mut replacement = String::new();
  for &(selector, name) in ENUMS {
    let mut matching = info
      .constants
      .iter()
      .filter(|(key, _)| key.starts_with(selector))
      .collect::<Vec<_>>();
    let mut suffix = "";
    if selector == "CALLBACK_" {
      matching.retain(|(key, _)| !key.starts_with("CALLBACK_ACTIVE"));
    } else if selector == "EVENT_" {
      matching.retain(|(key, _)| !key.starts_with("EVENT_ACTIVE_"));
    } else if selector == "CALLBACK_ACTIVE_" || selector == "EVENT_ACTIVE_" {
      suffix = "n";
    }
    let entries = matching
      .iter()
      .map(|(key, value)| format!("  {}: {value}{suffix}", &key[selector.len()..]))
      .chain(
        matching
          .iter()
          .map(|(key, value)| format!("  {value}{suffix}: '{}'", &key[selector.len()..])),
      )
      .collect::<Vec<_>>()
      .join(",\n");
    replacement.push_str(&format!("const {name} = Object.freeze({{\n{entries}\n}})\n\n"));
  }
  replacement.trim_end().to_owned()
}

fn generate_enums_lists() -> String {
  ENUMS
    .iter()
    .map(|(_, name)| format!("{name},"))
    .collect::<Vec<_>>()
    .join("\n")
}

fn generate_constants(info: &BuildInfo) -> String {
  info
    .constants
    .iter()
    .map(|(key, value)| {
      let mut value = value.to_string();
      if key.starts_with("CALLBACK_ACTIVE_") || key.starts_with("EVENT_ACTIVE_") {
        value.push('n');
      }
      format!("{key}: {value},")
    })
    .collect::<Vec<_>>()
    .join("\n")
}

fn generate_getters() -> String {
  let mut replacement = String::new();
  for (getter, kind, raw_getter) in GETTERS {
    let call = format!("this.{raw_getter}(parser)");
    let body = match kind {
      GetterType::Bool => format!("  return {call} !== 0"),
      GetterType::Number => format!("  return {call} >>> 0"),
      GetterType::BigInt => format!("  return BigInt.asUintN(64, {call})"),
      GetterType::String => {
        format!(
          "  const raw = {call}\n  const len = Number(BigInt.asUintN(32, raw))\n  const ptr = Number(raw >> 32n)\n  \
           return textDecoder.decode(new Uint8Array(this.memory.buffer, ptr, len))"
        )
      }
    };
    replacement.push_str(&format!("function {getter} (parser) {{\n{body}\n}}\n\n"));
  }
  replacement.trim_end().to_owned()
}

fn generate_setters() -> String {
  let mut replacement = String::new();
  for (setter, raw_setter) in SETTERS {
    replacement.push_str(&format!(
      "function {setter} (parser, value) {{\n  this.{raw_setter}(parser, value)\n}}\n\n"
    ));
  }
  replacement.trim_end().to_owned()
}

fn generate_getters_list() -> String {
  GETTERS
    .iter()
    .map(|(name, _, _)| format!("{name}: {name}.bind(wasm),"))
    .collect::<Vec<_>>()
    .join("\n")
}

fn generate_setters_list() -> String {
  SETTERS
    .iter()
    .map(|(name, _)| format!("{name}: {name}.bind(wasm),"))
    .collect::<Vec<_>>()
    .join("\n")
}

fn generate_noop_callbacks(info: &BuildInfo) -> String {
  get_callbacks(info)
    .iter()
    .map(|(key, _)| format!("{}: noop,", key.replacen("CALLBACK_", "", 1).to_lowercase()))
    .collect::<Vec<_>>()
    .join("\n")
}

fn generate_simple_callbacks(info: &BuildInfo) -> String {
  get_callbacks(info)
    .iter()
    .map(|(key, value)| {
      let name = key.replacen("CALLBACK_", "", 1).to_lowercase();
      format!("{name} (parser, at, len) {{\n  spans[parser].push([{value}, at, len])\n}}")
    })
    .collect::<Vec<_>>()
    .join(",\n")
}

fn generate_module(profile: &str, info: &BuildInfo, loader: &str, commonjs: bool) -> Result<String, Box<dyn Error>> {
  let template = fs::read_to_string(Path::new(env!("CARGO_MANIFEST_DIR")).join("../parser/wasm/src/template.js"))?;
  let version = &info.version;
  let mut output = String::new();

  // The template is shared by bundled/unbundled and ESM/CJS outputs;
  // placeholders keep the runtime code identical.
  // Placeholders occupy complete lines. Emit already formatted fragments using
  // the template's indentation, so packaging needs no JavaScript formatter.
  for line in template.lines() {
    let trimmed = line.trim_start();
    if let Some(id) = trimmed.strip_prefix("/* REPLACE: ").and_then(|s| s.strip_suffix(" */")) {
      let replacement = match id {
        "module" => loader.to_owned(),
        "version" => {
          format!(
            "version: {{\n  raw: '{}',\n  major: {},\n  minor: {},\n  patch: {},\n  prerelease: '{}'\n}},",
            version.raw, version.major, version.minor, version.patch, version.prerelease
          )
        }
        "enums" => generate_enums(info),
        "enums:list" => generate_enums_lists(),
        "constants" => generate_constants(info),
        "getters" => generate_getters(),
        "setters" => generate_setters(),
        "getters:list" => generate_getters_list(),
        "setters:list" => generate_setters_list(),
        "callbacks:noop" => generate_noop_callbacks(info),
        "callbacks:simple" => generate_simple_callbacks(info),
        "start" => {
          if profile == "debug" {
            "wasm.__start()".to_owned()
          } else {
            continue;
          }
        }
        _ => {
          eprintln!("Unsupported placeholder type {id}");
          trimmed.to_owned()
        }
      };
      let indent = &line[..line.len() - trimmed.len()];
      for fragment in replacement.lines() {
        if !fragment.is_empty() {
          output.push_str(indent);
          output.push_str(fragment);
        }
        output.push('\n');
      }
    } else {
      output.push_str(line);
      output.push('\n');
    }
  }
  if commonjs {
    output = output
      .lines()
      .map(|line| {
        if line.starts_with("export function ") {
          &line["export ".len()..]
        } else {
          line
        }
      })
      .collect::<Vec<_>>()
      .join("\n");
    output.push('\n');
    output.push_str("\nmodule.exports = { wasmModule, noop, setup, simple }\n");
  }
  Ok(output)
}

fn generate_commonjs_package_json(package_json: &Value) -> Value {
  let mut cjs_package_json = package_json.clone();
  cjs_package_json["name"] = "@perseveranza-pets/milo-cjs".into();
  cjs_package_json["type"] = "commonjs".into();
  cjs_package_json
}

fn generate_variant(
  profile: &str,
  info: &BuildInfo,
  root_folder: &Path,
  variant: &str,
  commonjs: bool,
) -> Result<(), Box<dyn Error>> {
  let wasm_file = format!("{variant}.wasm");
  let source_folder = root_folder.join("src").join(variant);
  let profile_root = Path::new(env!("CARGO_MANIFEST_DIR")).join("../dist/wasm").join(profile);
  let wasm = STANDARD.encode(fs::read(profile_root.join("binary").join(&wasm_file))?);
  let unbundled_loader = if commonjs {
    format!(
      "const {{ readFileSync }} = require('node:fs')\nconst {{ join }} = require('node:path')\n\nconst wasmModule = \
       new WebAssembly.Module(readFileSync(join(__dirname, '../../binary/{wasm_file}')))"
    )
  } else {
    format!(
      "import {{ readFileSync }} from 'node:fs'\n\nexport const wasmModule = new WebAssembly.Module(readFileSync(new \
       URL('../../binary/{wasm_file}', import.meta.url)))"
    )
  };
  let bundled_loader = if commonjs {
    format!("const wasmModule = new WebAssembly.Module(\n  Buffer.from(\n    '{wasm}',\n    'base64'\n  )\n)")
  } else {
    format!(
      "export const wasmModule = new WebAssembly.Module(\n  Uint8Array.from(\n    globalThis.atob(\n      '{wasm}'\n    ),\n    c => c.codePointAt(0)\n  )\n)"
    )
  };
  let unbundled = generate_module(profile, info, &unbundled_loader, commonjs)?;
  let bundled = generate_module(profile, info, &bundled_loader, commonjs)?;
  fs::create_dir_all(&source_folder)?;
  fs::write(source_folder.join("unbundled.js"), unbundled)?;
  fs::write(source_folder.join("index.js"), bundled)?;
  Ok(())
}

fn copy_directory(source: &Path, destination: &Path) -> Result<(), Box<dyn Error>> {
  fs::create_dir_all(destination)?;
  for entry in fs::read_dir(source)? {
    let entry = entry?;
    let target = destination.join(entry.file_name());
    if entry.file_type()?.is_dir() {
      copy_directory(&entry.path(), &target)?;
    } else {
      fs::copy(entry.path(), target)?;
    }
  }
  Ok(())
}

// TODO@PI: Generate TypeScript declarations.
fn main() -> Result<(), Box<dyn Error>> {
  let info = get_build_info()?;
  let profile = env::args().nth(1).ok_or("Usage: postbuild-wasm <profile>")?;
  let root = Path::new(env!("CARGO_MANIFEST_DIR")).join("..");
  // Open the package.json and update the version.
  let mut package_json: Value = serde_json::from_str(&fs::read_to_string(root.join("parser/wasm/src/package.json"))?)?;
  let profile_root = root.join("dist/wasm").join(&profile);
  let root_folder = profile_root.join("package");
  let cjs_root_folder = profile_root.join("package-cjs");
  package_json["version"] = info.version.raw.clone().into();
  let cjs_package_json = generate_commonjs_package_json(&package_json);

  // Write files.
  for folder in [&root_folder, &cjs_root_folder] {
    match fs::remove_dir_all(folder) {
      Ok(()) => {}
      Err(error) if error.kind() == ErrorKind::NotFound => {}
      Err(error) => return Err(error.into()),
    }
  }
  fs::create_dir_all(&root_folder)?;
  fs::create_dir_all(&cjs_root_folder)?;
  copy_directory(&profile_root.join("binary"), &root_folder.join("binary"))?;
  copy_directory(&profile_root.join("binary"), &cjs_root_folder.join("binary"))?;
  generate_variant(&profile, &info, &root_folder, "simd", false)?;
  generate_variant(&profile, &info, &root_folder, "no-simd", false)?;
  generate_variant(&profile, &info, &cjs_root_folder, "simd", true)?;
  generate_variant(&profile, &info, &cjs_root_folder, "no-simd", true)?;
  fs::write(
    root_folder.join("package.json"),
    serde_json::to_string_pretty(&package_json)?,
  )?;
  fs::write(
    cjs_root_folder.join("package.json"),
    serde_json::to_string_pretty(&cjs_package_json)?,
  )?;
  // Copy other Markdown files from root.
  for name in ["CODE_OF_CONDUCT.md", "LICENSE.md", "README.md"] {
    fs::copy(root.join(name), root_folder.join(name))?;
    fs::copy(root.join(name), cjs_root_folder.join(name))?;
  }
  Ok(())
}
