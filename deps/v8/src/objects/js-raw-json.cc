// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/js-raw-json.h"

#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/json/json-parser.h"
#include "src/objects/js-raw-json-inl.h"
#include "src/objects/string-inl.h"

namespace v8 {
namespace internal {

// https://tc39.es/proposal-json-parse-with-source/#sec-json.rawjson
MaybeDirectHandle<JSRawJson> JSRawJson::Create(Isolate* isolate,
                                               Handle<Object> text) {
  Handle<String> json_string;
  ASSIGN_RETURN_ON_EXCEPTION(isolate, json_string,
                             Object::ToString(isolate, text));
  Handle<String> flat = String::Flatten(isolate, json_string);
  if (String::IsOneByteRepresentationUnderneath(*flat)) {
    if (!JsonParser<uint8_t>::CheckRawJson(isolate, flat)) {
      DCHECK(isolate->has_exception());
      return MaybeDirectHandle<JSRawJson>();
    }
  } else {
    if (!JsonParser<uint16_t>::CheckRawJson(isolate, flat)) {
      DCHECK(isolate->has_exception());
      return MaybeDirectHandle<JSRawJson>();
    }
  }
  DirectHandle<JSObject> result =
      isolate->factory()->NewJSObjectFromMap(isolate->js_raw_json_map());
  result->InObjectPropertyAtPut(JSRawJson::kRawJsonInitialIndex, *flat);
  JSObject::SetIntegrityLevel(isolate, result, FROZEN, kThrowOnError).Check();
  return Cast<JSRawJson>(result);
}

}  // namespace internal
}  // namespace v8
