// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_RUNTIME_RUNTIME_UTILS_H_
#define V8_RUNTIME_RUNTIME_UTILS_H_

#include "src/runtime/runtime.h"

namespace v8 {
namespace internal {

#define RUNTIME_ASSERT(value) \
  if (!(value)) return isolate->ThrowIllegalOperation();

#define RUNTIME_ASSERT_HANDLIFIED(value, T) \
  if (!(value)) {                           \
    isolate->ThrowIllegalOperation();       \
    return MaybeHandle<T>();                \
  }

// Cast the given object to a value of the specified type and store
// it in a variable with the given name.  If the object is not of the
// expected type call IllegalOperation and return.
#define CONVERT_ARG_CHECKED(Type, name, index) \
  RUNTIME_ASSERT(args[index]->Is##Type());     \
  Type* name = Type::cast(args[index]);

#define CONVERT_ARG_HANDLE_CHECKED(Type, name, index) \
  RUNTIME_ASSERT(args[index]->Is##Type());            \
  Handle<Type> name = args.at<Type>(index);

#define CONVERT_NUMBER_ARG_HANDLE_CHECKED(name, index) \
  RUNTIME_ASSERT(args[index]->IsNumber());             \
  Handle<Object> name = args.at<Object>(index);

// Cast the given object to a boolean and store it in a variable with
// the given name.  If the object is not a boolean call IllegalOperation
// and return.
#define CONVERT_BOOLEAN_ARG_CHECKED(name, index) \
  RUNTIME_ASSERT(args[index]->IsBoolean());      \
  bool name = args[index]->IsTrue();

// Cast the given argument to a Smi and store its value in an int variable
// with the given name.  If the argument is not a Smi call IllegalOperation
// and return.
#define CONVERT_SMI_ARG_CHECKED(name, index) \
  RUNTIME_ASSERT(args[index]->IsSmi());      \
  int name = args.smi_at(index);

// Cast the given argument to a double and store it in a variable with
// the given name.  If the argument is not a number (as opposed to
// the number not-a-number) call IllegalOperation and return.
#define CONVERT_DOUBLE_ARG_CHECKED(name, index) \
  RUNTIME_ASSERT(args[index]->IsNumber());      \
  double name = args.number_at(index);


// Cast the given argument to a size_t and store its value in a variable with
// the given name.  If the argument is not a size_t call IllegalOperation and
// return.
#define CONVERT_SIZE_ARG_CHECKED(name, index)            \
  RUNTIME_ASSERT(args[index]->IsNumber());               \
  Handle<Object> name##_object = args.at<Object>(index); \
  size_t name = 0;                                       \
  RUNTIME_ASSERT(TryNumberToSize(isolate, *name##_object, &name));


// Call the specified converter on the object *comand store the result in
// a variable of the specified type with the given name.  If the
// object is not a Number call IllegalOperation and return.
#define CONVERT_NUMBER_CHECKED(type, name, Type, obj) \
  RUNTIME_ASSERT(obj->IsNumber());                    \
  type name = NumberTo##Type(obj);


// Cast the given argument to PropertyDetails and store its value in a
// variable with the given name.  If the argument is not a Smi call
// IllegalOperation and return.
#define CONVERT_PROPERTY_DETAILS_CHECKED(name, index) \
  RUNTIME_ASSERT(args[index]->IsSmi());               \
  PropertyDetails name = PropertyDetails(Smi::cast(args[index]));


// Assert that the given argument has a valid value for a LanguageMode
// and store it in a LanguageMode variable with the given name.
#define CONVERT_LANGUAGE_MODE_ARG_CHECKED(name, index)        \
  RUNTIME_ASSERT(args[index]->IsSmi());                       \
  RUNTIME_ASSERT(is_valid_language_mode(args.smi_at(index))); \
  LanguageMode name = static_cast<LanguageMode>(args.smi_at(index));


// Assert that the given argument is a number within the Int32 range
// and convert it to int32_t.  If the argument is not an Int32 call
// IllegalOperation and return.
#define CONVERT_INT32_ARG_CHECKED(name, index) \
  RUNTIME_ASSERT(args[index]->IsNumber());     \
  int32_t name = 0;                            \
  RUNTIME_ASSERT(args[index]->ToInt32(&name));


// Cast the given argument to PropertyAttributes and store its value in a
// variable with the given name.  If the argument is not a Smi call or the
// enum value is out of range, call IllegalOperation and return.
#define CONVERT_PROPERTY_ATTRIBUTES_CHECKED(name, index)                   \
  RUNTIME_ASSERT(args[index]->IsSmi());                                    \
  RUNTIME_ASSERT(                                                          \
      (args.smi_at(index) & ~(READ_ONLY | DONT_ENUM | DONT_DELETE)) == 0); \
  PropertyAttributes name = static_cast<PropertyAttributes>(args.smi_at(index));


// A mechanism to return a pair of Object pointers in registers (if possible).
// How this is achieved is calling convention-dependent.
// All currently supported x86 compiles uses calling conventions that are cdecl
// variants where a 64-bit value is returned in two 32-bit registers
// (edx:eax on ia32, r1:r0 on ARM).
// In AMD-64 calling convention a struct of two pointers is returned in rdx:rax.
// In Win64 calling convention, a struct of two pointers is returned in memory,
// allocated by the caller, and passed as a pointer in a hidden first parameter.
#ifdef V8_HOST_ARCH_64_BIT
struct ObjectPair {
  Object* x;
  Object* y;
};


static inline ObjectPair MakePair(Object* x, Object* y) {
  ObjectPair result = {x, y};
  // Pointers x and y returned in rax and rdx, in AMD-x64-abi.
  // In Win64 they are assigned to a hidden first argument.
  return result;
}
#elif V8_TARGET_ARCH_X64 && V8_TARGET_ARCH_32_BIT
// For x32 a 128-bit struct return is done as rax and rdx from the ObjectPair
// are used in the full codegen and Crankshaft compiler. An alternative is
// using uint64_t and modifying full codegen and Crankshaft compiler.
struct ObjectPair {
  Object* x;
  uint32_t x_upper;
  Object* y;
  uint32_t y_upper;
};


static inline ObjectPair MakePair(Object* x, Object* y) {
  ObjectPair result = {x, 0, y, 0};
  // Pointers x and y returned in rax and rdx, in x32-abi.
  return result;
}
#else
typedef uint64_t ObjectPair;
static inline ObjectPair MakePair(Object* x, Object* y) {
#if defined(V8_TARGET_LITTLE_ENDIAN)
  return reinterpret_cast<uint32_t>(x) |
         (reinterpret_cast<ObjectPair>(y) << 32);
#elif defined(V8_TARGET_BIG_ENDIAN)
  return reinterpret_cast<uint32_t>(y) |
         (reinterpret_cast<ObjectPair>(x) << 32);
#else
#error Unknown endianness
#endif
}
#endif


// A mechanism to return a triple of Object pointers. In all calling
// conventions, a struct of two pointers is returned in memory,
// allocated by the caller, and passed as a pointer in a hidden first parameter.
struct ObjectTriple {
  Object* x;
  Object* y;
  Object* z;
};

static inline ObjectTriple MakeTriple(Object* x, Object* y, Object* z) {
  ObjectTriple result = {x, y, z};
  // ObjectTriple is assigned to a hidden first argument.
  return result;
}

}  // namespace internal
}  // namespace v8

#endif  // V8_RUNTIME_RUNTIME_UTILS_H_
