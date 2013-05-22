// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_ARGUMENTS_H_
#define V8_ARGUMENTS_H_

#include "allocation.h"

namespace v8 {
namespace internal {

// Arguments provides access to runtime call parameters.
//
// It uses the fact that the instance fields of Arguments
// (length_, arguments_) are "overlayed" with the parameters
// (no. of parameters, and the parameter pointer) passed so
// that inside the C++ function, the parameters passed can
// be accessed conveniently:
//
//   Object* Runtime_function(Arguments args) {
//     ... use args[i] here ...
//   }

class Arguments BASE_EMBEDDED {
 public:
  Arguments(int length, Object** arguments)
      : length_(length), arguments_(arguments) { }

  Object*& operator[] (int index) {
    ASSERT(0 <= index && index < length_);
    return arguments_[-index];
  }

  template <class S> Handle<S> at(int index) {
    Object** value = &((*this)[index]);
    // This cast checks that the object we're accessing does indeed have the
    // expected type.
    S::cast(*value);
    return Handle<S>(reinterpret_cast<S**>(value));
  }

  int smi_at(int index) {
    return Smi::cast((*this)[index])->value();
  }

  double number_at(int index) {
    return (*this)[index]->Number();
  }

  // Get the total number of arguments including the receiver.
  int length() const { return length_; }

  Object** arguments() { return arguments_; }

 private:
  int length_;
  Object** arguments_;
};


// mappings from old property callbacks to new ones
// F(old name, new name, return value, parameters...)
//
// These aren't included in the list as they have duplicate signatures
// F(NamedPropertyEnumerator, NamedPropertyEnumeratorCallback, ...)
// F(NamedPropertyGetter, NamedPropertyGetterCallback, ...)

#define FOR_EACH_CALLBACK_TABLE_MAPPING_0(F) \
  F(IndexedPropertyEnumerator, IndexedPropertyEnumeratorCallback, v8::Array) \

#define FOR_EACH_CALLBACK_TABLE_MAPPING_1(F) \
  F(AccessorGetter, AccessorGetterCallback, v8::Value, v8::Local<v8::String>) \
  F(NamedPropertyQuery, \
    NamedPropertyQueryCallback, \
    v8::Integer, \
    v8::Local<v8::String>) \
  F(NamedPropertyDeleter, \
    NamedPropertyDeleterCallback, \
    v8::Boolean, \
    v8::Local<v8::String>) \
  F(IndexedPropertyGetter, \
    IndexedPropertyGetterCallback, \
    v8::Value, \
    uint32_t) \
  F(IndexedPropertyQuery, \
    IndexedPropertyQueryCallback, \
    v8::Integer, \
    uint32_t) \
  F(IndexedPropertyDeleter, \
    IndexedPropertyDeleterCallback, \
    v8::Boolean, \
    uint32_t) \

#define FOR_EACH_CALLBACK_TABLE_MAPPING_2(F) \
  F(NamedPropertySetter, \
    NamedPropertySetterCallback, \
    v8::Value, \
    v8::Local<v8::String>, \
    v8::Local<v8::Value>) \
  F(IndexedPropertySetter, \
    IndexedPropertySetterCallback, \
    v8::Value, \
    uint32_t, \
    v8::Local<v8::Value>) \

#define FOR_EACH_CALLBACK_TABLE_MAPPING_2_VOID_RETURN(F) \
  F(AccessorSetter, \
    AccessorSetterCallback, \
    void, \
    v8::Local<v8::String>, \
    v8::Local<v8::Value>) \

// All property callbacks as well as invocation callbacks
#define FOR_EACH_CALLBACK_TABLE_MAPPING(F) \
  F(InvocationCallback, FunctionCallback) \
  F(AccessorGetter, AccessorGetterCallback) \
  F(AccessorSetter, AccessorSetterCallback) \
  F(NamedPropertySetter, NamedPropertySetterCallback) \
  F(NamedPropertyQuery, NamedPropertyQueryCallback) \
  F(NamedPropertyDeleter, NamedPropertyDeleterCallback) \
  F(IndexedPropertyGetter, IndexedPropertyGetterCallback) \
  F(IndexedPropertySetter, IndexedPropertySetterCallback) \
  F(IndexedPropertyQuery, IndexedPropertyQueryCallback) \
  F(IndexedPropertyDeleter, IndexedPropertyDeleterCallback) \
  F(IndexedPropertyEnumerator, IndexedPropertyEnumeratorCallback) \


// TODO(dcarney): Remove this class when old callbacks are gone.
class CallbackTable {
 public:
  // TODO(dcarney): Flip this when it makes sense for performance.
  static const bool kStoreVoidFunctions = true;
  static inline bool ReturnsVoid(Isolate* isolate, void* function) {
    CallbackTable* table = isolate->callback_table();
    bool contains =
        table != NULL &&
        table->map_.occupancy() != 0 &&
        table->Contains(function);
    return contains == kStoreVoidFunctions;
  }

