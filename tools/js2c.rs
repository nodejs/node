use std::collections::BTreeMap;
use std::fs;
use std::io::{self, Write};
use std::path::{Path, PathBuf};

const TEMPLATE: &str = r#"
#include "env-inl.h"
#include "node_builtins.h"
#include "node_external_reference.h"
#include "node_internals.h"

namespace node {

namespace builtins {

{definitions}
namespace {
const ThreadsafeCopyOnWrite<BuiltinSourceMap> global_source_map {
  BuiltinSourceMap {
{initializers}
  }  // BuiltinSourceMap
};  // ThreadsafeCopyOnWrite
}  // anonymous namespace

void BuiltinLoader::LoadJavaScriptSource() {
  source_ = global_source_map;
}

void RegisterExternalReferencesForInternalizedBuiltinCode(
  ExternalReferenceRegistry* registry) {
{registrations}
}

UnionBytes BuiltinLoader::GetConfig() {
  return UnionBytes(&config_resource);
}

}  // namespace builtins

}  // namespace node
"#;

const LATIN1_STRING_LITERAL_START: &str = "reinterpret_cast<const uint8_t*>(\"";
const ASCII_STRING_LITERAL_START: &str =
    "reinterpret_cast<const uint8_t*>(R\"JS2C1b732aee(";
const UTF16_STRING_LITERAL_START: &str =
    "reinterpret_cast<const uint16_t*>(uR\"JS2C1b732aee(";
const LATIN1_STRING_LITERAL_END: &str = "\");";
const UTF_STRING_LITERAL_END: &str = ")JS2C1b732aee\");";
const ARRAY_LITERAL_START: &str = "{\n";
const ARRAY_LITERAL_END: &str = "\n};\n\n";

const MJS_SUFFIX: &str = ".mjs";
const JS_SUFFIX: &str = ".js";
const GYPI_SUFFIX: &str = ".gypi";
const DEPS_PREFIX: &str = "deps/";
const LIB_PREFIX: &str = "lib/";

#[cfg(node_js2c_use_string_literals)]
const USE_STRING_LITERALS: bool = true;
#[cfg(not(node_js2c_use_string_literals))]
const USE_STRING_LITERALS: bool = false;

#[derive(Clone, Copy)]
enum CodeType {
    Ascii,
    Latin1,
    TwoByte,
}

#[derive(Clone, Copy)]
enum BuiltinSourceType {
    BootstrapRealm,
    BootstrapScript,
    PerContextScript,
    MainScript,
    Function,
    SourceTextModule,
}

impl BuiltinSourceType {
    fn as_cpp_name(self) -> &'static str {
        match self {
            Self::BootstrapRealm => "kBootstrapRealm",
            Self::BootstrapScript => "kBootstrapScript",
            Self::PerContextScript => "kPerContextScript",
            Self::MainScript => "kMainScript",
            Self::Function => "kFunction",
            Self::SourceTextModule => "kSourceTextModule",
        }
    }
}

struct Context {
    verbose: bool,
}

impl Context {
    fn debug(&self, args: std::fmt::Arguments<'_>) {
        if self.verbose {
            let _ = io::stderr().write_fmt(args);
        }
    }
}

macro_rules! debug {
    ($ctx:expr, $($arg:tt)*) => {
        $ctx.debug(format_args!($($arg)*))
    };
}

fn print_error(message: &str) {
    let _ = writeln!(io::stderr(), "{message}");
}

fn filename_is_config_gypi(path: &str) -> bool {
    path == "config.gypi" || path.ends_with("/config.gypi")
}

fn has_allowed_extension(filename: &str) -> Option<&'static str> {
    if filename.ends_with(GYPI_SUFFIX) {
        Some(GYPI_SUFFIX)
    } else if filename.ends_with(JS_SUFFIX) {
        Some(JS_SUFFIX)
    } else if filename.ends_with(MJS_SUFFIX) {
        Some(MJS_SUFFIX)
    } else {
        None
    }
}

fn search_files(
    dir: &Path,
    js_files: &mut Vec<String>,
    mjs_files: &mut Vec<String>,
) -> io::Result<()> {
    for entry in fs::read_dir(dir)? {
        let entry = entry?;
        let path = entry.path();
        if path.is_dir() {
            search_files(&path, js_files, mjs_files)?;
            continue;
        }

        let path_str = path_to_posix_string(&path)?;
        if path_str.ends_with(JS_SUFFIX) {
            js_files.push(path_str);
        } else if path_str.ends_with(MJS_SUFFIX) {
            mjs_files.push(path_str);
        }
    }

    Ok(())
}

