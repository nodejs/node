// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/program.h"

#include "experimental/redline/fast/parser.h"
#include "experimental/redline/fast/printer.h"

namespace redline {

// static
uint64_t Program::HashSource(std::string_view source) {
  // FNV-1a 64.
  uint64_t h = 0xcbf29ce484222325ull;
  for (char ch : source) {
    const unsigned char c = static_cast<unsigned char>(ch);
    h ^= c;
    h *= 0x100000001b3ull;
  }
  return h;
}

// static
std::shared_ptr<Program> Program::FromTree(std::unique_ptr<fast::Tree> tree,
                                           Lineage lineage) {
  std::string source = fast::Print(tree->root());
  return FromTree(std::move(tree), std::move(lineage), std::move(source));
}

// static
std::shared_ptr<Program> Program::FromTree(std::unique_ptr<fast::Tree> tree,
                                           Lineage lineage,
                                           std::string source) {
  std::shared_ptr<Program> program(new Program());
  program->tree_ = std::move(tree);
  program->source_ = std::move(source);
  program->hash_ = HashSource(program->source_);
  program->node_count_ = fast::CountNodes(program->tree_->root());
  program->lineage_ = std::move(lineage);
  return program;
}

// static
std::shared_ptr<Program> Program::Parse(std::string_view source,
                                        Lineage lineage, std::string* error) {
  fast::ParseError parse_error;
  std::unique_ptr<fast::Tree> tree =
      fast::Parser::ParseScript(source, fast::ParseOptions(), &parse_error);
  if (tree == nullptr) {
    if (error != nullptr) {
      *error = parse_error.message;
    }
    return nullptr;
  }
  return FromTree(std::move(tree), std::move(lineage));
}

const fast::ScopeAnalysis& Program::scopes() const {
  std::call_once(scopes_once_, [this] {
    scopes_ = fast::ScopeAnalysis::Analyze(tree_.get());
  });
  return *scopes_;
}

}  // namespace redline
