// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/objects/templates.h"

#include "src/api/api-inl.h"
#include "src/execution/isolate.h"
#include "src/heap/factory.h"
#include "src/objects/function-kind.h"
#include "src/objects/instance-type-inl.h"
#include "src/objects/js-function-inl.h"
#include "src/objects/map-inl.h"
#include "src/objects/name-inl.h"
#include "src/objects/shared-function-info-inl.h"
#include "src/objects/string-inl.h"

namespace v8 {
namespace internal {

bool FunctionTemplateInfo::HasInstanceType() {
  return instance_type() != kNoJSApiObjectType;
}

Handle<SharedFunctionInfo> FunctionTemplateInfo::GetOrCreateSharedFunctionInfo(
    Isolate* isolate, Handle<FunctionTemplateInfo> info,
    MaybeHandle<Name> maybe_name) {
  Object current_info = info->shared_function_info();
  if (current_info.IsSharedFunctionInfo()) {
    return handle(SharedFunctionInfo::cast(current_info), isolate);
  }
  Handle<Name> name;
  Handle<String> name_string;
  if (maybe_name.ToHandle(&name) && name->IsString()) {
    name_string = Handle<String>::cast(name);
  } else if (info->class_name().IsString()) {
    name_string = handle(String::cast(info->class_name()), isolate);
  } else {
    name_string = isolate->factory()->empty_string();
  }
  FunctionKind function_kind;
  if (info->remove_prototype()) {
    function_kind = FunctionKind::kConciseMethod;
  } else {
    function_kind = FunctionKind::kNormalFunction;
  }
  Handle<SharedFunctionInfo> sfi =
      isolate->factory()->NewSharedFunctionInfoForApiFunction(name_string, info,
                                                              function_kind);
  {
    DisallowGarbageCollection no_gc;
    auto raw_sfi = *sfi;
    auto raw_template = *info;
    raw_sfi.set_length(raw_template.length());
    raw_sfi.DontAdaptArguments();
    DCHECK(raw_sfi.IsApiFunction());
    raw_template.set_shared_function_info(raw_sfi);
  }
  return sfi;
}

bool FunctionTemplateInfo::IsTemplateFor(Map map) const {
  RCS_SCOPE(
      LocalHeap::Current() == nullptr
          ? GetIsolate()->counters()->runtime_call_stats()
          : LocalIsolate::FromHeap(LocalHeap::Current())->runtime_call_stats(),
      RuntimeCallCounterId::kIsTemplateFor);

  // There is a constraint on the object; check.
  if (!map.IsJSObjectMap()) return false;

  if (v8_flags.embedder_instance_types) {
    DCHECK_IMPLIES(allowed_receiver_instance_type_range_start() == 0,
                   allowed_receiver_instance_type_range_end() == 0);
    if (base::IsInRange(map.instance_type(),
                        allowed_receiver_instance_type_range_start(),
                        allowed_receiver_instance_type_range_end())) {
      return true;
    }
  }

  // Fetch the constructor function of the object.
  Object cons_obj = map.GetConstructor();
  Object type;
  if (cons_obj.IsJSFunction()) {
    JSFunction fun = JSFunction::cast(cons_obj);
    type = fun.shared().function_data(kAcquireLoad);
  } else if (cons_obj.IsFunctionTemplateInfo()) {
    type = FunctionTemplateInfo::cast(cons_obj);
  } else {
    return false;
  }
  // Iterate through the chain of inheriting function templates to
  // see if the required one occurs.
  while (type.IsFunctionTemplateInfo()) {
    if (type == *this) return true;
    type = FunctionTemplateInfo::cast(type).GetParentTemplate();
  }
  // Didn't find the required type in the inheritance chain.
  return false;
}

bool FunctionTemplateInfo::IsLeafTemplateForApiObject(Object object) const {
  i::DisallowGarbageCollection no_gc;

  if (!object.IsJSApiObject()) {
    return false;
  }

  bool result = false;
  Map map = HeapObject::cast(object).map();
  Object constructor_obj = map.GetConstructor();
  if (constructor_obj.IsJSFunction()) {
    JSFunction fun = JSFunction::cast(constructor_obj);
    result = (*this == fun.shared().function_data(kAcquireLoad));
  } else if (constructor_obj.IsFunctionTemplateInfo()) {
    result = (*this == constructor_obj);
  }
  DCHECK_IMPLIES(result, IsTemplateFor(map));
  return result;
}

// static
FunctionTemplateRareData FunctionTemplateInfo::AllocateFunctionTemplateRareData(
    Isolate* isolate, Handle<FunctionTemplateInfo> function_template_info) {
  DCHECK(function_template_info->rare_data(kAcquireLoad).IsUndefined(isolate));
  Handle<FunctionTemplateRareData> rare_data =
      isolate->factory()->NewFunctionTemplateRareData();
  function_template_info->set_rare_data(*rare_data, kReleaseStore);
  return *rare_data;
}

base::Optional<Name> FunctionTemplateInfo::TryGetCachedPropertyName(
    Isolate* isolate, Object getter) {
  DisallowGarbageCollection no_gc;
  if (!getter.IsFunctionTemplateInfo()) {
    if (!getter.IsJSFunction()) return {};
    SharedFunctionInfo info = JSFunction::cast(getter).shared();
    if (!info.IsApiFunction()) return {};
    getter = info.get_api_func_data();
  }
  // Check if the accessor uses a cached property.
  Object maybe_name = FunctionTemplateInfo::cast(getter).cached_property_name();
  if (maybe_name.IsTheHole(isolate)) return {};
  return Name::cast(maybe_name);
}

int FunctionTemplateInfo::GetCFunctionsCount() const {
  i::DisallowHeapAllocation no_gc;
  return FixedArray::cast(GetCFunctionOverloads()).length() /
         kFunctionOverloadEntrySize;
}

Address FunctionTemplateInfo::GetCFunction(int index) const {
  i::DisallowHeapAllocation no_gc;
  return v8::ToCData<Address>(FixedArray::cast(GetCFunctionOverloads())
                                  .get(index * kFunctionOverloadEntrySize));
}

const CFunctionInfo* FunctionTemplateInfo::GetCSignature(int index) const {
  i::DisallowHeapAllocation no_gc;
  return v8::ToCData<CFunctionInfo*>(
      FixedArray::cast(GetCFunctionOverloads())
          .get(index * kFunctionOverloadEntrySize + 1));
}

}  // namespace internal
}  // namespace v8
