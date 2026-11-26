// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/importer.h"

#include <cctype>
#include <cstring>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/log.h"
#include "experimental/redline/engine/d8_flags.h"
#include "experimental/redline/fast/ast.h"
#include "experimental/redline/fast/parser.h"

namespace redline {

namespace {

constexpr char kLog[] = "import";

// If `statement` is `load("f")` or `d8.file.execute("f")`, returns "f".
std::string LoadedFile(const fast::Statement* statement) {
  const auto* expression_statement = statement->As<fast::ExpressionStatement>();
  if (expression_statement == nullptr) {
    return "";
  }
  const auto* call =
      expression_statement->expression->As<fast::CallExpression>();
  if (call == nullptr || call->arguments.size() != 1) {
    return "";
  }
  const auto* argument = call->arguments[0]->As<fast::StringLiteral>();
  if (argument == nullptr) {
    return "";
  }
  if (const auto* callee = call->callee->As<fast::Identifier>()) {
    if (callee->name != "load") {
      return "";
    }
    return argument->value;
  }
  // d8.file.execute(...)
  const auto* execute = call->callee->As<fast::MemberExpression>();
  if (execute == nullptr || execute->computed) {
    return "";
  }
  const auto* execute_name = execute->property->As<fast::Identifier>();
  if (execute_name == nullptr || execute_name->name != "execute") {
    return "";
  }
  const auto* file = execute->object->As<fast::MemberExpression>();
  if (file == nullptr || file->computed) {
    return "";
  }
  const auto* file_name = file->property->As<fast::Identifier>();
  const auto* d8 = file->object->As<fast::Identifier>();
  if (file_name == nullptr || file_name->name != "file" || d8 == nullptr ||
      d8->name != "d8") {
    return "";
  }
  return argument->value;
}

std::string DirName(const std::string& path) {
  size_t slash = path.rfind('/');
  if (slash == std::string::npos) {
    return ".";
  }
  if (slash == 0) {
    return "/";
  }
  return path.substr(0, slash);
}

bool EndsWith(const std::string& text, const char* suffix) {
  size_t n = strlen(suffix);
  return text.size() >= n && text.compare(text.size() - n, n, suffix) == 0;
}

bool IsPreludeProvidedHarness(const std::string& file) {
  return EndsWith(file, "mjsunit.js") || file == "sta.js" ||
         EndsWith(file, "/sta.js") || file == "assert.js" ||
         EndsWith(file, "/assert.js") || file == "compareArray.js" ||
         EndsWith(file, "/compareArray.js") || file == "doneprintHandle.js" ||
         EndsWith(file, "/doneprintHandle.js");
}

std::string_view Trim(std::string_view s) {
  size_t first = s.find_first_not_of(" \t\r\n");
  if (first == std::string_view::npos) {
    return "";
  }
  size_t last = s.find_last_not_of(" \t\r\n");
  return s.substr(first, last - first + 1);
}

bool HasTokenInBrackets(std::string_view list, std::string_view token) {
  size_t i = 0;
  while (i < list.size()) {
    while (i < list.size() &&
           (isspace(static_cast<unsigned char>(list[i])) || list[i] == ',')) {
      i++;
    }
    size_t start = i;
    while (i < list.size() && !isspace(static_cast<unsigned char>(list[i])) &&
           list[i] != ',') {
      i++;
    }
    if (i > start && list.substr(start, i - start) == token) {
      return true;
    }
  }
  return false;
}

// Parses Test262 `/*--- ... ---*/` frontmatter. Returns false with `reason` if
// the file is a negative or module test. Appends `includes:` files to
// `includes`.
bool ParseTest262Frontmatter(std::string_view source,
                             std::vector<std::string>* includes,
                             std::string* reason) {
  size_t start = source.find("/*---");
  if (start == std::string_view::npos) {
    return true;
  }
  size_t end = source.find("---*/", start + 5);
  if (end == std::string_view::npos) {
    return true;
  }
  std::string_view fm = source.substr(start + 5, end - (start + 5));
  bool in_includes_list = false;
  bool in_flags_list = false;
  size_t pos = 0;
  while (pos < fm.size()) {
    size_t nl = fm.find('\n', pos);
    if (nl == std::string_view::npos) {
      nl = fm.size();
    }
    std::string_view raw_line = fm.substr(pos, nl - pos);
    pos = nl + 1;
    std::string_view line = Trim(raw_line);
    if (line.empty() || line.starts_with("#")) {
      continue;
    }
    const bool indented =
        !raw_line.empty() && (raw_line[0] == ' ' || raw_line[0] == '\t');
    if (line.starts_with("-") && (in_includes_list || in_flags_list)) {
      std::string_view item = Trim(line.substr(1));
      if (in_includes_list && !item.empty()) {
        includes->emplace_back(item);
      } else if (in_flags_list && item == "module") {
        *reason = "test262 module test";
        return false;
      }
      continue;
    }
    if (indented) {
      in_includes_list = false;
      in_flags_list = false;
      continue;
    }
    in_includes_list = false;
    in_flags_list = false;
    if (line.starts_with("negative:")) {
      *reason = "test262 negative test";
      return false;
    }
    if (line.starts_with("flags:")) {
      std::string_view rest = Trim(line.substr(6));
      if (rest.starts_with("[") && rest.ends_with("]")) {
        if (HasTokenInBrackets(rest.substr(1, rest.size() - 2), "module")) {
          *reason = "test262 module test";
          return false;
        }
      } else if (rest.empty()) {
        in_flags_list = true;
      }
      continue;
    }
    if (line.starts_with("includes:")) {
      std::string_view rest = Trim(line.substr(9));
      if (rest.starts_with("[") && rest.ends_with("]")) {
        std::string_view inner = rest.substr(1, rest.size() - 2);
        size_t i = 0;
        while (i < inner.size()) {
          size_t comma = inner.find(',', i);
          if (comma == std::string_view::npos) {
            comma = inner.size();
          }
          std::string_view item = Trim(inner.substr(i, comma - i));
          if (!item.empty()) {
            includes->emplace_back(item);
          }
          i = comma + 1;
        }
      } else if (rest.empty()) {
        in_includes_list = true;
      }
    }
  }
  return true;
}

bool ResolveTest262Harness(const std::string& inc,
                           const std::string& loader_path,
                           const ImportOptions& options,
                           std::string* resolved) {
  std::vector<std::string> candidates;
  if (!options.root.empty()) {
    candidates.push_back(options.root + "/test/test262/data/harness/" + inc);
  }
  std::string dir = DirName(loader_path);
  while (!dir.empty() && dir != "." && dir != "/") {
    if (!EndsWith(dir, "/test")) {
      candidates.push_back(dir + "/harness/" + inc);
    }
    candidates.push_back(dir + "/data/harness/" + inc);
    size_t slash = dir.rfind('/');
    if (slash == std::string::npos) {
      break;
    }
    dir = slash == 0 ? "/" : dir.substr(0, slash);
  }
  candidates.push_back("test/test262/data/harness/" + inc);
  for (const std::string& candidate : candidates) {
    if (FileExists(candidate)) {
      *resolved = candidate;
      return true;
    }
  }
  return false;
}

// Finds the file a load() refers to: relative to the import root, then to
// the loading file, then to the current directory.
bool Resolve(const std::string& file, const std::string& loader_path,
             const ImportOptions& options, std::string* resolved) {
  std::vector<std::string> candidates;
  if (!file.empty() && file[0] == '/') {
    candidates.push_back(file);
  } else {
    if (!options.root.empty()) {
      candidates.push_back(options.root + "/" + file);
    }
    candidates.push_back(DirName(loader_path) + "/" + file);
    candidates.push_back(file);
  }
  for (const std::string& candidate : candidates) {
    if (FileExists(candidate)) {
      *resolved = candidate;
      return true;
    }
  }
  return false;
}

// Replaces load() statements in `tree` (parsed from `path`) by the loaded
// files' statements, recursively.
void InlineLoads(fast::Tree* tree, const std::string& path,
                 const ImportOptions& options, int depth, ImportInfo* info) {
  std::vector<fast::Statement*>& body = tree->root()->body;
  for (size_t i = 0; i < body.size();) {
    std::string file = LoadedFile(body[i]);
    if (file.empty()) {
      i++;
      continue;
    }
    body.erase(body.begin() + static_cast<ptrdiff_t>(i));
    // The prelude provides the `mjsunit` and Test262 assertion harnesses.
    if (IsPreludeProvidedHarness(file)) {
      continue;
    }
    std::string resolved;
    if (depth >= options.max_depth ||
        !Resolve(file, path, options, &resolved)) {
      info->dropped.push_back(file);
      continue;
    }
    std::string source, error;
    if (!ReadFileToString(resolved, &source, &error)) {
      info->dropped.push_back(file);
      continue;
    }
    fast::ParseError parse_error;
    std::unique_ptr<fast::Tree> loaded =
        fast::Parser::ParseScript(source, fast::ParseOptions(), &parse_error);
    if (loaded == nullptr) {
      REDLINE_VLOG(kLog, "%s: cannot inline %s: %s", path.c_str(),
                   resolved.c_str(), parse_error.ToString().c_str());
      info->dropped.push_back(file);
      continue;
    }
    InlineLoads(loaded.get(), resolved, options, depth + 1, info);
    std::vector<fast::Statement*> clones;
    for (const fast::Statement* statement : loaded->root()->body) {
      clones.push_back(tree->Clone(statement));
    }
    body.insert(body.begin() + static_cast<ptrdiff_t>(i), clones.begin(),
                clones.end());
    info->inlined.push_back(resolved);
    i += clones.size();
  }
}

bool IsSkippedTest262Path(std::string_view path) {
  if (path.find("/RegExp/property-escapes/generated/") !=
          std::string_view::npos ||
      path.find("/RegExp/unicodeSets/generated/") != std::string_view::npos) {
    return true;
  }
  if (path.find("test262") == std::string_view::npos) {
    return false;
  }
  for (std::string_view dir :
       {"/early-errors/", "/redeclaration/", "/module-code/",
        "/dynamic-import/", "/import.meta/", "/top-level-await/",
        "/source-phase-import", "/test/language/import/",
        "/test/language/export/", "/test/harness/"}) {
    if (path.find(dir) != std::string_view::npos) {
      return true;
    }
  }
  return false;
}

// The flags of the comment lines of `source` with `keyword` after `//` and
// white space.
std::vector<std::string> ParseKeywordLines(std::string_view source,
                                           std::string_view keyword) {
  std::vector<std::string> flags;
  size_t pos = 0;
  while (pos < source.size()) {
    size_t nl = source.find('\n', pos);
    if (nl == std::string_view::npos) {
      nl = source.size();
    }
    std::string_view line = source.substr(pos, nl - pos);
    pos = nl + 1;
    size_t comment = line.find("//");
    if (comment == std::string_view::npos) {
      continue;
    }
    std::string_view rest = line.substr(comment + 2);
    const size_t text = rest.find_first_not_of(" \t");
    if (text == 0 || text == std::string_view::npos ||
        !rest.substr(text).starts_with(keyword)) {
      continue;
    }
    rest = rest.substr(text + keyword.size());
    size_t i = 0;
    while (i < rest.size()) {
      while (i < rest.size() && isspace(static_cast<unsigned char>(rest[i]))) {
        i++;
      }
      const size_t start = i;
      while (i < rest.size() && !isspace(static_cast<unsigned char>(rest[i]))) {
        i++;
      }
      if (i > start) {
        flags.emplace_back(rest.substr(start, i - start));
      }
    }
  }
  return flags;
}

}  // namespace

std::vector<std::string> ParseFlagsLines(std::string_view source) {
  // The same lines as FLAGS_PATTERN of the V8 test runner: `//`, white space,
  // then `Flags:`.
  return ParseKeywordLines(source, "Flags:");
}

std::vector<std::string> ParseRedlineFlagsLines(std::string_view source) {
  // kRedlineFlagsPrefix without `// `.
  return ParseKeywordLines(source, "redline-flags:");
}

std::unique_ptr<fast::Tree> ImportSource(std::string_view source,
                                         const std::string& path,
                                         const ImportOptions& options,
                                         ImportInfo* info, std::string* error) {
  if (EndsWith(path, "_FIXTURE.js")) {
    *error = "test262 fixture file";
    return nullptr;
  }
  if (IsSkippedTest262Path(path)) {
    *error = "test262 skipped path";
    return nullptr;
  }
  info->flags =
      SanitizeTestFlags(ParseFlagsLines(source), &info->rejected_flags);
  info->program_flags =
      SanitizeTestFlags(ParseRedlineFlagsLines(source), &info->rejected_flags);
  std::vector<std::string> includes;
  if (!ParseTest262Frontmatter(source, &includes, error)) {
    return nullptr;
  }
  fast::ParseError parse_error;
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(source, fast::ParseOptions(), &parse_error);
  if (tree == nullptr) {
    *error = parse_error.ToString();
    return nullptr;
  }
  InlineLoads(tree.get(), path, options, 0, info);
  if (!includes.empty()) {
    std::vector<fast::Statement*> prefix;
    for (const std::string& inc : includes) {
      if (IsPreludeProvidedHarness(inc)) {
        continue;
      }
      std::string resolved;
      if (options.max_depth <= 0 ||
          !ResolveTest262Harness(inc, path, options, &resolved)) {
        info->dropped.push_back(inc);
        continue;
      }
      std::string inc_source, read_error;
      if (!ReadFileToString(resolved, &inc_source, &read_error)) {
        info->dropped.push_back(inc);
        continue;
      }
      fast::ParseError inc_parse_error;
      std::unique_ptr<fast::Tree> loaded = fast::Parser::ParseScript(
          inc_source, fast::ParseOptions(), &inc_parse_error);
      if (loaded == nullptr) {
        info->dropped.push_back(inc);
        continue;
      }
      InlineLoads(loaded.get(), resolved, options, 1, info);
      for (const fast::Statement* stmt : loaded->root()->body) {
        prefix.push_back(tree->Clone(stmt));
      }
      info->inlined.push_back(resolved);
    }
    if (!prefix.empty()) {
      std::vector<fast::Statement*>& body = tree->root()->body;
      body.insert(body.begin(), prefix.begin(), prefix.end());
    }
  }
  return tree;
}

}  // namespace redline