  STATIC_ASSERT(sizeof(intptr_t) == sizeof(AccessorGetterCallback));

  template<typename F>
  static inline void* FunctionToVoidPtr(F function) {
    return reinterpret_cast<void*>(reinterpret_cast<intptr_t>(function));
  }

#define WRITE_REGISTER(OldFunction, NewFunction)                    \
  static OldFunction Register(Isolate* isolate, NewFunction f) {    \
    InsertCallback(isolate, FunctionToVoidPtr(f), true);            \
    return reinterpret_cast<OldFunction>(f);                        \
  }                                                                 \
                                                                    \
  static OldFunction Register(Isolate* isolate, OldFunction f) {    \
    InsertCallback(isolate, FunctionToVoidPtr(f), false);           \
    return f;                                                       \
  }
  FOR_EACH_CALLBACK_TABLE_MAPPING(WRITE_REGISTER)
#undef WRITE_REGISTER

 private:
  CallbackTable();
  bool Contains(void* function);
  static void InsertCallback(Isolate* isolate,
                             void* function,
                             bool returns_void);
  HashMap map_;
  DISALLOW_COPY_AND_ASSIGN(CallbackTable);
};


// Custom arguments replicate a small segment of stack that can be
// accessed through an Arguments object the same way the actual stack
// can.
template<int kArrayLength>
class CustomArgumentsBase : public Relocatable {
 public:
  virtual inline void IterateInstance(ObjectVisitor* v) {
    v->VisitPointers(values_, values_ + kArrayLength);
  }
 protected:
  inline Object** end() { return values_ + kArrayLength - 1; }
  explicit inline CustomArgumentsBase(Isolate* isolate)
      : Relocatable(isolate) {}
  Object* values_[kArrayLength];
};


template<typename T>
class CustomArguments : public CustomArgumentsBase<T::kArgsLength> {
 public:
  static const int kReturnValueOffset = T::kReturnValueIndex;

  typedef CustomArgumentsBase<T::kArgsLength> Super;
  ~CustomArguments() {
    // TODO(dcarney): create a new zap value for this.
    this->end()[kReturnValueOffset] =
        reinterpret_cast<Object*>(kHandleZapValue);
  }

 protected:
  explicit inline CustomArguments(Isolate* isolate) : Super(isolate) {}

  template<typename V>
  v8::Handle<V> GetReturnValue(Isolate* isolate);

