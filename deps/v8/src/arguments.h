// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARGUMENTS_H_
#define V8_ARGUMENTS_H_

#include "src/allocation.h"
#include "src/isolate.h"

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
//
// Note that length_ (whose value is in the integer range) is defined
// as intptr_t to provide endian-neutrality on 64-bit archs.

class Arguments BASE_EMBEDDED {
 public:
  Arguments(int length, Object** arguments)
      : length_(length), arguments_(arguments) { }

  Object*& operator[] (int index) {
    DCHECK(0 <= index && index < length_);
    return *(reinterpret_cast<Object**>(reinterpret_cast<intptr_t>(arguments_) -
                                        index * kPointerSize));
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
  int length() const { return static_cast<int>(length_); }

  Object** arguments() { return arguments_; }

  Object** lowest_address() { return &this->operator[](length() - 1); }

  Object** highest_address() { return &this->operator[](0); }

 private:
  intptr_t length_;
  Object** arguments_;
};


// For each type of callback, we have a list of arguments
// They are used to generate the Call() functions below
// These aren't included in the list as they have duplicate signatures
// F(GenericNamedPropertyEnumeratorCallback, ...)
// F(GenericNamedPropertyGetterCallback, ...)

#define FOR_EACH_CALLBACK_TABLE_MAPPING_0(F) \
  F(IndexedPropertyEnumeratorCallback, v8::Array)

#define FOR_EACH_CALLBACK_TABLE_MAPPING_1(F)                               \
  F(AccessorNameGetterCallback, v8::Value, v8::Local<v8::Name>)            \
  F(GenericNamedPropertyQueryCallback, v8::Integer, v8::Local<v8::Name>)   \
  F(GenericNamedPropertyDeleterCallback, v8::Boolean, v8::Local<v8::Name>) \
  F(IndexedPropertyGetterCallback, v8::Value, uint32_t)                    \
  F(IndexedPropertyQueryCallback, v8::Integer, uint32_t)                   \
  F(IndexedPropertyDeleterCallback, v8::Boolean, uint32_t)

#define FOR_EACH_CALLBACK_TABLE_MAPPING_2(F)                            \
  F(GenericNamedPropertySetterCallback, v8::Value, v8::Local<v8::Name>, \
    v8::Local<v8::Value>)                                               \
  F(IndexedPropertySetterCallback, v8::Value, uint32_t, v8::Local<v8::Value>)

#define FOR_EACH_CALLBACK_TABLE_MAPPING_2_VOID_RETURN(F) \
  F(AccessorNameSetterCallback, \
    void, \
    v8::Local<v8::Name>, \
    v8::Local<v8::Value>) \


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
  inline Object** begin() { return values_; }
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
    this->begin()[kReturnValueOffset] =
        reinterpret_cast<Object*>(kHandleZapValue);
  }

 protected:
  explicit inline CustomArguments(Isolate* isolate) : Super(isolate) {}

  template<typename V>
  v8::Handle<V> GetReturnValue(Isolate* isolate);

  inline Isolate* isolate() {
    return reinterpret_cast<Isolate*>(this->begin()[T::kIsolateIndex]);
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
  static const int kDataIndex = T::kDataIndex;
  static const int kReturnValueDefaultValueIndex =
      T::kReturnValueDefaultValueIndex;
  static const int kIsolateIndex = T::kIsolateIndex;

  PropertyCallbackArguments(Isolate* isolate,
                            Object* data,
                            Object* self,
                            JSObject* holder)
      : Super(isolate) {
    Object** values = this->begin();
    values[T::kThisIndex] = self;
    values[T::kHolderIndex] = holder;
    values[T::kDataIndex] = data;
    values[T::kIsolateIndex] = reinterpret_cast<Object*>(isolate);
    // Here the hole is set as default value.
    // It cannot escape into js as it's remove in Call below.
    values[T::kReturnValueDefaultValueIndex] =
        isolate->heap()->the_hole_value();
    values[T::kReturnValueIndex] = isolate->heap()->the_hole_value();
    DCHECK(values[T::kHolderIndex]->IsHeapObject());
    DCHECK(values[T::kIsolateIndex]->IsSmi());
  }

  /*
   * The following Call functions wrap the calling of all callbacks to handle
   * calling either the old or the new style callbacks depending on which one
   * has been registered.
   * For old callbacks which return an empty handle, the ReturnValue is checked
   * and used if it's been set to anything inside the callback.
   * New style callbacks always use the return value.
   */
#define WRITE_CALL_0(Function, ReturnValue)                                  \
  v8::Handle<ReturnValue> Call(Function f);                                  \

#define WRITE_CALL_1(Function, ReturnValue, Arg1)                            \
  v8::Handle<ReturnValue> Call(Function f, Arg1 arg1);                       \

#define WRITE_CALL_2(Function, ReturnValue, Arg1, Arg2)                      \
  v8::Handle<ReturnValue> Call(Function f, Arg1 arg1, Arg2 arg2);            \

#define WRITE_CALL_2_VOID(Function, ReturnValue, Arg1, Arg2)                 \
  void Call(Function f, Arg1 arg1, Arg2 arg2);                               \

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
  static const int kHolderIndex = T::kHolderIndex;
  static const int kDataIndex = T::kDataIndex;
  static const int kReturnValueDefaultValueIndex =
      T::kReturnValueDefaultValueIndex;
  static const int kIsolateIndex = T::kIsolateIndex;
  static const int kCalleeIndex = T::kCalleeIndex;
  static const int kContextSaveIndex = T::kContextSaveIndex;

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
    Object** values = begin();
    values[T::kDataIndex] = data;
    values[T::kCalleeIndex] = callee;
    values[T::kHolderIndex] = holder;
    values[T::kContextSaveIndex] = isolate->heap()->the_hole_value();
    values[T::kIsolateIndex] = reinterpret_cast<internal::Object*>(isolate);
    // Here the hole is set as default value.
    // It cannot escape into js as it's remove in Call below.
    values[T::kReturnValueDefaultValueIndex] =
        isolate->heap()->the_hole_value();
    values[T::kReturnValueIndex] = isolate->heap()->the_hole_value();
    DCHECK(values[T::kCalleeIndex]->IsJSFunction());
    DCHECK(values[T::kHolderIndex]->IsHeapObject());
    DCHECK(values[T::kIsolateIndex]->IsSmi());
  }

  /*
   * The following Call function wraps the calling of all callbacks to handle
   * calling either the old or the new style callbacks depending on which one
   * has been registered.
   * For old callbacks which return an empty handle, the ReturnValue is checked
   * and used if it's been set to anything inside the callback.
   * New style callbacks always use the return value.
   */
  v8::Handle<v8::Value> Call(FunctionCallback f);

 private:
  internal::Object** argv_;
  int argc_;
  bool is_construct_call_;
};


