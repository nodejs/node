// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/unoptimized-compilation-info.h"

#include "src/ast/ast.h"
#include "src/ast/scopes.h"
#include "src/debug/debug.h"
#include "src/isolate.h"
#include "src/objects-inl.h"
#include "src/parsing/parse-info.h"
#include "src/source-position.h"

namespace v8 {
namespace internal {

UnoptimizedCompilationInfo::UnoptimizedCompilationInfo(Zone* zone,
                                                       ParseInfo* parse_info,
                                                       FunctionLiteral* literal)
    : flags_(FLAG_untrusted_code_mitigations ? kUntrustedCodeMitigations : 0),
      zone_(zone),
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

  if (parse_info->is_eval()) MarkAsEval();
  if (parse_info->is_native()) MarkAsNative();
  if (parse_info->collect_type_profile()) MarkAsCollectTypeProfile();
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
  return is_native() ? SourcePositionTableBuilder::OMIT_SOURCE_POSITIONS
                     : SourcePositionTableBuilder::RECORD_SOURCE_POSITIONS;
}

}  // namespace internal
}  // namespace v8
