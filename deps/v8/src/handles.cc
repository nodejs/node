// Copyright 2009 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include "v8.h"

#include "accessors.h"
#include "api.h"
#include "arguments.h"
#include "bootstrapper.h"
#include "codegen.h"
#include "compiler.h"
#include "debug.h"
#include "execution.h"
#include "global-handles.h"
#include "natives.h"
#include "runtime.h"
#include "stub-cache.h"

namespace v8 {
namespace internal {


v8::ImplementationUtilities::HandleScopeData HandleScope::current_ =
    { -1, NULL, NULL };


int HandleScope::NumberOfHandles() {
  int n = HandleScopeImplementer::instance()->blocks()->length();
  if (n == 0) return 0;
  return ((n - 1) * kHandleBlockSize) + static_cast<int>(
      (current_.next - HandleScopeImplementer::instance()->blocks()->last()));
}


Object** HandleScope::Extend() {
  Object** result = current_.next;

  ASSERT(result == current_.limit);
  // Make sure there's at least one scope on the stack and that the
  // top of the scope stack isn't a barrier.
  if (current_.extensions < 0) {
    Utils::ReportApiFailure("v8::HandleScope::CreateHandle()",
                            "Cannot create a handle without a HandleScope");
    return NULL;
  }
  HandleScopeImplementer* impl = HandleScopeImplementer::instance();
  // If there's more room in the last block, we use that. This is used
  // for fast creation of scopes after scope barriers.
  if (!impl->blocks()->is_empty()) {
    Object** limit = &impl->blocks()->last()[kHandleBlockSize];
    if (current_.limit != limit) {
      current_.limit = limit;
    }
  }

  // If we still haven't found a slot for the handle, we extend the
  // current handle scope by allocating a new handle block.
  if (result == current_.limit) {
    // If there's a spare block, use it for growing the current scope.
    result = impl->GetSpareOrNewBlock();
    // Add the extension to the global list of blocks, but count the
    // extension as part of the current scope.
    impl->blocks()->Add(result);
    current_.extensions++;
    current_.limit = &result[kHandleBlockSize];
  }

  return result;
}


void HandleScope::DeleteExtensions() {
  ASSERT(current_.extensions != 0);
  HandleScopeImplementer::instance()->DeleteExtensions(current_.extensions);
}


void HandleScope::ZapRange(Object** start, Object** end) {
  if (start == NULL) return;
  for (Object** p = start; p < end; p++) {
    *reinterpret_cast<Address*>(p) = v8::internal::kHandleZapValue;
  }
}


Address HandleScope::current_extensions_address() {
  return reinterpret_cast<Address>(&current_.extensions);
}


Address HandleScope::current_next_address() {
  return reinterpret_cast<Address>(&current_.next);
}


Address HandleScope::current_limit_address() {
  return reinterpret_cast<Address>(&current_.limit);
}


Handle<FixedArray> AddKeysFromJSArray(Handle<FixedArray> content,
                                      Handle<JSArray> array) {
  CALL_HEAP_FUNCTION(content->AddKeysFromJSArray(*array), FixedArray);
}


Handle<FixedArray> UnionOfKeys(Handle<FixedArray> first,
                               Handle<FixedArray> second) {
  CALL_HEAP_FUNCTION(first->UnionOfKeys(*second), FixedArray);
}


Handle<JSGlobalProxy> ReinitializeJSGlobalProxy(
    Handle<JSFunction> constructor,
    Handle<JSGlobalProxy> global) {
  CALL_HEAP_FUNCTION(Heap::ReinitializeJSGlobalProxy(*constructor, *global),
                     JSGlobalProxy);
}


void SetExpectedNofProperties(Handle<JSFunction> func, int nof) {
  func->shared()->set_expected_nof_properties(nof);
  if (func->has_initial_map()) {
    Handle<Map> new_initial_map =
        Factory::CopyMapDropTransitions(Handle<Map>(func->initial_map()));
    new_initial_map->set_unused_property_fields(nof);
    func->set_initial_map(*new_initial_map);
  }
}


void SetPrototypeProperty(Handle<JSFunction> func, Handle<JSObject> value) {
  CALL_HEAP_FUNCTION_VOID(func->SetPrototype(*value));
}


static int ExpectedNofPropertiesFromEstimate(int estimate) {
  // TODO(1231235): We need dynamic feedback to estimate the number
  // of expected properties in an object. The static hack below
  // is barely a solution.
  if (estimate == 0) return 4;
  return estimate + 2;
}


void SetExpectedNofPropertiesFromEstimate(Handle<SharedFunctionInfo> shared,
                                          int estimate) {
  shared->set_expected_nof_properties(
      ExpectedNofPropertiesFromEstimate(estimate));
}


void NormalizeProperties(Handle<JSObject> object,
                         PropertyNormalizationMode mode,
                         int expected_additional_properties) {
  CALL_HEAP_FUNCTION_VOID(object->NormalizeProperties(
      mode,
      expected_additional_properties));
}


void NormalizeElements(Handle<JSObject> object) {
  CALL_HEAP_FUNCTION_VOID(object->NormalizeElements());
}


void TransformToFastProperties(Handle<JSObject> object,
                               int unused_property_fields) {
  CALL_HEAP_FUNCTION_VOID(
      object->TransformToFastProperties(unused_property_fields));
}


void FlattenString(Handle<String> string) {
  CALL_HEAP_FUNCTION_VOID(string->TryFlatten());
}


Handle<String> FlattenGetString(Handle<String> string) {
  Handle<String> result;
  CALL_AND_RETRY(string->TryFlatten(),
                 { result = Handle<String>(String::cast(__object__));
                   break; },
                 return Handle<String>());
  ASSERT(string->IsFlat());
  return result;
}


Handle<Object> SetPrototype(Handle<JSFunction> function,
                            Handle<Object> prototype) {
  ASSERT(function->should_have_prototype());
  CALL_HEAP_FUNCTION(Accessors::FunctionSetPrototype(*function,
                                                     *prototype,
                                                     NULL),
                     Object);
}


Handle<Object> SetProperty(Handle<JSObject> object,
                           Handle<String> key,
                           Handle<Object> value,
                           PropertyAttributes attributes) {
  CALL_HEAP_FUNCTION(object->SetProperty(*key, *value, attributes), Object);
}


Handle<Object> SetProperty(Handle<Object> object,
                           Handle<Object> key,
                           Handle<Object> value,
                           PropertyAttributes attributes) {
  CALL_HEAP_FUNCTION(
      Runtime::SetObjectProperty(object, key, value, attributes), Object);
}


Handle<Object> ForceSetProperty(Handle<JSObject> object,
                                Handle<Object> key,
                                Handle<Object> value,
                                PropertyAttributes attributes) {
  CALL_HEAP_FUNCTION(
      Runtime::ForceSetObjectProperty(object, key, value, attributes), Object);
}


Handle<Object> SetNormalizedProperty(Handle<JSObject> object,
                                     Handle<String> key,
                                     Handle<Object> value,
                                     PropertyDetails details) {
  CALL_HEAP_FUNCTION(object->SetNormalizedProperty(*key, *value, details),
                     Object);
}


Handle<Object> ForceDeleteProperty(Handle<JSObject> object,
                                   Handle<Object> key) {
  CALL_HEAP_FUNCTION(Runtime::ForceDeleteObjectProperty(object, key), Object);
}


Handle<Object> IgnoreAttributesAndSetLocalProperty(
    Handle<JSObject> object,
    Handle<String> key,
    Handle<Object> value,
    PropertyAttributes attributes) {
  CALL_HEAP_FUNCTION(object->
      IgnoreAttributesAndSetLocalProperty(*key, *value, attributes), Object);
}


Handle<Object> SetPropertyWithInterceptor(Handle<JSObject> object,
                                          Handle<String> key,
                                          Handle<Object> value,
                                          PropertyAttributes attributes) {
  CALL_HEAP_FUNCTION(object->SetPropertyWithInterceptor(*key,
                                                        *value,
                                                        attributes),
                     Object);
}


Handle<Object> GetProperty(Handle<JSObject> obj,
                           const char* name) {
  Handle<String> str = Factory::LookupAsciiSymbol(name);
  CALL_HEAP_FUNCTION(obj->GetProperty(*str), Object);
}


Handle<Object> GetProperty(Handle<Object> obj,
                           Handle<Object> key) {
  CALL_HEAP_FUNCTION(Runtime::GetObjectProperty(obj, key), Object);
}


Handle<Object> GetElement(Handle<Object> obj,
                          uint32_t index) {
  CALL_HEAP_FUNCTION(Runtime::GetElement(obj, index), Object);
}


Handle<Object> GetPropertyWithInterceptor(Handle<JSObject> receiver,
                                          Handle<JSObject> holder,
                                          Handle<String> name,
                                          PropertyAttributes* attributes) {
  CALL_HEAP_FUNCTION(holder->GetPropertyWithInterceptor(*receiver,
                                                        *name,
                                                        attributes),
                     Object);
}


Handle<Object> GetPrototype(Handle<Object> obj) {
  Handle<Object> result(obj->GetPrototype());
  return result;
}


Handle<Object> SetPrototype(Handle<JSObject> obj, Handle<Object> value) {
  const bool skip_hidden_prototypes = false;
  CALL_HEAP_FUNCTION(obj->SetPrototype(*value, skip_hidden_prototypes), Object);
}


Handle<Object> GetHiddenProperties(Handle<JSObject> obj,
                                   bool create_if_needed) {
  Object* holder = obj->BypassGlobalProxy();
  if (holder->IsUndefined()) return Factory::undefined_value();
  obj = Handle<JSObject>(JSObject::cast(holder));

  if (obj->HasFastProperties()) {
    // If the object has fast properties, check whether the first slot
    // in the descriptor array matches the hidden symbol. Since the
    // hidden symbols hash code is zero (and no other string has hash
    // code zero) it will always occupy the first entry if present.
    DescriptorArray* descriptors = obj->map()->instance_descriptors();
    if ((descriptors->number_of_descriptors() > 0) &&
        (descriptors->GetKey(0) == Heap::hidden_symbol()) &&
        descriptors->IsProperty(0)) {
      ASSERT(descriptors->GetType(0) == FIELD);
      return Handle<Object>(obj->FastPropertyAt(descriptors->GetFieldIndex(0)));
    }
  }

  // Only attempt to find the hidden properties in the local object and not
  // in the prototype chain.  Note that HasLocalProperty() can cause a GC in
  // the general case in the presence of interceptors.
  if (!obj->HasHiddenPropertiesObject()) {
    // Hidden properties object not found. Allocate a new hidden properties
    // object if requested. Otherwise return the undefined value.
    if (create_if_needed) {
      Handle<Object> hidden_obj = Factory::NewJSObject(Top::object_function());
      CALL_HEAP_FUNCTION(obj->SetHiddenPropertiesObject(*hidden_obj), Object);
    } else {
      return Factory::undefined_value();
    }
  }
  return Handle<Object>(obj->GetHiddenPropertiesObject());
}


Handle<Object> DeleteElement(Handle<JSObject> obj,
                             uint32_t index) {
  CALL_HEAP_FUNCTION(obj->DeleteElement(index, JSObject::NORMAL_DELETION),
                     Object);
}


Handle<Object> DeleteProperty(Handle<JSObject> obj,
                              Handle<String> prop) {
  CALL_HEAP_FUNCTION(obj->DeleteProperty(*prop, JSObject::NORMAL_DELETION),
                     Object);
}


Handle<Object> LookupSingleCharacterStringFromCode(uint32_t index) {
  CALL_HEAP_FUNCTION(Heap::LookupSingleCharacterStringFromCode(index), Object);
}


Handle<String> SubString(Handle<String> str,
                         int start,
                         int end,
                         PretenureFlag pretenure) {
  CALL_HEAP_FUNCTION(str->SubString(start, end, pretenure), String);
}


Handle<Object> SetElement(Handle<JSObject> object,
                          uint32_t index,
                          Handle<Object> value) {
  if (object->HasPixelElements() || object->HasExternalArrayElements()) {
    if (!value->IsSmi() && !value->IsHeapNumber() && !value->IsUndefined()) {
      bool has_exception;
      Handle<Object> number = Execution::ToNumber(value, &has_exception);
      if (has_exception) return Handle<Object>();
      value = number;
    }
  }
  CALL_HEAP_FUNCTION(object->SetElement(index, *value), Object);
}


Handle<JSObject> Copy(Handle<JSObject> obj) {
  CALL_HEAP_FUNCTION(Heap::CopyJSObject(*obj), JSObject);
}


Handle<Object> SetAccessor(Handle<JSObject> obj, Handle<AccessorInfo> info) {
  CALL_HEAP_FUNCTION(obj->DefineAccessor(*info), Object);
}


// Wrappers for scripts are kept alive and cached in weak global
// handles referred from proxy objects held by the scripts as long as
// they are used. When they are not used anymore, the garbage
// collector will call the weak callback on the global handle
// associated with the wrapper and get rid of both the wrapper and the
// handle.
static void ClearWrapperCache(Persistent<v8::Value> handle, void*) {
#ifdef ENABLE_HEAP_PROTECTION
  // Weak reference callbacks are called as if from outside V8.  We
  // need to reeenter to unprotect the heap.
  VMState state(OTHER);
#endif
  Handle<Object> cache = Utils::OpenHandle(*handle);
  JSValue* wrapper = JSValue::cast(*cache);
  Proxy* proxy = Script::cast(wrapper->value())->wrapper();
  ASSERT(proxy->proxy() == reinterpret_cast<Address>(cache.location()));
  proxy->set_proxy(0);
  GlobalHandles::Destroy(cache.location());
  Counters::script_wrappers.Decrement();
}


Handle<JSValue> GetScriptWrapper(Handle<Script> script) {
  if (script->wrapper()->proxy() != NULL) {
    // Return the script wrapper directly from the cache.
    return Handle<JSValue>(
        reinterpret_cast<JSValue**>(script->wrapper()->proxy()));
  }

  // Construct a new script wrapper.
  Counters::script_wrappers.Increment();
  Handle<JSFunction> constructor = Top::script_function();
  Handle<JSValue> result =
      Handle<JSValue>::cast(Factory::NewJSObject(constructor));
  result->set_value(*script);

  // Create a new weak global handle and use it to cache the wrapper
  // for future use. The cache will automatically be cleared by the
  // garbage collector when it is not used anymore.
  Handle<Object> handle = GlobalHandles::Create(*result);
  GlobalHandles::MakeWeak(handle.location(), NULL, &ClearWrapperCache);
  script->wrapper()->set_proxy(reinterpret_cast<Address>(handle.location()));
  return result;
}


// Init line_ends array with code positions of line ends inside script
// source.
void InitScriptLineEnds(Handle<Script> script) {
  if (!script->line_ends()->IsUndefined()) return;

  if (!script->source()->IsString()) {
    ASSERT(script->source()->IsUndefined());
    script->set_line_ends(*(Factory::NewFixedArray(0)));
    ASSERT(script->line_ends()->IsFixedArray());
    return;
  }

  Handle<String> src(String::cast(script->source()));

  Handle<FixedArray> array = CalculateLineEnds(src, true);

  script->set_line_ends(*array);
  ASSERT(script->line_ends()->IsFixedArray());
}


Handle<FixedArray> CalculateLineEnds(Handle<String> src,
                                     bool with_imaginary_last_new_line) {
  const int src_len = src->length();
  Handle<String> new_line = Factory::NewStringFromAscii(CStrVector("\n"));

  // Pass 1: Identify line count.
  int line_count = 0;
  int position = 0;
  while (position != -1 && position < src_len) {
    position = Runtime::StringMatch(src, new_line, position);
    if (position != -1) {
      position++;
    }
    if (position != -1) {
      line_count++;
    } else if (with_imaginary_last_new_line) {
      // Even if the last line misses a line end, it is counted.
      line_count++;
    }
  }

  // Pass 2: Fill in line ends positions
  Handle<FixedArray> array = Factory::NewFixedArray(line_count);
  int array_index = 0;
  position = 0;
  while (position != -1 && position < src_len) {
    position = Runtime::StringMatch(src, new_line, position);
    if (position != -1) {
      array->set(array_index++, Smi::FromInt(position++));
    } else if (with_imaginary_last_new_line) {
      // If the script does not end with a line ending add the final end
      // position as just past the last line ending.
      array->set(array_index++, Smi::FromInt(src_len));
    }
  }
  ASSERT(array_index == line_count);

  return array;
}


// Convert code position into line number.
int GetScriptLineNumber(Handle<Script> script, int code_pos) {
  InitScriptLineEnds(script);
  AssertNoAllocation no_allocation;
  FixedArray* line_ends_array = FixedArray::cast(script->line_ends());
  const int line_ends_len = line_ends_array->length();

  if (!line_ends_len)
    return -1;

  if ((Smi::cast(line_ends_array->get(0)))->value() >= code_pos)
    return script->line_offset()->value();

  int left = 0;
  int right = line_ends_len;
  while (int half = (right - left) / 2) {
    if ((Smi::cast(line_ends_array->get(left + half)))->value() > code_pos) {
      right -= half;
    } else {
      left += half;
    }
  }
  return right + script->line_offset()->value();
}


int GetScriptLineNumberSafe(Handle<Script> script, int code_pos) {
  AssertNoAllocation no_allocation;
  if (!script->line_ends()->IsUndefined()) {
    return GetScriptLineNumber(script, code_pos);
  }
  // Slow mode: we do not have line_ends. We have to iterate through source.
  if (!script->source()->IsString()) {
    return -1;
  }
  String* source = String::cast(script->source());
  int line = 0;
  int len = source->length();
  for (int pos = 0; pos < len; pos++) {
    if (pos == code_pos) {
      break;
    }
    if (source->Get(pos) == '\n') {
      line++;
    }
  }
  return line;
}


void CustomArguments::IterateInstance(ObjectVisitor* v) {
  v->VisitPointers(values_, values_ + ARRAY_SIZE(values_));
}


// Compute the property keys from the interceptor.
v8::Handle<v8::Array> GetKeysForNamedInterceptor(Handle<JSObject> receiver,
                                                 Handle<JSObject> object) {
  Handle<InterceptorInfo> interceptor(object->GetNamedInterceptor());
  CustomArguments args(interceptor->data(), *receiver, *object);
  v8::AccessorInfo info(args.end());
  v8::Handle<v8::Array> result;
  if (!interceptor->enumerator()->IsUndefined()) {
    v8::NamedPropertyEnumerator enum_fun =
        v8::ToCData<v8::NamedPropertyEnumerator>(interceptor->enumerator());
    LOG(ApiObjectAccess("interceptor-named-enum", *object));
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = enum_fun(info);
    }
  }
  return result;
}


// Compute the element keys from the interceptor.
v8::Handle<v8::Array> GetKeysForIndexedInterceptor(Handle<JSObject> receiver,
                                                   Handle<JSObject> object) {
  Handle<InterceptorInfo> interceptor(object->GetIndexedInterceptor());
  CustomArguments args(interceptor->data(), *receiver, *object);
  v8::AccessorInfo info(args.end());
  v8::Handle<v8::Array> result;
  if (!interceptor->enumerator()->IsUndefined()) {
    v8::IndexedPropertyEnumerator enum_fun =
        v8::ToCData<v8::IndexedPropertyEnumerator>(interceptor->enumerator());
    LOG(ApiObjectAccess("interceptor-indexed-enum", *object));
    {
      // Leaving JavaScript.
      VMState state(EXTERNAL);
      result = enum_fun(info);
    }
  }
  return result;
}


Handle<FixedArray> GetKeysInFixedArrayFor(Handle<JSObject> object,
                                          KeyCollectionType type) {
  Handle<FixedArray> content = Factory::empty_fixed_array();
  Handle<JSObject> arguments_boilerplate =
      Handle<JSObject>(
          Top::context()->global_context()->arguments_boilerplate());
  Handle<JSFunction> arguments_function =
      Handle<JSFunction>(
          JSFunction::cast(arguments_boilerplate->map()->constructor()));

  // Only collect keys if access is permitted.
  for (Handle<Object> p = object;
       *p != Heap::null_value();
       p = Handle<Object>(p->GetPrototype())) {
    Handle<JSObject> current(JSObject::cast(*p));

    // Check access rights if required.
    if (current->IsAccessCheckNeeded() &&
        !Top::MayNamedAccess(*current, Heap::undefined_value(),
                             v8::ACCESS_KEYS)) {
      Top::ReportFailedAccessCheck(*current, v8::ACCESS_KEYS);
      break;
    }

    // Compute the element keys.
    Handle<FixedArray> element_keys =
        Factory::NewFixedArray(current->NumberOfEnumElements());
    current->GetEnumElementKeys(*element_keys);
    content = UnionOfKeys(content, element_keys);

    // Add the element keys from the interceptor.
    if (current->HasIndexedInterceptor()) {
      v8::Handle<v8::Array> result =
          GetKeysForIndexedInterceptor(object, current);
      if (!result.IsEmpty())
        content = AddKeysFromJSArray(content, v8::Utils::OpenHandle(*result));
    }

    // We can cache the computed property keys if access checks are
    // not needed and no interceptors are involved.
    //
    // We do not use the cache if the object has elements and
    // therefore it does not make sense to cache the property names
    // for arguments objects.  Arguments objects will always have
    // elements.
    // Wrapped strings have elements, but don't have an elements
    // array or dictionary.  So the fast inline test for whether to
    // use the cache says yes, so we should not create a cache.
    bool cache_enum_keys =
        ((current->map()->constructor() != *arguments_function) &&
         !current->IsJSValue() &&
         !current->IsAccessCheckNeeded() &&
         !current->HasNamedInterceptor() &&
         !current->HasIndexedInterceptor());
    // Compute the property keys and cache them if possible.
    content =
        UnionOfKeys(content, GetEnumPropertyKeys(current, cache_enum_keys));

    // Add the property keys from the interceptor.
    if (current->HasNamedInterceptor()) {
      v8::Handle<v8::Array> result =
          GetKeysForNamedInterceptor(object, current);
      if (!result.IsEmpty())
        content = AddKeysFromJSArray(content, v8::Utils::OpenHandle(*result));
    }

    // If we only want local properties we bail out after the first
    // iteration.
    if (type == LOCAL_ONLY)
      break;
  }
  return content;
}


Handle<JSArray> GetKeysFor(Handle<JSObject> object) {
  Counters::for_in.Increment();
  Handle<FixedArray> elements = GetKeysInFixedArrayFor(object,
                                                       INCLUDE_PROTOS);
  return Factory::NewJSArrayWithElements(elements);
}


Handle<FixedArray> GetEnumPropertyKeys(Handle<JSObject> object,
                                       bool cache_result) {
  int index = 0;
  if (object->HasFastProperties()) {
    if (object->map()->instance_descriptors()->HasEnumCache()) {
      Counters::enum_cache_hits.Increment();
      DescriptorArray* desc = object->map()->instance_descriptors();
      return Handle<FixedArray>(FixedArray::cast(desc->GetEnumCache()));
    }
    Counters::enum_cache_misses.Increment();
    int num_enum = object->NumberOfEnumProperties();
    Handle<FixedArray> storage = Factory::NewFixedArray(num_enum);
    Handle<FixedArray> sort_array = Factory::NewFixedArray(num_enum);
    Handle<DescriptorArray> descs =
        Handle<DescriptorArray>(object->map()->instance_descriptors());
    for (int i = 0; i < descs->number_of_descriptors(); i++) {
      if (descs->IsProperty(i) && !descs->IsDontEnum(i)) {
        (*storage)->set(index, descs->GetKey(i));
        PropertyDetails details(descs->GetDetails(i));
        (*sort_array)->set(index, Smi::FromInt(details.index()));
        index++;
      }
    }
    (*storage)->SortPairs(*sort_array, sort_array->length());
    if (cache_result) {
      Handle<FixedArray> bridge_storage =
          Factory::NewFixedArray(DescriptorArray::kEnumCacheBridgeLength);
      DescriptorArray* desc = object->map()->instance_descriptors();
      desc->SetEnumCache(*bridge_storage, *storage);
    }
    ASSERT(storage->length() == index);
    return storage;
  } else {
    int num_enum = object->NumberOfEnumProperties();
    Handle<FixedArray> storage = Factory::NewFixedArray(num_enum);
    Handle<FixedArray> sort_array = Factory::NewFixedArray(num_enum);
    object->property_dictionary()->CopyEnumKeysTo(*storage, *sort_array);
    return storage;
  }
}


bool EnsureCompiled(Handle<SharedFunctionInfo> shared,
                    ClearExceptionFlag flag) {
  return shared->is_compiled() || CompileLazyShared(shared, flag);
}


static bool CompileLazyHelper(CompilationInfo* info,
                              ClearExceptionFlag flag) {
  // Compile the source information to a code object.
  ASSERT(!info->shared_info()->is_compiled());
  bool result = Compiler::CompileLazy(info);
  ASSERT(result != Top::has_pending_exception());
  if (!result && flag == CLEAR_EXCEPTION) Top::clear_pending_exception();
  return result;
}


bool CompileLazyShared(Handle<SharedFunctionInfo> shared,
                       ClearExceptionFlag flag) {
  CompilationInfo info(shared);
  return CompileLazyHelper(&info, flag);
}


bool CompileLazy(Handle<JSFunction> function,
                 Handle<Object> receiver,
                 ClearExceptionFlag flag) {
  if (function->shared()->is_compiled()) {
    function->set_code(function->shared()->code());
    return true;
  } else {
    CompilationInfo info(function, 0, receiver);
    bool result = CompileLazyHelper(&info, flag);
    PROFILE(FunctionCreateEvent(*function));
    return result;
  }
}


bool CompileLazyInLoop(Handle<JSFunction> function,
                       Handle<Object> receiver,
                       ClearExceptionFlag flag) {
  if (function->shared()->is_compiled()) {
    function->set_code(function->shared()->code());
    return true;
  } else {
    CompilationInfo info(function, 1, receiver);
    bool result = CompileLazyHelper(&info, flag);
    PROFILE(FunctionCreateEvent(*function));
    return result;
  }
}


OptimizedObjectForAddingMultipleProperties::
OptimizedObjectForAddingMultipleProperties(Handle<JSObject> object,
                                           int expected_additional_properties,
                                           bool condition) {
  object_ = object;
  if (condition && object_->HasFastProperties()) {
    // Normalize the properties of object to avoid n^2 behavior
    // when extending the object multiple properties. Indicate the number of
    // properties to be added.
    unused_property_fields_ = object->map()->unused_property_fields();
    NormalizeProperties(object_,
                        KEEP_INOBJECT_PROPERTIES,
                        expected_additional_properties);
    has_been_transformed_ = true;

  } else {
    has_been_transformed_ = false;
  }
}


OptimizedObjectForAddingMultipleProperties::
~OptimizedObjectForAddingMultipleProperties() {
  // Reoptimize the object to allow fast property access.
  if (has_been_transformed_) {
    TransformToFastProperties(object_, unused_property_fields_);
  }
}

} }  // namespace v8::internal
