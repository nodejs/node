// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ACCESSORS_H_
#define V8_ACCESSORS_H_

#include "include/v8.h"
#include "src/allocation.h"
#include "src/globals.h"
#include "src/handles.h"
#include "src/property-details.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AccessorInfo;

// The list of accessor descriptors. This is a second-order macro
// taking a macro to be applied to all accessor descriptor names.
#define ACCESSOR_INFO_LIST(V)     \
  V(ArgumentsIterator)            \
  V(ArrayLength)                  \
  V(FunctionArguments)            \
  V(FunctionCaller)               \
  V(FunctionName)                 \
  V(FunctionLength)               \
  V(FunctionPrototype)            \
  V(ScriptColumnOffset)           \
  V(ScriptCompilationType)        \
  V(ScriptContextData)            \
  V(ScriptEvalFromScript)         \
  V(ScriptEvalFromScriptPosition) \
  V(ScriptEvalFromFunctionName)   \
  V(ScriptId)                     \
  V(ScriptLineEnds)               \
  V(ScriptLineOffset)             \
  V(ScriptName)                   \
  V(ScriptSource)                 \
  V(ScriptType)                   \
  V(ScriptSourceUrl)              \
  V(ScriptSourceMappingUrl)       \
  V(ScriptIsEmbedderDebugScript)  \
  V(StringLength)

#define ACCESSOR_SETTER_LIST(V)        \
  V(ReconfigureToDataProperty)         \
  V(ObservedReconfigureToDataProperty) \
  V(ArrayLengthSetter)                 \
  V(FunctionPrototypeSetter)

// Accessors contains all predefined proxy accessors.

class Accessors : public AllStatic {
 public:
  // Accessor descriptors.
#define ACCESSOR_INFO_DECLARATION(name)                   \
  static void name##Getter(                               \
      v8::Local<v8::Name> name,                           \
      const v8::PropertyCallbackInfo<v8::Value>& info);   \
  static Handle<AccessorInfo> name##Info(                 \
      Isolate* isolate,                                   \
      PropertyAttributes attributes);
  ACCESSOR_INFO_LIST(ACCESSOR_INFO_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION

#define ACCESSOR_SETTER_DECLARATION(name)                                \
  static void name(v8::Local<v8::Name> name, v8::Local<v8::Value> value, \
                   const v8::PropertyCallbackInfo<void>& info);
  ACCESSOR_SETTER_LIST(ACCESSOR_SETTER_DECLARATION)
#undef ACCESSOR_SETTER_DECLARATION

  enum DescriptorId {
#define ACCESSOR_INFO_DECLARATION(name) \
    k##name##Getter, \
    k##name##Setter,
  ACCESSOR_INFO_LIST(ACCESSOR_INFO_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION
    descriptorCount
  };

  // Accessor functions called directly from the runtime system.
  MUST_USE_RESULT static MaybeHandle<Object> FunctionSetPrototype(
      Handle<JSFunction> object, Handle<Object> value);
  static Handle<JSObject> FunctionGetArguments(Handle<JSFunction> object);

  // Accessor infos.
  static Handle<AccessorInfo> MakeModuleExport(
      Handle<String> name, int index, PropertyAttributes attributes);

  // Returns true for properties that are accessors to object fields.
  // If true, *object_offset contains offset of object field.
  static bool IsJSObjectFieldAccessor(Handle<Map> map, Handle<Name> name,
                                      int* object_offset);

  // Returns true for properties that are accessors to ArrayBufferView and
  // derived classes fields. If true, *object_offset contains offset of
  // object field. The caller still has to check whether the underlying
  // buffer was neutered.
  static bool IsJSArrayBufferViewFieldAccessor(Handle<Map> map,
                                               Handle<Name> name,
                                               int* object_offset);

  static Handle<AccessorInfo> MakeAccessor(
      Isolate* isolate,
      Handle<Name> name,
      AccessorNameGetterCallback getter,
      AccessorNameSetterCallback setter,
      PropertyAttributes attributes);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ACCESSORS_H_
