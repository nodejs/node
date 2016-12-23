// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_ASMJS_ASM_TYPES_H_
#define SRC_ASMJS_ASM_TYPES_H_

#include <string>

#include "src/base/macros.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace wasm {

class AsmType;
class AsmFFIType;
class AsmFunctionType;
class AsmOverloadedFunctionType;
class AsmFunctionTableType;

// List of V(CamelName, string_name, number, parent_types)
#define FOR_EACH_ASM_VALUE_TYPE_LIST(V)                                       \
  /* These tags are not types that are expressable in the asm source. They */ \
  /* are used to express semantic information about the types they tag.    */ \
  V(Heap, "[]", 1, 0)                                                         \
  V(FloatishDoubleQ, "floatish|double?", 2, 0)                                \
  V(FloatQDoubleQ, "float?|double?", 3, 0)                                    \
  /* The following are actual types that appear in the asm source. */         \
  V(Void, "void", 4, 0)                                                       \
  V(Extern, "extern", 5, 0)                                                   \
  V(DoubleQ, "double?", 6, kAsmFloatishDoubleQ | kAsmFloatQDoubleQ)           \
  V(Double, "double", 7, kAsmDoubleQ | kAsmExtern)                            \
  V(Intish, "intish", 8, 0)                                                   \
  V(Int, "int", 9, kAsmIntish)                                                \
  V(Signed, "signed", 10, kAsmInt | kAsmExtern)                               \
  V(Unsigned, "unsigned", 11, kAsmInt)                                        \
  V(FixNum, "fixnum", 12, kAsmSigned | kAsmUnsigned)                          \
  V(Floatish, "floatish", 13, kAsmFloatishDoubleQ)                            \
  V(FloatQ, "float?", 14, kAsmFloatQDoubleQ | kAsmFloatish)                   \
  V(Float, "float", 15, kAsmFloatQ)                                           \
  /* Types used for expressing the Heap accesses. */                          \
  V(Uint8Array, "Uint8Array", 16, kAsmHeap)                                   \
  V(Int8Array, "Int8Array", 17, kAsmHeap)                                     \
  V(Uint16Array, "Uint16Array", 18, kAsmHeap)                                 \
  V(Int16Array, "Int16Array", 19, kAsmHeap)                                   \
  V(Uint32Array, "Uint32Array", 20, kAsmHeap)                                 \
  V(Int32Array, "Int32Array", 21, kAsmHeap)                                   \
  V(Float32Array, "Float32Array", 22, kAsmHeap)                               \
  V(Float64Array, "Float64Array", 23, kAsmHeap)                               \
  /* None is used to represent errors in the type checker. */                 \
  V(None, "<none>", 31, 0)

// List of V(CamelName)
#define FOR_EACH_ASM_CALLABLE_TYPE_LIST(V) \
  V(FunctionType)                          \
  V(FFIType)                               \
  V(OverloadedFunctionType)                \
  V(FunctionTableType)

class AsmValueType {
 public:
  typedef uint32_t bitset_t;

  enum : uint32_t {
#define DEFINE_TAG(CamelName, string_name, number, parent_types) \
  kAsm##CamelName = ((1u << (number)) | (parent_types)),
    FOR_EACH_ASM_VALUE_TYPE_LIST(DEFINE_TAG)
#undef DEFINE_TAG
        kAsmUnknown = 0,
    kAsmValueTypeTag = 1u
  };

 private:
  friend class AsmType;

  static AsmValueType* AsValueType(AsmType* type) {
    if ((reinterpret_cast<uintptr_t>(type) & kAsmValueTypeTag) ==
        kAsmValueTypeTag) {
      return reinterpret_cast<AsmValueType*>(type);
    }
    return nullptr;
  }

  bitset_t Bitset() const {
    DCHECK((reinterpret_cast<uintptr_t>(this) & kAsmValueTypeTag) ==
           kAsmValueTypeTag);
    return static_cast<bitset_t>(reinterpret_cast<uintptr_t>(this) &
                                 ~kAsmValueTypeTag);
  }