fn path_to_posix_string(path: &Path) -> io::Result<String> {
    let value = path
        .to_str()
        .ok_or_else(|| io::Error::new(io::ErrorKind::InvalidData, "non-utf8 path"))?;
    Ok(value.replace('\\', "/"))
}

fn read_file(path: &str, ctx: &Context) -> io::Result<Vec<u8>> {
    debug!(ctx, "ReadFileSync {path}\n");
    fs::read(path)
}

fn write_if_changed(out: &[u8], dest: &str, ctx: &Context) -> io::Result<()> {
    debug!(ctx, "output size {}\n", out.len());

    let changed = match fs::read(dest) {
        Ok(existing) => {
            debug!(ctx, "existing size {}\n", existing.len());
            existing != out
        }
        Err(err) if err.kind() == io::ErrorKind::NotFound => {
            debug!(ctx, "existing size 0\n");
            true
        }
        Err(err) => return Err(err),
    };

    if !changed {
        debug!(ctx, "No change, return\n");
        return Ok(());
    }

    debug!(ctx, "WriteFileSync {} bytes to {dest}\n", out.len());
    fs::write(dest, out)
}

fn get_file_id(filename: &str) -> String {
    let mut end = filename.len();
    let mut start = 0;
    let mut prefix = "";

    // Match the builtin IDs expected by Node's builtin loader by removing the
    // source extension and rewriting the lib/ and deps/ roots.
    if filename.ends_with(MJS_SUFFIX) {
        end -= MJS_SUFFIX.len();
    } else if filename.ends_with(JS_SUFFIX) {
        end -= JS_SUFFIX.len();
    }

    if filename.starts_with(DEPS_PREFIX) {
        start = DEPS_PREFIX.len();
        prefix = "internal/deps/";
    } else if filename.starts_with(LIB_PREFIX) {
        start = LIB_PREFIX.len();
    }

    format!("{prefix}{}", &filename[start..end])
}

fn get_variable_name(id: &str) -> String {
    id.chars()
        .map(|ch| match ch {
            '.' | '-' | '/' => '_',
            other => other,
        })
        .collect()
}

fn append_octal_code(out: &mut String, ch: u8) {
    // Keep simple printable bytes readable in generated literals, but force
    // escapes for bytes that could terminate or alter the C++ string literal.
    if (b' '..=b'~').contains(&ch) && ch != b'\\' && ch != b'"' && ch != b'?' {
        out.push(char::from(ch));
        return;
    }

    out.push('\\');
    out.push(char::from(b'0' + ((ch >> 6) & 7)));
    out.push(char::from(b'0' + ((ch >> 3) & 7)));
    out.push(char::from(b'0' + (ch & 7)));
}

fn append_decimal<T>(out: &mut String, mut value: T)
where
    T: Copy + Default + Eq + From<u8> + std::ops::DivAssign<T> + std::ops::Rem<Output = T>,
{
    if value == T::default() {
        out.push('0');
        return;
    }

    let mut buf = [0u8; 20];
    let mut index = buf.len();
    while value != T::default() {
        let digit = value % T::from(10);
        value /= T::from(10);
        index -= 1;
        buf[index] = b'0' + digit_to_u8(digit);
    }
    for &digit in &buf[index..] {
        out.push(char::from(digit));
    }
}

fn digit_to_u8<T>(value: T) -> u8
where
    T: Copy + Eq + From<u8>,
{
    let mut digit = 0u8;
    while T::from(digit) != value {
        digit += 1;
    }
    digit
}

fn utf8_to_latin1(code: &[u8]) -> Option<Vec<u8>> {
    let text = std::str::from_utf8(code).ok()?;
    let mut out = Vec::with_capacity(text.len());
    for ch in text.chars() {
        let value = ch as u32;
        if value > 0xFF {
            return None;
        }
        out.push(value as u8);
    }
    Some(out)
}

fn simplify(code: &[u8], var: &str, ctx: &Context) -> Option<Vec<u8>> {
    if var != "internal_deps_undici_undici" {
        return None;
    }

    // Keep this hot-fix allowlisted so we do not silently rewrite arbitrary
    // builtins while trying to save space in the embedded output.
    let mut simplified = Vec::with_capacity(code.len());
    let mut count = 0usize;
    let mut i = 0usize;
    while i < code.len() {
        if i + 2 < code.len() && code[i] == 226 && code[i + 1] == 128 && code[i + 2] == 153 {
            simplified.push(b'\'');
            count += 1;
            i += 3;
            continue;
        }

        simplified.push(code[i]);
        i += 1;
    }

    if count > 0 {
        debug!(
            ctx,
            "Simplified {count} characters, old size {}, new size {}\n",
            code.len(),
            simplified.len()
        );
        Some(simplified)
    } else {
        None
    }
}

