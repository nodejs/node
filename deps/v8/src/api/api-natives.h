// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_API_API_NATIVES_H_
#define V8_API_API_NATIVES_H_

#include "include/v8.h"
#include "src/base/macros.h"
#include "src/handles/handles.h"
#include "src/handles/maybe-handles.h"
#include "src/objects/objects.h"
#include "src/objects/property-details.h"

namespace v8 {
namespace internal {

// Forward declarations.
enum InstanceType : uint16_t;
class ObjectTemplateInfo;
class TemplateInfo;

class ApiNatives {
 public:
  static const int kInitialFunctionCacheSize = 256;

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSFunction> InstantiateFunction(
      Handle<FunctionTemplateInfo> data,
      MaybeHandle<Name> maybe_name = MaybeHandle<Name>());

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> InstantiateObject(
      Isolate* isolate, Handle<ObjectTemplateInfo> data,
      Handle<JSReceiver> new_target = Handle<JSReceiver>());

  V8_WARN_UNUSED_RESULT static MaybeHandle<JSObject> InstantiateRemoteObject(
      Handle<ObjectTemplateInfo> data);

  static Handle<JSFunction> CreateApiFunction(
      Isolate* isolate, Handle<FunctionTemplateInfo> obj,
      Handle<Object> prototype, InstanceType type,
      MaybeHandle<Name> name = MaybeHandle<Name>());

  static void AddDataProperty(Isolate* isolate, Handle<TemplateInfo> info,
                              Handle<Name> name, Handle<Object> value,
                              PropertyAttributes attributes);

  static void AddDataProperty(Isolate* isolate, Handle<TemplateInfo> info,
                              Handle<Name> name, v8::Intrinsic intrinsic,
                              PropertyAttributes attributes);

  static void AddAccessorProperty(Isolate* isolate, Handle<TemplateInfo> info,
                                  Handle<Name> name,
                                  Handle<FunctionTemplateInfo> getter,
                                  Handle<FunctionTemplateInfo> setter,
                                  PropertyAttributes attributes);

  static void AddNativeDataProperty(Isolate* isolate, Handle<TemplateInfo> info,
                                    Handle<AccessorInfo> property);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_API_API_NATIVES_H_