double ClobberDoubleRegisters(double x1, double x2, double x3, double x4);


#ifdef DEBUG
#define CLOBBER_DOUBLE_REGISTERS() ClobberDoubleRegisters(1, 2, 3, 4);
#else
#define CLOBBER_DOUBLE_REGISTERS()
#endif


#define DECLARE_RUNTIME_FUNCTION(Name)    \
Object* Name(int args_length, Object** args_object, Isolate* isolate)

#define RUNTIME_FUNCTION_RETURNS_TYPE(Type, Name)                        \
static INLINE(Type __RT_impl_##Name(Arguments args, Isolate* isolate));  \
Type Name(int args_length, Object** args_object, Isolate* isolate) {     \
  CLOBBER_DOUBLE_REGISTERS();                                            \
  Arguments args(args_length, args_object);                              \
  return __RT_impl_##Name(args, isolate);                                \
}                                                                        \
static Type __RT_impl_##Name(Arguments args, Isolate* isolate)


#define RUNTIME_FUNCTION(Name) RUNTIME_FUNCTION_RETURNS_TYPE(Object*, Name)
#define RUNTIME_FUNCTION_RETURN_PAIR(Name) \
    RUNTIME_FUNCTION_RETURNS_TYPE(ObjectPair, Name)

#define RUNTIME_ARGUMENTS(isolate, args) \
  args.length(), args.arguments(), isolate

} }  // namespace v8::internal

#endif  // V8_ARGUMENTS_H_
