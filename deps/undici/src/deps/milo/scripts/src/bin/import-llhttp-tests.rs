use std::{
  collections::HashSet,
  env,
  error::Error,
  fs,
  io::ErrorKind,
  path::{Path, PathBuf},
  process::{self, Command},
  sync::LazyLock,
};

use icu_collator::{Collator, options::CollatorOptions};
use icu_normalizer::DecomposingNormalizer;
use markdown::{ParseOptions, mdast::Node, to_mdast};
use regex::Regex;
use serde_json::{Map, Value, json};

const FIXTURE_PREFIX: &str = "tests/fixtures/llhttp";

struct Case {
  titles: Vec<String>,
  source: Value,
  meta: Value,
  http: Option<String>,
  log: Option<String>,
}

fn fail(message: &str) -> ! {
  eprintln!("{message}");
  process::exit(1);
}

fn clean_heading(title: &str) -> String { title.replace('`', "").trim().to_owned() }

fn decode_html_entities(value: &str) -> Result<String, Box<dyn Error>> {
  static ENTITIES: LazyLock<Regex> =
    LazyLock::new(|| Regex::new(r"&(?:quot|apos|amp|lt|gt|#(\d+)|#x([0-9a-fA-F]+));").unwrap());
  let mut decoded = String::new();
  let mut end = 0;
  for captures in ENTITIES.captures_iter(value) {
    let matched = captures.get(0).unwrap();
    decoded.push_str(&value[end..matched.start()]);
    let character = match matched.as_str() {
      "&quot;" => '"',
      "&apos;" => '\'',
      "&amp;" => '&',
      "&lt;" => '<',
      "&gt;" => '>',
      _ => {
        let code = if let Some(decimal) = captures.get(1) {
          decimal.as_str().parse::<u32>()?
        } else {
          u32::from_str_radix(captures.get(2).unwrap().as_str(), 16)?
        };
        char::from_u32(code).ok_or("Invalid HTML character reference")?
      }
    };
    decoded.push(character);
    end = matched.end();
  }
  decoded.push_str(&value[end..]);
  Ok(decoded)
}

fn parse_html_meta(value: &str) -> Result<Option<Value>, Box<dyn Error>> {
  static COMMENT: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"(?s)<!--\s*meta=(.*?)\s*-->").unwrap());
  static BARE: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"(?s)^\s*meta=(.*?)\s*$").unwrap());
  let Some(captures) = COMMENT.captures(value).or_else(|| BARE.captures(value)) else {
    return Ok(None);
  };
  // llhttp stores JSON metadata in markdown HTML nodes, sometimes entity-escaped
  // by the parser.
  let meta: Value = serde_json::from_str(&decode_html_entities(captures[1].trim())?)?;
  if !has_meta(&meta) {
    return Ok(None);
  }
  Ok(Some(meta))
}

fn has_meta(value: &Value) -> bool { value.as_object().is_some_and(|object| !object.is_empty()) }

