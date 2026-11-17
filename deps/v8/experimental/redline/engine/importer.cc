// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/engine/importer.h"

#include <cstring>

#include "experimental/redline/base/file_util.h"
#include "experimental/redline/base/log.h"
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
    // The mjsunit harness is provided by the execution prelude.
    if (EndsWith(file, "mjsunit.js")) {
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

}  // namespace

std::unique_ptr<fast::Tree> ImportSource(std::string_view source,
                                         const std::string& path,
                                         const ImportOptions& options,
                                         ImportInfo* info, std::string* error) {
  fast::ParseError parse_error;
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(source, fast::ParseOptions(), &parse_error);
  if (tree == nullptr) {
    *error = parse_error.ToString();
    return nullptr;
  }
  InlineLoads(tree.get(), path, options, 0, info);
  return tree;
}

}  // namespace redline
