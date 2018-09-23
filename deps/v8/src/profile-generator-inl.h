// Copyright 2010 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PROFILE_GENERATOR_INL_H_
#define V8_PROFILE_GENERATOR_INL_H_

#include "src/profile-generator.h"

namespace v8 {
namespace internal {

CodeEntry::CodeEntry(Logger::LogEventsAndTags tag, const char* name,
                     const char* name_prefix, const char* resource_name,
                     int line_number, int column_number,
                     JITLineInfoTable* line_info, Address instruction_start)
    : bit_field_(TagField::encode(tag) |
                 BuiltinIdField::encode(Builtins::builtin_count)),
      name_prefix_(name_prefix),
      name_(name),
      resource_name_(resource_name),
      line_number_(line_number),
      column_number_(column_number),
      script_id_(v8::UnboundScript::kNoScriptId),
      position_(0),
      no_frame_ranges_(NULL),
      bailout_reason_(kEmptyBailoutReason),
      deopt_reason_(kNoDeoptReason),
      deopt_position_(SourcePosition::Unknown()),
      line_info_(line_info),
      instruction_start_(instruction_start) {}


bool CodeEntry::is_js_function_tag(Logger::LogEventsAndTags tag) {
  return tag == Logger::FUNCTION_TAG
      || tag == Logger::LAZY_COMPILE_TAG
      || tag == Logger::SCRIPT_TAG
      || tag == Logger::NATIVE_FUNCTION_TAG
      || tag == Logger::NATIVE_LAZY_COMPILE_TAG
      || tag == Logger::NATIVE_SCRIPT_TAG;
}


ProfileNode::ProfileNode(ProfileTree* tree, CodeEntry* entry)
    : tree_(tree),
      entry_(entry),
      self_ticks_(0),
      children_(CodeEntriesMatch),
      id_(tree->next_node_id()),
      line_ticks_(LineTickMatch) {}


inline unsigned ProfileNode::function_id() const {
  return tree_->GetFunctionId(this);
}
} }  // namespace v8::internal

#endif  // V8_PROFILE_GENERATOR_INL_H_