fn stringify_fixture(fixture: &Value) -> String {
  // Match yaml's QUOTE_SINGLE values, PLAIN keys, and unlimited line width.
  // serde_yaml has no option for choosing the scalar style, so emit this JSON
  // subset explicitly while retaining the insertion order of object keys.
  fn quote(value: &str, indent: usize, key: bool) -> String {
    let double = value
      .chars()
      .any(|c| matches!(c, '\0'..='\u{8}' | '\u{b}'..='\u{1f}' | '\u{7f}'..='\u{9f}'))
      || (key && value.contains('\n'))
      || value
        .as_bytes()
        .windows(2)
        .any(|pair| matches!(pair, [b' ' | b'\t', b'\n'] | [b'\n', b' ' | b'\t']));
    if double {
      let json = serde_json::to_string(value).unwrap();
      let characters = json.chars().collect::<Vec<_>>();
      let mut result = String::new();
      let mut i = 0;
      while i < characters.len() {
        if characters[i] == ' ' && characters.get(i + 1..i + 3) == Some(&['\\', 'n']) {
          result.push_str("\\ ");
          i += 1;
        }
        if characters[i] == '\\' {
          match characters[i + 1] {
            'u' => {
              let code = characters[i + 2..i + 6].iter().collect::<String>();
              match code.as_str() {
                "0000" => result.push_str("\\0"),
                "0007" => result.push_str("\\a"),
                "000b" => result.push_str("\\v"),
                "001b" => result.push_str("\\e"),
                _ if code.starts_with("00") => result.push_str(&format!("\\x{}", &code[2..])),
                _ => result.push_str(&format!("\\u{code}")),
              }
              i += 6;
              continue;
            }
            'n' if !key && characters.get(i + 2) != Some(&'"') && json.encode_utf16().count() >= 40 => {
              result.push_str("\n\n");
              while characters.get(i + 2..i + 4) == Some(&['\\', 'n']) && characters.get(i + 4) != Some(&'"') {
                result.push('\n');
                i += 2;
              }
              result.push_str(&" ".repeat(indent));
              if characters.get(i + 2) == Some(&' ') {
                result.push('\\');
              }
              i += 2;
              continue;
            }
            _ => {
              result.push(characters[i]);
              result.push(characters[i + 1]);
              i += 2;
              continue;
            }
          }
        }
        result.push(characters[i]);
        i += 1;
      }
      return result;
    }
    let mut result = String::from("'");
    let mut characters = value.chars().peekable();
    while let Some(character) = characters.next() {
      result.push(character);
      if character == '\'' {
        result.push('\'');
      } else if character == '\n' && characters.peek() != Some(&'\n') {
        result.push('\n');
        result.push_str(&" ".repeat(indent));
      }
    }
    result.push('\'');
    result
  }

  fn key(value: &str, indent: usize) -> String {
    static SPECIAL: LazyLock<Regex> = LazyLock::new(|| {
      Regex::new(r#"^[\n\t ,\[\]{}#&*!|>'"%@`]|^[?-]$|^[?-][ \t]|[\n:][ \t]|[ \t]\n|[\n\t ]#|[\n\t :]$"#).unwrap()
    });
    if value.is_empty()
      || SPECIAL.is_match(value)
      || !matches!(serde_yaml::from_str::<Value>(value), Ok(Value::String(_)))
    {
      return quote(value, indent, true);
    }
    value.to_owned()
  }

  fn scalar(value: &Value, indent: usize) -> Option<String> {
    match value {
      Value::String(value) => Some(quote(value, indent, false)),
      Value::Array(value) if value.is_empty() => Some("[]".into()),
      Value::Object(value) if value.is_empty() => Some("{}".into()),
      Value::Array(_) | Value::Object(_) => None,
      _ => Some(value.to_string()),
    }
  }

  fn emit(value: &Value, indent: usize) -> String {
    let padding = " ".repeat(indent);
    let mut output = String::new();
    match value {
      Value::Object(object) => {
        for (name, value) in object {
          output.push_str(&format!("{padding}{}:", key(name, indent)));
          if let Some(value) = scalar(value, indent + 2) {
            output.push_str(&format!(" {value}\n"));
          } else {
            output.push('\n');
            output.push_str(&emit(value, indent + 2));
          }
        }
      }
      Value::Array(array) => {
        for value in array {
          output.push_str(&format!("{padding}- "));
          if let Some(value) = scalar(value, indent + 2) {
            output.push_str(&value);
            output.push('\n');
          } else {
            let nested = emit(value, indent + 2);
            output.push_str(&nested[indent + 2..]);
          }
        }
      }
      _ => {
        output.push_str(&format!("{padding}{}\n", scalar(value, indent).unwrap()));
      }
    }
    output
  }

  format!("---\n{}", emit(fixture, 0))
}

fn normalize_fixture_for_comparison(value: &Value) -> Value {
  match value {
    Value::Array(values) => values.iter().map(normalize_fixture_for_comparison).collect(),
    Value::Object(object) => {
      // Compare semantic fixture data only; checked is local review state and key
      // order is irrelevant.
      let mut keys = object.keys().filter(|key| *key != "checked").collect::<Vec<_>>();
      keys.sort();
      keys
        .into_iter()
        .map(|key| (key.clone(), normalize_fixture_for_comparison(&object[key])))
        .collect()
    }
    _ => value.clone(),
  }
}

// Node's glob excludes dotfiles and does not descend into symlinked
// directories.
fn glob_files(root: &Path, extension: &str) -> Result<Vec<PathBuf>, Box<dyn Error>> {
  let entries = match fs::read_dir(root) {
    Ok(entries) => entries,
    Err(error) if error.kind() == ErrorKind::NotFound => return Ok(Vec::new()),
    Err(error) => return Err(error.into()),
  };
  let mut entries = entries.collect::<Result<Vec<_>, _>>()?;
  entries.sort_by_key(|entry| entry.file_name());
  let mut files = Vec::new();
  for entry in entries {
    if entry.file_name().to_string_lossy().starts_with('.') {
      continue;
    }
    if entry.file_type()?.is_dir() {
      files.extend(glob_files(&entry.path(), extension)?);
    } else if entry.path().extension().is_some_and(|value| value == extension) {
      files.push(entry.path());
    }
  }
  Ok(files)
}

fn process_section(
  llhttp_root: &Path,
  output_root: &Path,
  fixture_root: &Path,
  section: &str,
  seen_files: &mut HashSet<String>,
) -> Result<(), Box<dyn Error>> {
  let source = llhttp_root.join("test").join(section);
  let mut cases = Vec::new();
  let mut used_names = HashSet::new();

  // Find markdown files.
  let mut files = glob_files(&source, "md")?;
  // Use Unicode collation, as localeCompare does, rather than byte ordering.
  let collator = Collator::try_new(Default::default(), CollatorOptions::default())?;
  files.sort_by(|a, b| collator.compare(&a.to_string_lossy(), &b.to_string_lossy()));

  // Process each file.
  for file in files {
    let raw = fs::read_to_string(&file)?;
    let source_path = file.strip_prefix(llhttp_root)?.to_string_lossy().replace('\\', "/");
    // Parse llhttp markdown sections into test cases.
    // markdown-rs exposes the same mdast node types used by remark-parse.
    let tree = to_mdast(&raw, &ParseOptions::default()).map_err(|error| error.to_string())?;
    let mut parsed = Vec::new();
    let mut current_section: Option<String> = None;
    let mut current_case: Option<Case> = None;

    let flush_case = |current_case: &mut Option<Case>, parsed: &mut Vec<Case>| {
      // A valid llhttp markdown case is complete only after both input and expected
      // log blocks are seen.
      if let Some(case) = current_case.take()
        && case.http.is_some()
        && case.log.is_some()
      {
        parsed.push(case);
      }
    };

    for node in tree.children().into_iter().flatten() {
      if let Node::Heading(heading) = node
        && (heading.depth == 2 || heading.depth == 3)
      {
        flush_case(&mut current_case, &mut parsed);
        let heading_text = heading
          .children
          .iter()
          .filter_map(|child| {
            match child {
              Node::Text(text) => Some(text.value.as_str()),
              Node::InlineCode(code) => Some(code.value.as_str()),
              _ => None,
            }
          })
          .collect::<String>();
        let cleaned_heading = clean_heading(&heading_text);
        let titles = if heading.depth == 2 {
          current_section = Some(cleaned_heading.clone());
          vec![cleaned_heading]
        } else if let Some(section) = current_section.as_ref().filter(|section| !section.is_empty()) {
          vec![section.clone(), cleaned_heading]
        } else {
          vec![cleaned_heading]
        };
        current_case = Some(Case {
          titles,
          source: json!({ "path": source_path, "line": heading.position.as_ref().map_or(1, |position| position.start.line) }),
          meta: Value::Null,
          http: None,
          log: None,
        });
        continue;
      }

      if let Node::Html(html) = node
        && let Some(case) = current_case.as_mut().filter(|case| case.http.is_none())
      {
        if let Some(Value::Object(meta)) = parse_html_meta(&html.value)? {
          if !case.meta.is_object() {
            case.meta = Value::Object(Map::new());
          }
          case.meta.as_object_mut().unwrap().extend(meta);
        }
        continue;
      }

      if let Node::Code(code) = node
        && let Some(case) = current_case.as_mut()
      {
        let code_lang = code.lang.as_deref().unwrap_or("").to_lowercase();
        if code_lang == "http" {
          case.http = Some(code.value.clone());
        } else if code_lang == "log" {
          case.log = Some(code.value.clone());
        }
      }
    }
    flush_case(&mut current_case, &mut parsed);

    for item in parsed {
      let input = item.http.unwrap();
      let log = item.log.unwrap();
      if input.is_empty() || log.is_empty() {
        continue;
      }

      // Build deterministic fixture file name from titles.
      // Keep the original normalization and collision suffixes for fixture names.
      static NON_NAME: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"[^a-z0-9\s-]").unwrap());
      static SPACES: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"[\s_]+").unwrap());
      static HYPHENS: LazyLock<Regex> = LazyLock::new(|| Regex::new(r"-+").unwrap());
      let name = item
        .titles
        .iter()
        .map(|part| {
          let normalized = DecomposingNormalizer::new_nfkd()
            .normalize(&clean_heading(part))
            .to_lowercase();
          let normalized = NON_NAME.replace_all(&normalized, " ");
          let normalized = SPACES.replace_all(normalized.trim(), "-");
          HYPHENS.replace_all(&normalized, "-").trim_matches('-').to_owned()
        })
        .filter(|part| !part.is_empty())
        .collect::<Vec<_>>()
        .join("-");
      let mut file_name = format!("{}.yml", if name.is_empty() { "test" } else { &name });
      if !used_names.contains(&file_name) {
        used_names.insert(file_name.clone());
      } else {
        let mut counter = 2;
        while used_names.contains(&format!("{name}-{counter}.yml")) {
          counter += 1;
        }
        file_name = format!("{name}-{counter}.yml");
        used_names.insert(file_name.clone());
      }

      let prefix = &item.titles[0];
      let child = item.titles.get(1).filter(|child| !child.is_empty());
      // Keep display names aligned with existing fixture format.
      let title_case = |value: &str| {
        value
          .split_whitespace()
          .map(|part| {
            let mut characters = part.chars();
            format!("{}{}", characters.next().unwrap().to_uppercase(), characters.as_str())
          })
          .collect::<Vec<_>>()
          .join(" ")
      };
      let mut fixture = json!({
        "path": format!("{FIXTURE_PREFIX}/{section}/{file_name}"),
        "name": if let Some(child) = child {
          format!("{} / {}", clean_heading(prefix), title_case(&clean_heading(child)))
        } else {
          title_case(&clean_heading(prefix))
        },
        "checked": false,
        "source": item.source,
      });
      if has_meta(&item.meta) {
        fixture["meta"] = item.meta;
      }
      fixture["input"] = input.split('\n').map(|line| Value::String(line.into())).collect();
      fixture["llhttp"] = log.split('\n').map(|line| Value::String(line.into())).collect();
      cases.push((file_name, fixture));
    }
  }

  let target_dir = fixture_root.join(format!("{section}s"));
  fs::create_dir_all(&target_dir)?;
  let temp_fixture_path = target_dir.join(format!(".import-llhttp-{section}-temp.yml"));
  for (i, (path, fixture)) in cases.iter().enumerate() {
    println!("Processing {section} case {}/{}: {path}", i + 1, cases.len());
    let file_path = target_dir.join(path);
    seen_files.insert(format!("{section}s/{path}"));
    let mut initial_fixture = fixture.clone();
    initial_fixture["output"] = json!([]);
    let initial_content = stringify_fixture(&initial_fixture);
    fs::write(&temp_fixture_path, initial_content)?;

    // Run generator using a temporary fixture to compute output.
    let file_arg = temp_fixture_path
      .strip_prefix(output_root)?
      .to_string_lossy()
      .replace('\\', "/");
    let result = Command::new("cargo")
      .args(["run", "--example", "llhttp", "--", "--generate", &file_arg])
      .current_dir(output_root)
      .output();
    let stdout = match result {
      Ok(result)
        if result.status.success()
          && result.stdout.len() <= 10 * 1024 * 1024
          && result.stderr.len() <= 10 * 1024 * 1024 =>
      {
        String::from_utf8_lossy(&result.stdout).into_owned()
      }
      result => {
        let details = result
          .map(|result| String::from_utf8_lossy(&result.stderr).trim().to_owned())
          .unwrap_or_default();
        let details = if details.is_empty() {
          String::new()
        } else {
          format!("\n{details}")
        };
        fail(&format!("Failed to run cargo generator for {file_arg}{details}"));
      }
    };
    let Some(marker_index) = stdout.find("---") else {
      fail(&format!(
        "Failed to parse cargo output for {file_arg}: missing YAML marker"
      ));
    };
    let yaml_snippet = stdout[marker_index + 3..].trim();
    if yaml_snippet.is_empty() {
      fail(&format!(
        "Failed to parse cargo output for {file_arg}: empty YAML snippet"
      ));
    }
    let output: Value = serde_yaml::from_str(yaml_snippet)?;
    let mut final_fixture = fixture.clone();
    final_fixture["output"] = output;

    // Skip writes when only comments/checked differ semantically.
    let skip_overwrite = match fs::read_to_string(&file_path) {
      Ok(raw) => {
        serde_yaml::from_str::<Value>(&raw).is_ok_and(|existing_fixture| {
          normalize_fixture_for_comparison(&existing_fixture) == normalize_fixture_for_comparison(&final_fixture)
        })
      }
      Err(error) if error.kind() == ErrorKind::NotFound => false,
      Err(error) => return Err(error.into()),
    };
    if skip_overwrite {
      continue;
    }
    let final_content = stringify_fixture(&final_fixture);
    fs::write(file_path, final_content)?;
  }
  match fs::remove_file(temp_fixture_path) {
    Ok(()) => {}
    Err(error) if error.kind() == ErrorKind::NotFound => {}
    Err(error) => return Err(error.into()),
  }
  Ok(())
}

