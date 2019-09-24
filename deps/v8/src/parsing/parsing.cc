// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/parsing/parsing.h"

#include <memory>

#include "src/ast/ast.h"
#include "src/execution/vm-state-inl.h"
#include "src/objects/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/parsing/parser.h"
#include "src/parsing/scanner-character-streams.h"
#include "src/zone/zone-list-inl.h"  // crbug.com/v8/8816

namespace v8 {
namespace internal {
namespace parsing {

bool ParseProgram(ParseInfo* info, Isolate* isolate,
                  ReportErrorsAndStatisticsMode mode) {
  DCHECK(info->is_toplevel());
  DCHECK_NULL(info->literal());

  VMState<PARSER> state(isolate);

  // Create a character stream for the parser.
  Handle<String> source(String::cast(info->script()->source()), isolate);
  isolate->counters()->total_parse_size()->Increment(source->length());
  std::unique_ptr<Utf16CharacterStream> stream(
      ScannerStream::For(isolate, source));
  info->set_character_stream(std::move(stream));

  Parser parser(info);

  FunctionLiteral* result = nullptr;
  // Ok to use Isolate here; this function is only called in the main thread.
  DCHECK(parser.parsing_on_main_thread_);

  result = parser.ParseProgram(isolate, info);
  info->set_literal(result);
  if (result) {
    info->set_language_mode(info->literal()->language_mode());
    if (info->is_eval()) {
      info->set_allow_eval_cache(parser.allow_eval_cache());
    }
  }

  if (mode == ReportErrorsAndStatisticsMode::kYes) {
    if (result == nullptr) {
      info->pending_error_handler()->ReportErrors(isolate, info->script(),
                                                  info->ast_value_factory());
    }
    parser.UpdateStatistics(isolate, info->script());
  }
  return (result != nullptr);
}

bool ParseFunction(ParseInfo* info, Handle<SharedFunctionInfo> shared_info,
                   Isolate* isolate, ReportErrorsAndStatisticsMode mode) {
  DCHECK(!info->is_toplevel());
  DCHECK(!shared_info.is_null());
  DCHECK_NULL(info->literal());

  // Create a character stream for the parser.
  Handle<String> source(String::cast(info->script()->source()), isolate);
  isolate->counters()->total_parse_size()->Increment(source->length());
  std::unique_ptr<Utf16CharacterStream> stream(
      ScannerStream::For(isolate, source, shared_info->StartPosition(),
                         shared_info->EndPosition()));
  info->set_character_stream(std::move(stream));

  VMState<PARSER> state(isolate);

  Parser parser(info);

  FunctionLiteral* result = nullptr;
  // Ok to use Isolate here; this function is only called in the main thread.
  DCHECK(parser.parsing_on_main_thread_);

  result = parser.ParseFunction(isolate, info, shared_info);
  info->set_literal(result);
  if (result) {
    info->ast_value_factory()->Internalize(isolate);
    if (info->is_eval()) {
      info->set_allow_eval_cache(parser.allow_eval_cache());
    }
  }

  if (mode == ReportErrorsAndStatisticsMode::kYes) {
    if (result == nullptr) {
      info->pending_error_handler()->ReportErrors(isolate, info->script(),
                                                  info->ast_value_factory());
    }
    parser.UpdateStatistics(isolate, info->script());
  }
  return (result != nullptr);
}

bool ParseAny(ParseInfo* info, Handle<SharedFunctionInfo> shared_info,
              Isolate* isolate, ReportErrorsAndStatisticsMode mode) {
  DCHECK(!shared_info.is_null());
  return info->is_toplevel() ? ParseProgram(info, isolate, mode)
                             : ParseFunction(info, shared_info, isolate, mode);
}

}  // namespace parsing
}  // namespace internal
}  // namespace v8
