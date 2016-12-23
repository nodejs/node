// Copyright 2013 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRANKSHAFT_TYPING_H_
#define V8_CRANKSHAFT_TYPING_H_

#include "src/allocation.h"
#include "src/ast/ast-type-bounds.h"
#include "src/ast/ast-types.h"
#include "src/ast/ast.h"
#include "src/ast/variables.h"
#include "src/effects.h"
#include "src/type-info.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class DeclarationScope;
class Isolate;
class FunctionLiteral;

class AstTyper final : public AstVisitor<AstTyper> {
 public:
  AstTyper(Isolate* isolate, Zone* zone, Handle<JSFunction> closure,
           DeclarationScope* scope, BailoutId osr_ast_id, FunctionLiteral* root,
           AstTypeBounds* bounds);
  void Run();

  DEFINE_AST_VISITOR_SUBCLASS_MEMBERS();

 private:
  Effect ObservedOnStack(Object* value);
  void ObserveTypesAtOsrEntry(IterationStatement* stmt);

  static const int kNoVar = INT_MIN;
  typedef v8::internal::Effects<int, kNoVar> Effects;
  typedef v8::internal::NestedEffects<int, kNoVar> Store;

  Isolate* isolate_;
  Zone* zone_;
  Handle<JSFunction> closure_;
  DeclarationScope* scope_;
  BailoutId osr_ast_id_;
  FunctionLiteral* root_;
  TypeFeedbackOracle oracle_;
  Store store_;
  AstTypeBounds* bounds_;

  Zone* zone() const { return zone_; }
  TypeFeedbackOracle* oracle() { return &oracle_; }

  void NarrowType(Expression* e, AstBounds b) {
    bounds_->set(e, AstBounds::Both(bounds_->get(e), b, zone()));
  }
  void NarrowLowerType(Expression* e, AstType* t) {
    bounds_->set(e, AstBounds::NarrowLower(bounds_->get(e), t, zone()));
  }

  Effects EnterEffects() {
    store_ = store_.Push();
    return store_.Top();
  }
  void ExitEffects() { store_ = store_.Pop(); }

  int parameter_index(int index) { return -index - 2; }
  int stack_local_index(int index) { return index; }

  int variable_index(Variable* var);

  void VisitDeclarations(ZoneList<Declaration*>* declarations);
  void VisitStatements(ZoneList<Statement*>* statements);

#define DECLARE_VISIT(type) void Visit##type(type* node);
  AST_NODE_LIST(DECLARE_VISIT)
#undef DECLARE_VISIT

  DISALLOW_COPY_AND_ASSIGN(AstTyper);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_CRANKSHAFT_TYPING_H_
