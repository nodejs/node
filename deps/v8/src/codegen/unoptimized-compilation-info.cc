// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/codegen/unoptimized-compilation-info.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/codegen/source-position.h"
#include "src/debug/debug.h"
#include "src/execution/isolate.h"
#include "src/objects/objects-inl.h"
#include "src/parsing/parse-info.h"

namespace v8 {
namespace internal {

UnoptimizedCompilationInfo::UnoptimizedCompilationInfo(Zone* zone,
                                                       ParseInfo* parse_info,
                                                       FunctionLiteral* literal)
    : flags_(parse_info->flags()),
      dispatcher_(parse_info->dispatcher()),
      character_stream_(parse_info->character_stream()),
      feedback_vector_spec_(zone) {
  // NOTE: The parse_info passed here represents the global information gathered
  // during parsing, but does not represent specific details of the actual
  // function literal being compiled for this OptimizedCompilationInfo. As such,
  // parse_info->literal() might be different from literal, and only global
  // details of the script being parsed are relevant to this
  // OptimizedCompilationInfo.
  DCHECK_NOT_NULL(literal);
  literal_ = literal;
  source_range_map_ = parse_info->source_range_map();
}

DeclarationScope* UnoptimizedCompilationInfo::scope() const {
  DCHECK_NOT_NULL(literal_);
  return literal_->scope();
}

int UnoptimizedCompilationInfo::num_parameters() const {
  return scope()->num_parameters();
}

int UnoptimizedCompilationInfo::num_parameters_including_this() const {
  return scope()->num_parameters() + 1;
}

SourcePositionTableBuilder::RecordingMode
UnoptimizedCompilationInfo::SourcePositionRecordingMode() const {
  if (flags().collect_source_positions()) {
    return SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS;
  }

  // Always collect source positions for functions that cannot be lazily
  // compiled, e.g. class member initializer functions.
  return !literal_->AllowsLazyCompilation()
             ? SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS
             : SourcePositionTableBuilder::LAZY_SOURCE_POSITIONS;
}

}  // namespace internal
}  // namespace v8