  inline Isolate* isolate() {
    return reinterpret_cast<Isolate*>(this->end()[T::kIsolateIndex]);
  }
};


class PropertyCallbackArguments
    : public CustomArguments<PropertyCallbackInfo<Value> > {
 public:
  typedef PropertyCallbackInfo<Value> T;
  typedef CustomArguments<T> Super;
  static const int kArgsLength = T::kArgsLength;
  static const int kThisIndex = T::kThisIndex;
  static const int kHolderIndex = T::kHolderIndex;

  PropertyCallbackArguments(Isolate* isolate,
                            Object* data,
                            Object* self,
                            JSObject* holder)
      : Super(isolate) {
    Object** values = this->end();
    values[T::kThisIndex] = self;
    values[T::kHolderIndex] = holder;
    values[T::kDataIndex] = data;
    values[T::kIsolateIndex] = reinterpret_cast<Object*>(isolate);
    values[T::kReturnValueIndex] = isolate->heap()->the_hole_value();
    ASSERT(values[T::kHolderIndex]->IsHeapObject());
    ASSERT(values[T::kIsolateIndex]->IsSmi());
  }

  /*
   * The following Call functions wrap the calling of all callbacks to handle
   * calling either the old or the new style callbacks depending on which one
   * has been registered.
   * For old callbacks which return an empty handle, the ReturnValue is checked
   * and used if it's been set to anything inside the callback.
   * New style callbacks always use the return value.
   */
#define WRITE_CALL_0(OldFunction, NewFunction, ReturnValue)                  \
  v8::Handle<ReturnValue> Call(OldFunction f);                               \

#define WRITE_CALL_1(OldFunction, NewFunction, ReturnValue, Arg1)            \
  v8::Handle<ReturnValue> Call(OldFunction f, Arg1 arg1);                    \

#define WRITE_CALL_2(OldFunction, NewFunction, ReturnValue, Arg1, Arg2)      \
  v8::Handle<ReturnValue> Call(OldFunction f, Arg1 arg1, Arg2 arg2);         \

#define WRITE_CALL_2_VOID(OldFunction, NewFunction, ReturnValue, Arg1, Arg2) \
  void Call(OldFunction f, Arg1 arg1, Arg2 arg2);                            \

FOR_EACH_CALLBACK_TABLE_MAPPING_0(WRITE_CALL_0)
FOR_EACH_CALLBACK_TABLE_MAPPING_1(WRITE_CALL_1)
FOR_EACH_CALLBACK_TABLE_MAPPING_2(WRITE_CALL_2)
FOR_EACH_CALLBACK_TABLE_MAPPING_2_VOID_RETURN(WRITE_CALL_2_VOID)

#undef WRITE_CALL_0
#undef WRITE_CALL_1
#undef WRITE_CALL_2
#undef WRITE_CALL_2_VOID
};


class FunctionCallbackArguments
    : public CustomArguments<FunctionCallbackInfo<Value> > {
 public:
  typedef FunctionCallbackInfo<Value> T;
  typedef CustomArguments<T> Super;
  static const int kArgsLength = T::kArgsLength;

  FunctionCallbackArguments(internal::Isolate* isolate,
      internal::Object* data,
      internal::JSFunction* callee,
      internal::Object* holder,
      internal::Object** argv,
      int argc,
      bool is_construct_call)
        : Super(isolate),
          argv_(argv),
          argc_(argc),
          is_construct_call_(is_construct_call) {
    Object** values = end();
    values[T::kDataIndex] = data;
    values[T::kCalleeIndex] = callee;
    values[T::kHolderIndex] = holder;
    values[T::kIsolateIndex] = reinterpret_cast<internal::Object*>(isolate);
    values[T::kReturnValueIndex] = isolate->heap()->the_hole_value();
    ASSERT(values[T::kCalleeIndex]->IsJSFunction());
    ASSERT(values[T::kHolderIndex]->IsHeapObject());
    ASSERT(values[T::kIsolateIndex]->IsSmi());
  }

  /*
   * The following Call function wraps the calling of all callbacks to handle
   * calling either the old or the new style callbacks depending on which one
   * has been registered.
   * For old callbacks which return an empty handle, the ReturnValue is checked
   * and used if it's been set to anything inside the callback.
   * New style callbacks always use the return value.
   */
  v8::Handle<v8::Value> Call(InvocationCallback f);

 private:
  internal::Object** argv_;
  int argc_;
  bool is_construct_call_;
};


#define DECLARE_RUNTIME_FUNCTION(Type, Name)    \
Type Name(int args_length, Object** args_object, Isolate* isolate)

#define RUNTIME_FUNCTION(Type, Name)                                  \
static Type __RT_impl_##Name(Arguments args, Isolate* isolate);       \
Type Name(int args_length, Object** args_object, Isolate* isolate) {  \
  Arguments args(args_length, args_object);                           \
  return __RT_impl_##Name(args, isolate);                             \
}                                                                     \
static Type __RT_impl_##Name(Arguments args, Isolate* isolate)

#define RUNTIME_ARGUMENTS(isolate, args) \
  args.length(), args.arguments(), isolate

} }  // namespace v8::internal

#endif  // V8_ARGUMENTS_H_
