// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_PARSING_FUNC_NAME_INFERRER_H_
#define V8_PARSING_FUNC_NAME_INFERRER_H_

#include "src/zone/zone-chunk-list.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {

class AstConsString;
class AstRawString;
class AstValueFactory;
class FunctionLiteral;

enum class InferName { kYes, kNo };

// FuncNameInferrer is a stateful class that is used to perform name
// inference for anonymous functions during static analysis of source code.
// Inference is performed in cases when an anonymous function is assigned
// to a variable or a property (see test-func-name-inference.cc for examples.)
//
// The basic idea is that during parsing of LHSs of certain expressions
// (assignments, declarations, object literals) we collect name strings,
// and during parsing of the RHS, a function literal can be collected. After
// parsing the RHS we can infer a name for function literals that do not have
// a name.
class FuncNameInferrer : public ZoneObject {
 public:
  FuncNameInferrer(AstValueFactory* ast_value_factory, Zone* zone);

  // To enter function name inference state, put a FuncNameInferrer::State
  // on the stack.
  class State {
   public:
    explicit State(FuncNameInferrer* fni) : fni_(fni) { fni_->Enter(); }
    ~State() { fni_->Leave(); }

   private:
    FuncNameInferrer* fni_;

    DISALLOW_COPY_AND_ASSIGN(State);
  };

  // Returns whether we have entered name collection state.
  bool IsOpen() const { return !entries_stack_.is_empty(); }

  // Pushes an enclosing the name of enclosing function onto names stack.
  void PushEnclosingName(const AstRawString* name);

  // Pushes an encountered name onto names stack when in collection state.
  void PushLiteralName(const AstRawString* name);

  void PushVariableName(const AstRawString* name);

  // Adds a function to infer name for.
  void AddFunction(FunctionLiteral* func_to_infer) {
    if (IsOpen()) {
      funcs_to_infer_.push_back(func_to_infer);
    }
  }

  void RemoveLastFunction() {
    if (IsOpen() && !funcs_to_infer_.is_empty()) {
      funcs_to_infer_.pop_back();
    }
  }

  void RemoveAsyncKeywordFromEnd();

  // Infers a function name and leaves names collection state.
  void Infer() {
    DCHECK(IsOpen());
    if (!funcs_to_infer_.is_empty()) {
      InferFunctionsNames();
    }
  }

 private:
  enum NameType {
    kEnclosingConstructorName,
    kLiteralName,
    kVariableName
  };
  struct Name {
    Name(const AstRawString* name, NameType type) : name(name), type(type) {}
    const AstRawString* name;
    NameType type;
  };

  void Enter() { entries_stack_.push_back(names_stack_.size()); }

  void Leave();

  Zone* zone() const { return zone_; }

  // Constructs a full name in dotted notation from gathered names.
  const AstConsString* MakeNameFromStack();

  // Performs name inferring for added functions.
  void InferFunctionsNames();

  AstValueFactory* ast_value_factory_;
  ZoneChunkList<size_t> entries_stack_;
  ZoneChunkList<Name> names_stack_;
  ZoneChunkList<FunctionLiteral*> funcs_to_infer_;
  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(FuncNameInferrer);
};


}  // namespace internal
}  // namespace v8

#endif  // V8_PARSING_FUNC_NAME_INFERRER_H_
