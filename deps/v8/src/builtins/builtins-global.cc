// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/code-stub-assembler.h"
#include "src/compiler.h"
#include "src/uri.h"

namespace v8 {
namespace internal {

// ES6 section 18.2.6.2 decodeURI (encodedURI)
BUILTIN(GlobalDecodeURI) {
  HandleScope scope(isolate);
  Handle<String> encoded_uri;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, encoded_uri,
      Object::ToString(isolate, args.atOrUndefined(isolate, 1)));

  RETURN_RESULT_OR_FAILURE(isolate, Uri::DecodeUri(isolate, encoded_uri));
}

// ES6 section 18.2.6.3 decodeURIComponent (encodedURIComponent)
BUILTIN(GlobalDecodeURIComponent) {
  HandleScope scope(isolate);
  Handle<String> encoded_uri_component;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, encoded_uri_component,
      Object::ToString(isolate, args.atOrUndefined(isolate, 1)));

  RETURN_RESULT_OR_FAILURE(
      isolate, Uri::DecodeUriComponent(isolate, encoded_uri_component));
}

// ES6 section 18.2.6.4 encodeURI (uri)
BUILTIN(GlobalEncodeURI) {
  HandleScope scope(isolate);
  Handle<String> uri;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, uri, Object::ToString(isolate, args.atOrUndefined(isolate, 1)));

  RETURN_RESULT_OR_FAILURE(isolate, Uri::EncodeUri(isolate, uri));
}

// ES6 section 18.2.6.5 encodeURIComponenet (uriComponent)
BUILTIN(GlobalEncodeURIComponent) {
  HandleScope scope(isolate);
  Handle<String> uri_component;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, uri_component,
      Object::ToString(isolate, args.atOrUndefined(isolate, 1)));

  RETURN_RESULT_OR_FAILURE(isolate,
                           Uri::EncodeUriComponent(isolate, uri_component));
}

// ES6 section B.2.1.1 escape (string)
BUILTIN(GlobalEscape) {
  HandleScope scope(isolate);
  Handle<String> string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, string,
      Object::ToString(isolate, args.atOrUndefined(isolate, 1)));

  RETURN_RESULT_OR_FAILURE(isolate, Uri::Escape(isolate, string));
}

// ES6 section B.2.1.2 unescape (string)
BUILTIN(GlobalUnescape) {
  HandleScope scope(isolate);
  Handle<String> string;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, string,
      Object::ToString(isolate, args.atOrUndefined(isolate, 1)));

  RETURN_RESULT_OR_FAILURE(isolate, Uri::Unescape(isolate, string));
}

// ES6 section 18.2.1 eval (x)
BUILTIN(GlobalEval) {
  HandleScope scope(isolate);
  Handle<Object> x = args.atOrUndefined(isolate, 1);
  Handle<JSFunction> target = args.target();
  Handle<JSObject> target_global_proxy(target->global_proxy(), isolate);
  if (!x->IsString()) return *x;
  if (!Builtins::AllowDynamicFunction(isolate, target, target_global_proxy)) {
    isolate->CountUsage(v8::Isolate::kFunctionConstructorReturnedUndefined);
    return isolate->heap()->undefined_value();
  }
  Handle<JSFunction> function;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, function, Compiler::GetFunctionFromString(
                             handle(target->native_context(), isolate),
                             Handle<String>::cast(x), NO_PARSE_RESTRICTION));
  RETURN_RESULT_OR_FAILURE(
      isolate,
      Execution::Call(isolate, function, target_global_proxy, 0, nullptr));
}

// ES6 section 18.2.2 isFinite ( number )
void Builtins::Generate_GlobalIsFinite(compiler::CodeAssemblerState* state) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;
  CodeStubAssembler assembler(state);

  Node* context = assembler.Parameter(4);

  Label return_true(&assembler), return_false(&assembler);

  // We might need to loop once for ToNumber conversion.
  Variable var_num(&assembler, MachineRepresentation::kTagged);
  Label loop(&assembler, &var_num);
  var_num.Bind(assembler.Parameter(1));
  assembler.Goto(&loop);
  assembler.Bind(&loop);
  {
    // Load the current {num} value.
    Node* num = var_num.value();

    // Check if {num} is a Smi or a HeapObject.
    assembler.GotoIf(assembler.TaggedIsSmi(num), &return_true);

    // Check if {num} is a HeapNumber.
    Label if_numisheapnumber(&assembler),
        if_numisnotheapnumber(&assembler, Label::kDeferred);
    assembler.Branch(assembler.IsHeapNumberMap(assembler.LoadMap(num)),
                     &if_numisheapnumber, &if_numisnotheapnumber);

    assembler.Bind(&if_numisheapnumber);
    {
      // Check if {num} contains a finite, non-NaN value.
      Node* num_value = assembler.LoadHeapNumberValue(num);
      assembler.BranchIfFloat64IsNaN(assembler.Float64Sub(num_value, num_value),
                                     &return_false, &return_true);
    }

    assembler.Bind(&if_numisnotheapnumber);
    {
      // Need to convert {num} to a Number first.
      Callable callable = CodeFactory::NonNumberToNumber(assembler.isolate());
      var_num.Bind(assembler.CallStub(callable, context, num));
      assembler.Goto(&loop);
    }
  }

  assembler.Bind(&return_true);
  assembler.Return(assembler.BooleanConstant(true));

  assembler.Bind(&return_false);
  assembler.Return(assembler.BooleanConstant(false));
}

// ES6 section 18.2.3 isNaN ( number )
void Builtins::Generate_GlobalIsNaN(compiler::CodeAssemblerState* state) {
  typedef CodeStubAssembler::Label Label;
  typedef compiler::Node Node;
  typedef CodeStubAssembler::Variable Variable;
  CodeStubAssembler assembler(state);

  Node* context = assembler.Parameter(4);

  Label return_true(&assembler), return_false(&assembler);

  // We might need to loop once for ToNumber conversion.
  Variable var_num(&assembler, MachineRepresentation::kTagged);
  Label loop(&assembler, &var_num);
  var_num.Bind(assembler.Parameter(1));
  assembler.Goto(&loop);
  assembler.Bind(&loop);
  {
    // Load the current {num} value.
    Node* num = var_num.value();

    // Check if {num} is a Smi or a HeapObject.
    assembler.GotoIf(assembler.TaggedIsSmi(num), &return_false);

    // Check if {num} is a HeapNumber.
    Label if_numisheapnumber(&assembler),
        if_numisnotheapnumber(&assembler, Label::kDeferred);
    assembler.Branch(assembler.IsHeapNumberMap(assembler.LoadMap(num)),
                     &if_numisheapnumber, &if_numisnotheapnumber);

    assembler.Bind(&if_numisheapnumber);
    {
      // Check if {num} contains a NaN.
      Node* num_value = assembler.LoadHeapNumberValue(num);
      assembler.BranchIfFloat64IsNaN(num_value, &return_true, &return_false);
    }

    assembler.Bind(&if_numisnotheapnumber);
    {
      // Need to convert {num} to a Number first.
      Callable callable = CodeFactory::NonNumberToNumber(assembler.isolate());
      var_num.Bind(assembler.CallStub(callable, context, num));
      assembler.Goto(&loop);
    }
  }

  assembler.Bind(&return_true);
  assembler.Return(assembler.BooleanConstant(true));

  assembler.Bind(&return_false);
  assembler.Return(assembler.BooleanConstant(false));
}

}  // namespace internal
}  // namespace v8
