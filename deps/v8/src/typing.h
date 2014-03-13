// Copyright 2013 the V8 project authors. All rights reserved.
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

#ifndef V8_TYPING_H_
#define V8_TYPING_H_

#include "v8.h"

#include "allocation.h"
#include "ast.h"
#include "compiler.h"
#include "type-info.h"
#include "types.h"
#include "effects.h"
#include "zone.h"
#include "scopes.h"

namespace v8 {
namespace internal {


class AstTyper: public AstVisitor {
 public:
  static void Run(CompilationInfo* info);

  void* operator new(size_t size, Zone* zone) {
    return zone->New(static_cast<int>(size));
  }
  void operator delete(void* pointer, Zone* zone) { }
  void operator delete(void* pointer) { }

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  explicit AstTyper(CompilationInfo* info);

  Effect ObservedOnStack(Object* value);
  void ObserveTypesAtOsrEntry(IterationStatement* stmt);

  static const int kNoVar = INT_MIN;
  typedef v8::internal::Effects<int, kNoVar> Effects;
  typedef v8::internal::NestedEffects<int, kNoVar> Store;

  CompilationInfo* info_;
  TypeFeedbackOracle oracle_;
  Store store_;

  TypeFeedbackOracle* oracle() { return &oracle_; }

  void NarrowType(Expression* e, Bounds b) {
    e->set_bounds(Bounds::Both(e->bounds(), b, zone()));
  }
  void NarrowLowerType(Expression* e, Type* t) {
    e->set_bounds(Bounds::NarrowLower(e->bounds(), t, zone()));
  }

  Effects EnterEffects() {
    store_ = store_.Push();
    return store_.Top();
  }
  void ExitEffects() { store_ = store_.Pop(); }

  int parameter_index(int index) { return -index - 2; }
  int stack_local_index(int index) { return index; }

  int variable_index(Variable* var) {
    // Stack locals have the range [0 .. l]
    // Parameters have the range [-1 .. p]
    // We map this to [-p-2 .. -1, 0 .. l]
    return var->IsStackLocal() ? stack_local_index(var->index()) :
           var->IsParameter() ? parameter_index(var->index()) : kNoVar;
  }

  void VisitDeclarations(ZoneList<Declaration*>* declarations);
  void VisitStatements(ZoneList<Statement*>* statements);

#define DECLARE_VISIT(type) virtual void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  DISALLOW_COPY_AND_ASSIGN(AstTyper);
};

} }  // namespace v8::internal

#endif  // V8_TYPING_H_
