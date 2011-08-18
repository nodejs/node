// Copyright 2011 the V8 project authors. All rights reserved.
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
#include "scopes.h"

namespace v8 {
namespace internal {


SwitchStatement::SwitchStatement(Isolate* isolate,
                                 ZoneStringList* labels)
    : BreakableStatement(isolate, labels, TARGET_FOR_ANONYMOUS),
      tag_(NULL), cases_(NULL) {
}


Block::Block(Isolate* isolate,
             ZoneStringList* labels,
             int capacity,
             bool is_initializer_block)
    : BreakableStatement(isolate, labels, TARGET_FOR_NAMED_ONLY),
      statements_(capacity),
      is_initializer_block_(is_initializer_block),
      block_scope_(NULL) {
}


BreakableStatement::BreakableStatement(Isolate* isolate,
                                       ZoneStringList* labels,
                                       Type type)
    : labels_(labels),
      type_(type),
      entry_id_(GetNextId(isolate)),
      exit_id_(GetNextId(isolate)) {
  ASSERT(labels == NULL || labels->length() > 0);
}


IterationStatement::IterationStatement(Isolate* isolate, ZoneStringList* labels)
    : BreakableStatement(isolate, labels, TARGET_FOR_ANONYMOUS),
      body_(NULL),
      continue_target_(),
      osr_entry_id_(GetNextId(isolate)) {
}


DoWhileStatement::DoWhileStatement(Isolate* isolate, ZoneStringList* labels)
    : IterationStatement(isolate, labels),
      cond_(NULL),
      condition_position_(-1),
      continue_id_(GetNextId(isolate)),
      back_edge_id_(GetNextId(isolate)) {
}


WhileStatement::WhileStatement(Isolate* isolate, ZoneStringList* labels)
    : IterationStatement(isolate, labels),
      cond_(NULL),
      may_have_function_literal_(true),
      body_id_(GetNextId(isolate)) {
}


ForStatement::ForStatement(Isolate* isolate, ZoneStringList* labels)
    : IterationStatement(isolate, labels),
      init_(NULL),
      cond_(NULL),
      next_(NULL),
      may_have_function_literal_(true),
      loop_variable_(NULL),
      continue_id_(GetNextId(isolate)),
      body_id_(GetNextId(isolate)) {
}


ForInStatement::ForInStatement(Isolate* isolate, ZoneStringList* labels)
    : IterationStatement(isolate, labels),
      each_(NULL),
      enumerable_(NULL),
      assignment_id_(GetNextId(isolate)) {
}


bool FunctionLiteral::strict_mode() const {
  return scope()->is_strict_mode();
}


} }  // namespace v8::internal

#endif  // V8_AST_INL_H_
