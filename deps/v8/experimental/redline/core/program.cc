// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "experimental/redline/core/program.h"

#include "experimental/redline/base/log.h"
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
  program->tree_ptr_.store(program->tree_.get(), std::memory_order_release);
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

const fast::Tree& Program::EnsureTree() const {
  std::scoped_lock<std::mutex> lock(tree_mutex_);
  if (tree_ == nullptr) {
    fast::ParseError parse_error;
    tree_ =
        fast::Parser::ParseScript(source_, fast::ParseOptions(), &parse_error);
    if (tree_ == nullptr) {
      Fatal("Program::EnsureTree: failed to parse stored source: %s",
            parse_error.message.c_str());
    }
    tree_ptr_.store(tree_.get(), std::memory_order_release);
  }
  return *tree_;
}

const fast::Tree& Program::tree() const {
  if (const fast::Tree* cached = tree_ptr_.load(std::memory_order_acquire)) {
    return *cached;
  }
  return EnsureTree();
}

std::unique_ptr<fast::Tree> Program::CopyTree() const { return tree().Copy(); }

const fast::ScopeAnalysis& Program::scopes() const {
  if (const fast::ScopeAnalysis* cached =
          scopes_ptr_.load(std::memory_order_acquire)) {
    return *cached;
  }
  std::scoped_lock<std::mutex> lock(tree_mutex_);
  if (tree_ == nullptr) {
    fast::ParseError parse_error;
    tree_ =
        fast::Parser::ParseScript(source_, fast::ParseOptions(), &parse_error);
    if (tree_ == nullptr) {
      Fatal("Program::scopes: failed to parse stored source: %s",
            parse_error.message.c_str());
    }
    tree_ptr_.store(tree_.get(), std::memory_order_release);
  }
  if (scopes_ == nullptr) {
    scopes_ = fast::ScopeAnalysis::Analyze(tree_.get());
    scopes_ptr_.store(scopes_.get(), std::memory_order_release);
  }
  return *scopes_;
}

void Program::ReleaseTree() const {
  std::scoped_lock<std::mutex> lock(tree_mutex_);
  scopes_ptr_.store(nullptr, std::memory_order_release);
  scopes_.reset();
  tree_ptr_.store(nullptr, std::memory_order_release);
  tree_.reset();
}

}  // namespace redline
