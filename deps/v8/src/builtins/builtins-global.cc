// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"
#include "src/code-factory.h"
#include "src/compiler.h"
#include "src/counters.h"
#include "src/objects-inl.h"
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
      isolate, function,
      Compiler::GetFunctionFromString(handle(target->native_context(), isolate),
                                      Handle<String>::cast(x),
                                      NO_PARSE_RESTRICTION, kNoSourcePosition));
  RETURN_RESULT_OR_FAILURE(
      isolate,
      Execution::Call(isolate, function, target_global_proxy, 0, nullptr));
}

}  // namespace internal
}  // namespace v8