  static AsmType* New(bitset_t bits) {
    DCHECK_EQ((bits & kAsmValueTypeTag), 0);
    return reinterpret_cast<AsmType*>(
        static_cast<uintptr_t>(bits | kAsmValueTypeTag));
  }

  // AsmValueTypes can't be created except through AsmValueType::New.
  DISALLOW_IMPLICIT_CONSTRUCTORS(AsmValueType);
};

class AsmCallableType : public ZoneObject {
 public:
  virtual std::string Name() = 0;

  virtual bool CanBeInvokedWith(AsmType* return_type,
                                const ZoneVector<AsmType*>& args) = 0;

#define DECLARE_CAST(CamelName) \
  virtual Asm##CamelName* As##CamelName() { return nullptr; }
  FOR_EACH_ASM_CALLABLE_TYPE_LIST(DECLARE_CAST)
#undef DECLARE_CAST

 protected:
  AsmCallableType() = default;
  virtual ~AsmCallableType() = default;
  virtual bool IsA(AsmType* other);

 private:
  friend class AsmType;

  DISALLOW_COPY_AND_ASSIGN(AsmCallableType);
};

class AsmFunctionType final : public AsmCallableType {
 public:
  AsmFunctionType* AsFunctionType() final { return this; }

  void AddArgument(AsmType* type) { args_.push_back(type); }
  const ZoneVector<AsmType*> Arguments() const { return args_; }
  AsmType* ReturnType() const { return return_type_; }

  bool CanBeInvokedWith(AsmType* return_type,
                        const ZoneVector<AsmType*>& args) override;

 protected:
  AsmFunctionType(Zone* zone, AsmType* return_type)
      : return_type_(return_type), args_(zone) {}

 private:
  friend AsmType;

  std::string Name() override;
  bool IsA(AsmType* other) override;

  AsmType* return_type_;
  ZoneVector<AsmType*> args_;

  DISALLOW_COPY_AND_ASSIGN(AsmFunctionType);
};

class AsmOverloadedFunctionType final : public AsmCallableType {
 public:
  AsmOverloadedFunctionType* AsOverloadedFunctionType() override {
    return this;
  }

  void AddOverload(AsmType* overload);

 private:
  friend AsmType;

  explicit AsmOverloadedFunctionType(Zone* zone) : overloads_(zone) {}

  std::string Name() override;
  bool CanBeInvokedWith(AsmType* return_type,
                        const ZoneVector<AsmType*>& args) override;

  ZoneVector<AsmType*> overloads_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AsmOverloadedFunctionType);
};

class AsmFFIType final : public AsmCallableType {
 public:
  AsmFFIType* AsFFIType() override { return this; }

  std::string Name() override { return "Function"; }
  bool CanBeInvokedWith(AsmType* return_type,
                        const ZoneVector<AsmType*>& args) override;

 private:
  friend AsmType;

  AsmFFIType() = default;

  DISALLOW_COPY_AND_ASSIGN(AsmFFIType);
};

class AsmFunctionTableType : public AsmCallableType {
 public:
  AsmFunctionTableType* AsFunctionTableType() override { return this; }

  std::string Name() override;

  bool CanBeInvokedWith(AsmType* return_type,
                        const ZoneVector<AsmType*>& args) override;

  size_t length() const { return length_; }
  AsmType* signature() { return signature_; }

 private:
  friend class AsmType;

  AsmFunctionTableType(size_t length, AsmType* signature);

  size_t length_;
  AsmType* signature_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(AsmFunctionTableType);
};

class AsmType {
 public:
#define DEFINE_CONSTRUCTOR(CamelName, string_name, number, parent_types) \
  static AsmType* CamelName() {                                          \
    return AsmValueType::New(AsmValueType::kAsm##CamelName);             \
  }
  FOR_EACH_ASM_VALUE_TYPE_LIST(DEFINE_CONSTRUCTOR)
#undef DEFINE_CONSTRUCTOR

#define DEFINE_CAST(CamelCase)                                        \
  Asm##CamelCase* As##CamelCase() {                                   \
    if (AsValueType() != nullptr) {                                   \
      return nullptr;                                                 \
    }                                                                 \
    return reinterpret_cast<AsmCallableType*>(this)->As##CamelCase(); \
  }
  FOR_EACH_ASM_CALLABLE_TYPE_LIST(DEFINE_CAST)
#undef DEFINE_CAST
  AsmValueType* AsValueType() { return AsmValueType::AsValueType(this); }
  AsmCallableType* AsCallableType();

