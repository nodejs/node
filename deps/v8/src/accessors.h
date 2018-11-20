// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ACCESSORS_H_
#define V8_ACCESSORS_H_

#include "include/v8.h"
#include "src/allocation.h"
#include "src/globals.h"
#include "src/property-details.h"

namespace v8 {
namespace internal {

// Forward declarations.
class AccessorInfo;
template <typename T>
class Handle;
class FieldIndex;
class JavaScriptFrame;

// The list of accessor descriptors. This is a second-order macro
// taking a macro to be applied to all accessor descriptor names.
#define ACCESSOR_INFO_LIST(V)                                       \
  V(arguments_iterator, ArgumentsIterator)                          \
  V(array_length, ArrayLength)                                      \
  V(bound_function_length, BoundFunctionLength)                     \
  V(bound_function_name, BoundFunctionName)                         \
  V(error_stack, ErrorStack)                                        \
  V(function_arguments, FunctionArguments)                          \
  V(function_caller, FunctionCaller)                                \
  V(function_name, FunctionName)                                    \
  V(function_length, FunctionLength)                                \
  V(function_prototype, FunctionPrototype)                          \
  V(string_length, StringLength)

#define SIDE_EFFECT_FREE_ACCESSOR_INFO_LIST(V) \
  V(ArrayLength)                               \
  V(BoundFunctionLength)                       \
  V(BoundFunctionName)                         \
  V(FunctionName)                              \
  V(FunctionLength)                            \
  V(FunctionPrototype)                         \
  V(StringLength)

#define ACCESSOR_SETTER_LIST(V) \
  V(ArrayLengthSetter)          \
  V(ErrorStackSetter)           \
  V(FunctionPrototypeSetter)    \
  V(ModuleNamespaceEntrySetter) \
  V(ReconfigureToDataProperty)

// Accessors contains all predefined proxy accessors.

class Accessors : public AllStatic {
 public:
#define ACCESSOR_GETTER_DECLARATION(accessor_name, AccessorName) \
  static void AccessorName##Getter(                              \
      v8::Local<v8::Name> name,                                  \
      const v8::PropertyCallbackInfo<v8::Value>& info);
  ACCESSOR_INFO_LIST(ACCESSOR_GETTER_DECLARATION)
#undef ACCESSOR_GETTER_DECLARATION

#define ACCESSOR_SETTER_DECLARATION(accessor_name)          \
  static void accessor_name(                                \
      v8::Local<v8::Name> name, v8::Local<v8::Value> value, \
      const v8::PropertyCallbackInfo<v8::Boolean>& info);
  ACCESSOR_SETTER_LIST(ACCESSOR_SETTER_DECLARATION)
#undef ACCESSOR_SETTER_DECLARATION

  static constexpr int kAccessorInfoCount =
#define COUNT_ACCESSOR(...) +1
      ACCESSOR_INFO_LIST(COUNT_ACCESSOR);
#undef COUNT_ACCESSOR

  static constexpr int kAccessorSetterCount =
#define COUNT_ACCESSOR(...) +1
      ACCESSOR_SETTER_LIST(COUNT_ACCESSOR);
#undef COUNT_ACCESSOR

  static void ModuleNamespaceEntryGetter(
      v8::Local<v8::Name> name,
      const v8::PropertyCallbackInfo<v8::Value>& info);
  static Handle<AccessorInfo> MakeModuleNamespaceEntryInfo(Isolate* isolate,
                                                           Handle<String> name);

  // Accessor function called directly from the runtime system. Returns the
  // newly materialized arguments object for the given {frame}. Note that for
  // optimized frames it is possible to specify an {inlined_jsframe_index}.
  static Handle<JSObject> FunctionGetArguments(JavaScriptFrame* frame,
                                               int inlined_jsframe_index);

  // Returns true for properties that are accessors to object fields.
  // If true, the matching FieldIndex is returned through |field_index|.
  static bool IsJSObjectFieldAccessor(Isolate* isolate, Handle<Map> map,
                                      Handle<Name> name,
                                      FieldIndex* field_index);

  static MaybeHandle<Object> ReplaceAccessorWithDataProperty(
      Isolate* isolate, Handle<Object> receiver, Handle<JSObject> holder,
      Handle<Name> name, Handle<Object> value);

  // Create an AccessorInfo. The setter is optional (can be nullptr).
  //
  // Note that the type of setter is AccessorNameBooleanSetterCallback instead
  // of v8::AccessorNameSetterCallback.  The difference is that the former can
  // set a (boolean) return value. The setter should roughly follow the same
  // conventions as many of the internal methods in objects.cc:
  // - The return value is unset iff there was an exception.
  // - If the ShouldThrow argument is true, the return value must not be false.
  typedef void (*AccessorNameBooleanSetterCallback)(
      Local<v8::Name> property, Local<v8::Value> value,
      const PropertyCallbackInfo<v8::Boolean>& info);

  static Handle<AccessorInfo> MakeAccessor(
      Isolate* isolate, Handle<Name> name, AccessorNameGetterCallback getter,
      AccessorNameBooleanSetterCallback setter);

 private:
#define ACCESSOR_INFO_DECLARATION(accessor_name, AccessorName) \
  static Handle<AccessorInfo> Make##AccessorName##Info(Isolate* isolate);
  ACCESSOR_INFO_LIST(ACCESSOR_INFO_DECLARATION)
#undef ACCESSOR_INFO_DECLARATION

  friend class Heap;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_ACCESSORS_H_