fn get_builtin_source_type(id: &str, filename: &str) -> BuiltinSourceType {
    if filename.ends_with(MJS_SUFFIX) {
        return BuiltinSourceType::SourceTextModule;
    }
    if id.starts_with("internal/bootstrap/realm") {
        return BuiltinSourceType::BootstrapRealm;
    }
    if id.starts_with("internal/bootstrap/") {
        return BuiltinSourceType::BootstrapScript;
    }
    if id.starts_with("internal/per_context/") {
        return BuiltinSourceType::PerContextScript;
    }
    if id.starts_with("internal/main/") {
        return BuiltinSourceType::MainScript;
    }
    if id.starts_with("internal/deps/v8/tools/") {
        return BuiltinSourceType::SourceTextModule;
    }
    BuiltinSourceType::Function
}

fn get_definition(var: &str, code: &[u8], ctx: &Context) -> io::Result<String> {
    debug!(ctx, "GetDefinition {var}, code size {}\n", code.len());

    if code.iter().all(|byte| *byte < 0x80) {
        debug!(ctx, "ASCII-only, static size {}\n", code.len());
        return get_definition_impl(var, code, CodeType::Ascii, ctx);
    }

    if let Some(latin1) = utf8_to_latin1(code) {
        debug!(
            ctx,
            "Latin-1-only, old size {}, new size {}\n",
            code.len(),
            latin1.len()
        );
        return get_definition_impl(var, &latin1, CodeType::Latin1, ctx);
    }

    if let Some(simplified) = simplify(code, var, ctx) {
        debug!(ctx, "{var} is simplified, re-generate definition\n");
        return get_definition(var, &simplified, ctx);
    }

    get_definition_impl(var, code, CodeType::TwoByte, ctx)
}

fn get_definition_impl(
    var: &str,
    code: &[u8],
    code_type: CodeType,
    ctx: &Context,
) -> io::Result<String> {
    let (arr_type, resource_type) = match code_type {
        CodeType::Ascii | CodeType::Latin1 => ("uint8_t", "StaticExternalOneByteResource"),
        CodeType::TwoByte => ("uint16_t", "StaticExternalTwoByteResource"),
    };

    let utf8_text = match code_type {
        CodeType::Ascii | CodeType::Latin1 => None,
        CodeType::TwoByte => Some(
            std::str::from_utf8(code)
                .map_err(|_| io::Error::new(io::ErrorKind::InvalidData, "invalid utf-8 source"))?,
        ),
    };
    let utf16_code = if USE_STRING_LITERALS {
        None
    } else {
        match code_type {
            CodeType::Ascii | CodeType::Latin1 => None,
            CodeType::TwoByte => Some(utf8_text.expect("checked above").encode_utf16().collect::<Vec<_>>()),
        }
    };
    let count = match code_type {
        CodeType::Ascii | CodeType::Latin1 => code.len(),
        CodeType::TwoByte => match &utf16_code {
            Some(utf16) => utf16.len(),
            None => utf8_text.expect("checked above").encode_utf16().count(),
        },
    };

    let mut result = String::with_capacity(128 + code.len() * 4);
    if USE_STRING_LITERALS {
        result.push_str("static const ");
        result.push_str(arr_type);
        result.push_str(" *");
        result.push_str(var);
        result.push_str("_raw = ");
        match code_type {
            CodeType::Ascii => {
                result.push_str(ASCII_STRING_LITERAL_START);
                result.push_str(std::str::from_utf8(code).map_err(|_| {
                    io::Error::new(io::ErrorKind::InvalidData, "invalid ascii source")
                })?);
                result.push_str(UTF_STRING_LITERAL_END);
            }
            CodeType::Latin1 => {
                result.push_str(LATIN1_STRING_LITERAL_START);
                for (index, ch) in code.iter().copied().enumerate() {
                    if ch > 127 {
                        debug!(
                            ctx,
                            "In {var}, found non-ASCII Latin-1 character at {index}: {ch}\n"
                        );
                    }
                    append_octal_code(&mut result, ch);
                }
                result.push_str(LATIN1_STRING_LITERAL_END);
            }
            CodeType::TwoByte => {
                // Preserve the original UTF-8 source in a UTF-16 raw literal so
                // the compiler performs the code unit conversion for us.
                result.push_str(UTF16_STRING_LITERAL_START);
                result.push_str(utf8_text.expect("checked above"));
                result.push_str(UTF_STRING_LITERAL_END);
            }
        }
    } else {
        result.push_str("static const ");
        result.push_str(arr_type);
        result.push(' ');
        result.push_str(var);
        result.push_str("_raw[] = ");
        result.push_str(ARRAY_LITERAL_START);

        match code_type {
            CodeType::Ascii | CodeType::Latin1 => {
                for (index, ch) in code.iter().copied().enumerate() {
                    if ch > 127 {
                        debug!(
                            ctx,
                            "In {var}, found non-ASCII Latin-1 character at {index}: {ch}\n"
                        );
                    }
                    append_decimal(&mut result, ch);
                    result.push(',');
                }
            }
            CodeType::TwoByte => {
                let utf16 = utf16_code.expect("computed above");
                debug!(ctx, "static size {}\n", utf16.len());
                for code_unit in utf16 {
                    append_decimal(&mut result, code_unit);
                    result.push(',');
                }
            }
        }

        result.push_str(ARRAY_LITERAL_END);
    }

    result.push_str("static ");
    result.push_str(resource_type);
    result.push(' ');
    result.push_str(var);
    result.push_str("_resource(");
    result.push_str(var);
    result.push_str("_raw, ");
    append_decimal(&mut result, count);
    result.push_str(", nullptr);\n");
    Ok(result)
}

