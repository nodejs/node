// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/runtime/runtime-utils.h"

#include "src/arguments.h"
#include "src/char-predicates-inl.h"
#include "src/isolate-inl.h"
#include "src/json-stringifier.h"
#include "src/objects-inl.h"
#include "src/parsing/json-parser.h"

namespace v8 {
namespace internal {

RUNTIME_FUNCTION(Runtime_QuoteJSONString) {
  HandleScope scope(isolate);
  CONVERT_ARG_HANDLE_CHECKED(String, string, 0);
  DCHECK(args.length() == 1);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(
      isolate, result, BasicJsonStringifier::StringifyString(isolate, string));
  return *result;
}


RUNTIME_FUNCTION(Runtime_BasicJSONStringify) {
  HandleScope scope(isolate);
  DCHECK(args.length() == 1);
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  BasicJsonStringifier stringifier(isolate);
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     stringifier.Stringify(object));
  return *result;
}


RUNTIME_FUNCTION(Runtime_ParseJson) {
  HandleScope scope(isolate);
  DCHECK_EQ(1, args.length());
  CONVERT_ARG_HANDLE_CHECKED(Object, object, 0);
  Handle<String> source;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, source,
                                     Object::ToString(isolate, object));
  source = String::Flatten(source);
  // Optimized fast case where we only have Latin1 characters.
  Handle<Object> result;
  ASSIGN_RETURN_FAILURE_ON_EXCEPTION(isolate, result,
                                     source->IsSeqOneByteString()
                                         ? JsonParser<true>::Parse(source)
                                         : JsonParser<false>::Parse(source));
  return *result;
}

}  // namespace internal
}  // namespace v8