  // A function returning ret. Callers still need to invoke AddArgument with the
  // returned type to fully create this type.
  static AsmType* Function(Zone* zone, AsmType* ret) {
    AsmFunctionType* f = new (zone) AsmFunctionType(zone, ret);
    return reinterpret_cast<AsmType*>(f);
  }

  // Overloaded function types. Not creatable by asm source, but useful to
  // represent the overloaded stdlib functions.
  static AsmType* OverloadedFunction(Zone* zone) {
    auto* f = new (zone) AsmOverloadedFunctionType(zone);
    return reinterpret_cast<AsmType*>(f);
  }

  // The type for fround(src).
  static AsmType* FroundType(Zone* zone);

  // The (variadic) type for min and max.
  static AsmType* MinMaxType(Zone* zone, AsmType* dest, AsmType* src);

  // The type for foreign functions.
  static AsmType* FFIType(Zone* zone) {
    auto* f = new (zone) AsmFFIType();
    return reinterpret_cast<AsmType*>(f);
  }

  // The type for function tables.
  static AsmType* FunctionTableType(Zone* zone, size_t length,
                                    AsmType* signature) {
    auto* f = new (zone) AsmFunctionTableType(length, signature);
    return reinterpret_cast<AsmType*>(f);
  }

  std::string Name();
  // IsExactly returns true if this is the exact same type as that. For
  // non-value types (e.g., callables), this returns this == that.
  bool IsExactly(AsmType* that);
  // IsA is used to query whether this is an instance of that (i.e., if this is
  // a type derived from that.) For non-value types (e.g., callables), this
  // returns this == that.
  bool IsA(AsmType* that);

  // Types allowed in return statements. void is the type for returns without
  // an expression.
  bool IsReturnType() {
    return this == AsmType::Void() || this == AsmType::Double() ||
           this == AsmType::Signed() || this == AsmType::Float();
  }

  // Converts this to the corresponding valid argument type.
  AsmType* ToReturnType() {
    if (this->IsA(AsmType::Signed())) {
      return AsmType::Signed();
    }
    if (this->IsA(AsmType::Double())) {
      return AsmType::Double();
    }
    if (this->IsA(AsmType::Float())) {
      return AsmType::Float();
    }
    if (this->IsA(AsmType::Void())) {
      return AsmType::Void();
    }
    return AsmType::None();
  }

  // Types allowed to be parameters in asm functions.
  bool IsParameterType() {
    return this == AsmType::Double() || this == AsmType::Int() ||
           this == AsmType::Float();
  }

  // Converts this to the corresponding valid argument type.
  AsmType* ToParameterType() {
    if (this->IsA(AsmType::Int())) {
      return AsmType::Int();
    }
    if (this->IsA(AsmType::Double())) {
      return AsmType::Double();
    }
    if (this->IsA(AsmType::Float())) {
      return AsmType::Float();
    }
    return AsmType::None();
  }

  // Types allowed to be compared using the comparison operators.
  bool IsComparableType() {
    return this == AsmType::Double() || this == AsmType::Signed() ||
           this == AsmType::Unsigned() || this == AsmType::Float();
  }

  // The following methods are meant to be used for inspecting the traits of
  // element types for the heap view types.
  enum : int32_t { kNotHeapType = -1 };

  // Returns the element size if this is a heap type. Otherwise returns
  // kNotHeapType.
  int32_t ElementSizeInBytes();
  // Returns the load type if this is a heap type. AsmType::None is returned if
  // this is not a heap type.
  AsmType* LoadType();
  // Returns the store type if this is a heap type. AsmType::None is returned if
  // this is not a heap type.
  AsmType* StoreType();
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // SRC_ASMJS_ASM_TYPES_H_
