// Copyright 2006-2009 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_FUNC_NAME_INFERRER_H_
#define V8_FUNC_NAME_INFERRER_H_

#include "handles.h"
#include "zone.h"

namespace v8 {
namespace internal {

class FunctionLiteral;
class Isolate;

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
  FuncNameInferrer(Isolate* isolate, Zone* zone);

  // Returns whether we have entered name collection state.
  bool IsOpen() const { return !entries_stack_.is_empty(); }

  // Pushes an enclosing the name of enclosing function onto names stack.
  void PushEnclosingName(Handle<String> name);

  // Enters name collection state.
  void Enter() {
    entries_stack_.Add(names_stack_.length(), zone());
  }

  // Pushes an encountered name onto names stack when in collection state.
  void PushLiteralName(Handle<String> name);

  void PushVariableName(Handle<String> name);

  // Adds a function to infer name for.
  void AddFunction(FunctionLiteral* func_to_infer) {
    if (IsOpen()) {
      funcs_to_infer_.Add(func_to_infer, zone());
    }
  }

  void RemoveLastFunction() {
    if (IsOpen() && !funcs_to_infer_.is_empty()) {
      funcs_to_infer_.RemoveLast();
    }
  }

  // Infers a function name and leaves names collection state.
  void Infer() {
    ASSERT(IsOpen());
    if (!funcs_to_infer_.is_empty()) {
      InferFunctionsNames();
    }
  }

  // Leaves names collection state.
  void Leave() {
    ASSERT(IsOpen());
    names_stack_.Rewind(entries_stack_.RemoveLast());
    if (entries_stack_.is_empty())
      funcs_to_infer_.Clear();
  }

 private:
  enum NameType {
    kEnclosingConstructorName,
    kLiteralName,
    kVariableName
  };
  struct Name {
    Name(Handle<String> name, NameType type) : name(name), type(type) { }
    Handle<String> name;
    NameType type;
  };

  Isolate* isolate() { return isolate_; }
  Zone* zone() const { return zone_; }

  // Constructs a full name in dotted notation from gathered names.
  Handle<String> MakeNameFromStack();

  // A helper function for MakeNameFromStack.
  Handle<String> MakeNameFromStackHelper(int pos, Handle<String> prev);

  // Performs name inferring for added functions.
  void InferFunctionsNames();

  Isolate* isolate_;
  ZoneList<int> entries_stack_;
  ZoneList<Name> names_stack_;
  ZoneList<FunctionLiteral*> funcs_to_infer_;
  Zone* zone_;

  DISALLOW_COPY_AND_ASSIGN(FuncNameInferrer);
};


} }  // namespace v8::internal

#endif  // V8_FUNC_NAME_INFERRER_H_
