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

bool ParseProgram(ParseInfo* info, Isolate* isolate, bool internalize) {
  DCHECK(info->is_toplevel());
  DCHECK_NULL(info->literal());

  Parser parser(info);

  FunctionLiteral* result = nullptr;
  // Ok to use Isolate here; this function is only called in the main thread.
  DCHECK(parser.parsing_on_main_thread_);

  parser.SetCachedData(info);
  result = parser.ParseProgram(isolate, info);
  info->set_literal(result);
  if (result == nullptr) {
    parser.ReportErrors(isolate, info->script());
  } else {
    info->set_language_mode(info->literal()->language_mode());
  }
  parser.UpdateStatistics(isolate, info->script());
  if (internalize) {
    info->ast_value_factory()->Internalize(isolate);
  }
  return (result != nullptr);
}

bool ParseFunction(ParseInfo* info, Isolate* isolate, bool internalize) {
  DCHECK(!info->is_toplevel());
  DCHECK_NULL(info->literal());

  Parser parser(info);

  FunctionLiteral* result = nullptr;
  // Ok to use Isolate here; this function is only called in the main thread.
  DCHECK(parser.parsing_on_main_thread_);

  result = parser.ParseFunction(isolate, info);
  info->set_literal(result);
  if (result == nullptr) {
    parser.ReportErrors(isolate, info->script());
  }
  parser.UpdateStatistics(isolate, info->script());
  if (internalize) {
    info->ast_value_factory()->Internalize(isolate);
  }
  return (result != nullptr);
}

bool ParseAny(ParseInfo* info, Isolate* isolate, bool internalize) {
  return info->is_toplevel() ? ParseProgram(info, isolate, internalize)
                             : ParseFunction(info, isolate, internalize);
}

}  // namespace parsing
}  // namespace internal
}  // namespace v8