fn main() -> Result<(), Box<dyn Error>> {
  let args = env::args().collect::<Vec<_>>();
  if args.len() < 3 || args[1].is_empty() || args[2].is_empty() {
    println!("Usage: import-llhttp-tests <llhttp-root> <output-root>");
    return Ok(());
  }
  let llhttp_root = std::path::absolute(&args[1])?;
  let output_root = std::path::absolute(&args[2])?;
  let fixture_root = output_root.join(FIXTURE_PREFIX);
  fs::create_dir_all(&fixture_root)?;

  // Snapshot fixtures that existed before import starts.
  // Keep discovery order for stale-file messages, as JavaScript's Set does.
  let mut existing_files = Vec::new();
  for section in ["requests", "responses"] {
    let section_root = fixture_root.join(section);
    // Section folder may not exist yet.
    if let Ok(files) = glob_files(&section_root, "yml") {
      for file in files {
        existing_files.push(file.strip_prefix(&fixture_root)?.to_string_lossy().replace('\\', "/"));
      }
    }
  }
  let mut seen_files = HashSet::new();
  process_section(&llhttp_root, &output_root, &fixture_root, "request", &mut seen_files)?;
  process_section(&llhttp_root, &output_root, &fixture_root, "response", &mut seen_files)?;
  for file in existing_files {
    if seen_files.contains(&file) {
      continue;
    }
    match fs::remove_file(fixture_root.join(&file)) {
      Ok(()) => {}
      Err(error) if error.kind() == ErrorKind::NotFound => {}
      Err(error) => return Err(error.into()),
    }
    println!("Removed stale fixture: {file}");
  }
  Ok(())
}
