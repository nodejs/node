// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parsing.h"

#include <memory>

#include "src/ast/ast.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"

namespace v8 {
namespace internal {
namespace parsing {

bool ParseProgram(ParseInfo* info) {
  DCHECK(info->is_toplevel());
  DCHECK_NULL(info->literal());

  Parser parser(info);

  FunctionLiteral* result = nullptr;
  // Ok to use Isolate here; this function is only called in the main thread.
  DCHECK(parser.parsing_on_main_thread_);
  Isolate* isolate = info->isolate();

  parser.SetCachedData(info);
  result = parser.ParseProgram(isolate, info);
  info->set_literal(result);
  parser.Internalize(isolate, info->script(), result == nullptr);
  if (result != nullptr) {
    info->set_language_mode(info->literal()->language_mode());
  }
  return (result != nullptr);
}

bool ParseFunction(ParseInfo* info) {
  DCHECK(!info->is_toplevel());
  DCHECK_NULL(info->literal());

  Parser parser(info);

  FunctionLiteral* result = nullptr;
  // Ok to use Isolate here; this function is only called in the main thread.
  DCHECK(parser.parsing_on_main_thread_);
  Isolate* isolate = info->isolate();

  result = parser.ParseFunction(isolate, info);
  info->set_literal(result);
  parser.Internalize(isolate, info->script(), result == nullptr);
  return (result != nullptr);
}

bool ParseAny(ParseInfo* info) {
  return info->is_toplevel() ? ParseProgram(info) : ParseFunction(info);
}

}  // namespace parsing
}  // namespace internal
}  // namespace v8
