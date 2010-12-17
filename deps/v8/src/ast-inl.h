// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_AST_INL_H_
#define V8_AST_INL_H_

#include "v8.h"

#include "ast.h"
#include "jump-target-inl.h"

namespace v8 {
namespace internal {


SwitchStatement::SwitchStatement(ZoneStringList* labels)
    : BreakableStatement(labels, TARGET_FOR_ANONYMOUS),
      tag_(NULL), cases_(NULL) {
}


Block::Block(ZoneStringList* labels, int capacity, bool is_initializer_block)
    : BreakableStatement(labels, TARGET_FOR_NAMED_ONLY),
      statements_(capacity),
      is_initializer_block_(is_initializer_block) {
}


BreakableStatement::BreakableStatement(ZoneStringList* labels, Type type)
    : labels_(labels),
      type_(type),
      entry_id_(GetNextId()),
      exit_id_(GetNextId()) {
  ASSERT(labels == NULL || labels->length() > 0);
}


IterationStatement::IterationStatement(ZoneStringList* labels)
    : BreakableStatement(labels, TARGET_FOR_ANONYMOUS),
      body_(NULL),
      continue_target_(JumpTarget::BIDIRECTIONAL),
      osr_entry_id_(GetNextId()) {
}


DoWhileStatement::DoWhileStatement(ZoneStringList* labels)
    : IterationStatement(labels),
      cond_(NULL),
      condition_position_(-1),
      continue_id_(GetNextId()),
      back_edge_id_(GetNextId()) {
}


WhileStatement::WhileStatement(ZoneStringList* labels)
    : IterationStatement(labels),
      cond_(NULL),
      may_have_function_literal_(true),
      body_id_(GetNextId()) {
}


ForStatement::ForStatement(ZoneStringList* labels)
    : IterationStatement(labels),
      init_(NULL),
      cond_(NULL),
      next_(NULL),
      may_have_function_literal_(true),
      loop_variable_(NULL),
      continue_id_(GetNextId()),
      body_id_(GetNextId()) {
}


ForInStatement::ForInStatement(ZoneStringList* labels)
    : IterationStatement(labels), each_(NULL), enumerable_(NULL),
      assignment_id_(GetNextId()) {
}


} }  // namespace v8::internal

#endif  // V8_AST_INL_H_