fn replace_all(data: &[u8], search: &[u8], replacement: &[u8]) -> Vec<u8> {
    let mut result = Vec::with_capacity(data.len());
    let mut cursor = 0usize;

    while let Some(found) = data[cursor..]
        .windows(search.len())
        .position(|window| window == search)
    {
        let absolute = cursor + found;
        result.extend_from_slice(&data[cursor..absolute]);
        result.extend_from_slice(replacement);
        cursor = absolute + search.len();
    }

    result.extend_from_slice(&data[cursor..]);
    result
}

fn strip_comments(input: &[u8]) -> Vec<u8> {
    let mut result = Vec::with_capacity(input.len());

    for chunk in input.split_inclusive(|byte| *byte == b'\n') {
        // config.gypi only needs line-based shell-style comments removed before
        // it can be embedded as JSON-ish data.
        if let Some(pos) = chunk.iter().position(|byte| *byte == b'#') {
            result.extend_from_slice(&chunk[..pos]);
            if chunk.ends_with(b"\n") {
                result.push(b'\n');
            }
        } else {
            result.extend_from_slice(chunk);
        }
    }

    result
}

fn jsonify(code: &[u8]) -> Vec<u8> {
    // Preserve the existing js2c behavior for config.gypi by removing comment
    // lines and unquoting the pseudo-boolean strings it contains.
    let stripped = strip_comments(code);
    let result1 = replace_all(&stripped, br#""true""#, b"true");
    replace_all(&result1, br#""false""#, b"false")
}

fn add_module(
    filename: &str,
    definitions: &mut String,
    initializers: &mut String,
    registrations: &mut String,
    ctx: &Context,
) -> io::Result<()> {
    debug!(ctx, "AddModule {filename} start\n");
    let code = read_file(filename, ctx)?;
    let file_id = get_file_id(filename);
    let var = get_variable_name(&file_id);

    definitions.push_str(&get_definition(&var, &code, ctx)?);
    definitions.push('\n');
    let source_type = get_builtin_source_type(&file_id, filename).as_cpp_name();
    initializers.push_str("    {\"");
    initializers.push_str(&file_id);
    initializers.push_str("\", BuiltinSource{ \"");
    initializers.push_str(&file_id);
    initializers.push_str("\", UnionBytes(&");
    initializers.push_str(&var);
    initializers.push_str("_resource), BuiltinSourceType::");
    initializers.push_str(source_type);
    initializers.push_str("} },\n");
    registrations.push_str("  registry->Register(&");
    registrations.push_str(&var);
    registrations.push_str("_resource);\n");
    Ok(())
}

fn add_gypi(
    var: &str,
    filename: &str,
    definitions: &mut String,
    ctx: &Context,
) -> io::Result<()> {
    debug!(ctx, "AddGypi {filename} start\n");
    let code = read_file(filename, ctx)?;
    let transformed = jsonify(&code);
    definitions.push_str(&get_definition(var, &transformed, ctx)?);
    definitions.push('\n');
    Ok(())
}

fn format_output(definitions: &str, initializers: &str, registrations: &str) -> Vec<u8> {
    let mut out = String::with_capacity(
        TEMPLATE.len() + definitions.len() + initializers.len() + registrations.len(),
    );
    let mut rest = TEMPLATE;
    for (needle, replacement) in [
        ("{definitions}", definitions),
        ("{initializers}", initializers),
        ("{registrations}", registrations),
    ] {
        let (head, tail) = rest
            .split_once(needle)
            .expect("template placeholder missing");
        out.push_str(head);
        out.push_str(replacement);
        rest = tail;
    }
    out.push_str(rest);
    out.into_bytes()
}

fn js2c(js_files: &[String], mjs_files: &[String], config: &str, dest: &str, ctx: &Context) -> io::Result<()> {
    let input_count = js_files.len() + mjs_files.len() + 1;
    let mut definitions = String::with_capacity(input_count * 256);
    let mut initializers = String::with_capacity(input_count * 96);
    let mut registrations = String::with_capacity(input_count * 48);

    for filename in js_files {
        add_module(filename, &mut definitions, &mut initializers, &mut registrations, ctx)?;
    }
    for filename in mjs_files {
        add_module(filename, &mut definitions, &mut initializers, &mut registrations, ctx)?;
    }

    add_gypi("config", config, &mut definitions, ctx)?;
    let out = format_output(&definitions, &initializers, &registrations);
    write_if_changed(&out, dest, ctx)
}

fn print_usage(argv0: &str) -> i32 {
    print_error(&format!(
        "Usage: {argv0} [--verbose] [--root /path/to/project/root] path/to/output.cc path/to/directory [extra-files ...]"
    ));
    1
}

fn main_impl() -> Result<(), i32> {
    let mut raw_args = std::env::args();
    let argv0 = raw_args.next().unwrap_or_else(|| "js2c".to_string());

    let mut args = Vec::new();
    let mut root_dir: Option<String> = None;
    let mut verbose = false;

    while let Some(arg) = raw_args.next() {
        match arg.as_str() {
            "--verbose" => verbose = true,
            "--root" => {
                let Some(path) = raw_args.next() else {
                    print_error("--root must be followed by a path");
                    return Err(1);
                };
                root_dir = Some(path);
            }
            _ => args.push(arg),
        }
    }

    if args.len() < 2 {
        return Err(print_usage(&argv0));
    }

    if let Some(root_dir) = root_dir {
        if let Err(err) = std::env::set_current_dir(&root_dir) {
            print_error("Cannot switch to the directory specified by --root");
            print_error(&format!("chdir {root_dir}: {err}"));
            return Err(1);
        }
    }

    let ctx = Context { verbose };
    let output = args[0].clone();
    let mut file_map: BTreeMap<String, Vec<String>> = BTreeMap::new();

    for file in &args[1..] {
        let path = PathBuf::from(file);
        if path.is_dir() {
            let js_files = file_map.entry(JS_SUFFIX.to_string()).or_default();
            let mut found_js = std::mem::take(js_files);
            let mjs_files = file_map.entry(MJS_SUFFIX.to_string()).or_default();
            let mut found_mjs = std::mem::take(mjs_files);
            if let Err(err) = search_files(&path, &mut found_js, &mut found_mjs) {
                print_error(&format!("scandir {}: {err}", path.display()));
                return Err(1);
            }
            file_map.insert(JS_SUFFIX.to_string(), found_js);
            file_map.insert(MJS_SUFFIX.to_string(), found_mjs);
            continue;
        }

        if !path.exists() {
            print_error(&format!("Unsupported or missing file: {file}"));
            return Err(1);
        }

        match has_allowed_extension(file) {
            Some(extension) => file_map.entry(extension.to_string()).or_default().push(file.clone()),
            None => {
                print_error(&format!("Unsupported file: {file}"));
                return Err(1);
            }
        }
    }

    let Some(gypi_files) = file_map.get(GYPI_SUFFIX) else {
        print_error("Arguments should contain one and only one .gypi file: config.gypi");
        return Err(1);
    };
    if gypi_files.len() != 1 || !filename_is_config_gypi(&gypi_files[0]) {
        print_error("Arguments should contain one and only one .gypi file: config.gypi");
        return Err(1);
    }
    let config = gypi_files[0].clone();

    let mut js_files = file_map.remove(JS_SUFFIX).unwrap_or_default();
    let mut mjs_files = file_map.remove(MJS_SUFFIX).unwrap_or_default();

    if let Some(index) = mjs_files
        .iter()
        .position(|file| file == "lib/eslint.config_partial.mjs")
    {
        mjs_files.remove(index);
    }

    js_files.sort();
    mjs_files.sort();

    js2c(&js_files, &mjs_files, &config, &output, &ctx).map_err(|err| {
        print_error(&format!("{err}"));
        1
    })
}

fn main() {
    std::process::exit(match main_impl() {
        Ok(()) => 0,
        Err(code) => code,
    });
}
