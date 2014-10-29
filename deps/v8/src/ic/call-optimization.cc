// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/v8.h"

#include "src/ic/call-optimization.h"


namespace v8 {
namespace internal {

CallOptimization::CallOptimization(Handle<JSFunction> function) {
  Initialize(function);
}


Handle<JSObject> CallOptimization::LookupHolderOfExpectedType(
    Handle<Map> object_map, HolderLookup* holder_lookup) const {
  DCHECK(is_simple_api_call());
  if (!object_map->IsJSObjectMap()) {
    *holder_lookup = kHolderNotFound;
    return Handle<JSObject>::null();
  }
  if (expected_receiver_type_.is_null() ||
      expected_receiver_type_->IsTemplateFor(*object_map)) {
    *holder_lookup = kHolderIsReceiver;
    return Handle<JSObject>::null();
  }
  while (true) {
    if (!object_map->prototype()->IsJSObject()) break;
    Handle<JSObject> prototype(JSObject::cast(object_map->prototype()));
    if (!prototype->map()->is_hidden_prototype()) break;
    object_map = handle(prototype->map());
    if (expected_receiver_type_->IsTemplateFor(*object_map)) {
      *holder_lookup = kHolderFound;
      return prototype;
    }
  }
  *holder_lookup = kHolderNotFound;
  return Handle<JSObject>::null();
}


bool CallOptimization::IsCompatibleReceiver(Handle<Object> receiver,
                                            Handle<JSObject> holder) const {
  DCHECK(is_simple_api_call());
  if (!receiver->IsJSObject()) return false;
  Handle<Map> map(JSObject::cast(*receiver)->map());
  HolderLookup holder_lookup;
  Handle<JSObject> api_holder = LookupHolderOfExpectedType(map, &holder_lookup);
  switch (holder_lookup) {
    case kHolderNotFound:
      return false;
    case kHolderIsReceiver:
      return true;
    case kHolderFound:
      if (api_holder.is_identical_to(holder)) return true;
      // Check if holder is in prototype chain of api_holder.
      {
        JSObject* object = *api_holder;
        while (true) {
          Object* prototype = object->map()->prototype();
          if (!prototype->IsJSObject()) return false;
          if (prototype == *holder) return true;
          object = JSObject::cast(prototype);
        }
      }
      break;
  }
  UNREACHABLE();
  return false;
}


void CallOptimization::Initialize(Handle<JSFunction> function) {
  constant_function_ = Handle<JSFunction>::null();
  is_simple_api_call_ = false;
  expected_receiver_type_ = Handle<FunctionTemplateInfo>::null();
  api_call_info_ = Handle<CallHandlerInfo>::null();

  if (function.is_null() || !function->is_compiled()) return;

  constant_function_ = function;
  AnalyzePossibleApiFunction(function);
}


void CallOptimization::AnalyzePossibleApiFunction(Handle<JSFunction> function) {
  if (!function->shared()->IsApiFunction()) return;
  Handle<FunctionTemplateInfo> info(function->shared()->get_api_func_data());

  // Require a C++ callback.
  if (info->call_code()->IsUndefined()) return;
  api_call_info_ =
      Handle<CallHandlerInfo>(CallHandlerInfo::cast(info->call_code()));

  // Accept signatures that either have no restrictions at all or
  // only have restrictions on the receiver.
  if (!info->signature()->IsUndefined()) {
    Handle<SignatureInfo> signature =
        Handle<SignatureInfo>(SignatureInfo::cast(info->signature()));
    if (!signature->args()->IsUndefined()) return;
    if (!signature->receiver()->IsUndefined()) {
      expected_receiver_type_ = Handle<FunctionTemplateInfo>(
          FunctionTemplateInfo::cast(signature->receiver()));
    }
  }

  is_simple_api_call_ = true;
}
}
}  // namespace v8::internal
