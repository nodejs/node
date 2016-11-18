// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-types.h"

#include <cinttypes>

#include "src/v8.h"

namespace v8 {
namespace internal {
namespace wasm {

AsmCallableType* AsmType::AsCallableType() {
  if (AsValueType() != nullptr) {
    return nullptr;
  }

  return reinterpret_cast<AsmCallableType*>(this);
}

std::string AsmType::Name() {
  AsmValueType* avt = this->AsValueType();
  if (avt != nullptr) {
    switch (avt->Bitset()) {
#define RETURN_TYPE_NAME(CamelName, string_name, number, parent_types) \
  case AsmValueType::kAsm##CamelName:                                  \
    return string_name;
      FOR_EACH_ASM_VALUE_TYPE_LIST(RETURN_TYPE_NAME)
#undef RETURN_TYPE_NAME
      default:
        UNREACHABLE();
    }
  }

  return this->AsCallableType()->Name();
}

bool AsmType::IsExactly(AsmType* that) {
  // TODO(jpp): maybe this can become this == that.
  AsmValueType* avt = this->AsValueType();
  if (avt != nullptr) {
    AsmValueType* tavt = that->AsValueType();
    if (tavt == nullptr) {
      return false;
    }
    return avt->Bitset() == tavt->Bitset();
  }

  // TODO(jpp): is it useful to allow non-value types to be tested with
  // IsExactly?
  return that == this;
}

bool AsmType::IsA(AsmType* that) {
  // IsA is used for querying inheritance relationships. Therefore it is only
  // meaningful for basic types.
  if (auto* avt = this->AsValueType()) {
    if (auto* tavt = that->AsValueType()) {
      return (avt->Bitset() & tavt->Bitset()) == tavt->Bitset();
    }
    return false;
  }

  if (auto* as_callable = this->AsCallableType()) {
    return as_callable->IsA(that);
  }

  UNREACHABLE();
  return that == this;
}

int32_t AsmType::ElementSizeInBytes() {
  auto* value = AsValueType();
  if (value == nullptr) {
    return AsmType::kNotHeapType;
  }
  switch (value->Bitset()) {
    case AsmValueType::kAsmInt8Array:
    case AsmValueType::kAsmUint8Array:
      return 1;
    case AsmValueType::kAsmInt16Array:
    case AsmValueType::kAsmUint16Array:
      return 2;
    case AsmValueType::kAsmInt32Array:
    case AsmValueType::kAsmUint32Array:
    case AsmValueType::kAsmFloat32Array:
      return 4;
    case AsmValueType::kAsmFloat64Array:
      return 8;
    default:
      return AsmType::kNotHeapType;
  }
}

AsmType* AsmType::LoadType() {
  auto* value = AsValueType();
  if (value == nullptr) {
    return AsmType::None();
  }
  switch (value->Bitset()) {
    case AsmValueType::kAsmInt8Array:
    case AsmValueType::kAsmUint8Array:
    case AsmValueType::kAsmInt16Array:
    case AsmValueType::kAsmUint16Array:
    case AsmValueType::kAsmInt32Array:
    case AsmValueType::kAsmUint32Array:
      return AsmType::Intish();
    case AsmValueType::kAsmFloat32Array:
      return AsmType::FloatQ();
    case AsmValueType::kAsmFloat64Array:
      return AsmType::DoubleQ();
    default:
      return AsmType::None();
  }
}

AsmType* AsmType::StoreType() {
  auto* value = AsValueType();
  if (value == nullptr) {
    return AsmType::None();
  }
  switch (value->Bitset()) {
    case AsmValueType::kAsmInt8Array:
    case AsmValueType::kAsmUint8Array:
    case AsmValueType::kAsmInt16Array:
    case AsmValueType::kAsmUint16Array:
    case AsmValueType::kAsmInt32Array:
    case AsmValueType::kAsmUint32Array:
      return AsmType::Intish();
    case AsmValueType::kAsmFloat32Array:
      return AsmType::FloatishDoubleQ();
    case AsmValueType::kAsmFloat64Array:
      return AsmType::FloatQDoubleQ();
    default:
      return AsmType::None();
  }
}

bool AsmCallableType::IsA(AsmType* other) {
  return other->AsCallableType() == this;
}

std::string AsmFunctionType::Name() {
  std::string ret;
  ret += "(";
  for (size_t ii = 0; ii < args_.size(); ++ii) {
    ret += args_[ii]->Name();
    if (ii != args_.size() - 1) {
      ret += ", ";
    }
  }
  ret += ") -> ";
  ret += return_type_->Name();
  return ret;
}

namespace {
class AsmFroundType final : public AsmCallableType {
 public:
  friend AsmType;

  AsmFroundType() : AsmCallableType() {}

  bool CanBeInvokedWith(AsmType* return_type,
                        const ZoneVector<AsmType*>& args) override;

  std::string Name() override { return "fround"; }
};
}  // namespace

AsmType* AsmType::FroundType(Zone* zone) {
  auto* Fround = new (zone) AsmFroundType();
  return reinterpret_cast<AsmType*>(Fround);
}

bool AsmFroundType::CanBeInvokedWith(AsmType* return_type,
                                     const ZoneVector<AsmType*>& args) {
  if (args.size() != 1) {
    return false;
  }

  auto* arg = args[0];
  if (!arg->IsA(AsmType::Floatish()) && !arg->IsA(AsmType::DoubleQ()) &&
      !arg->IsA(AsmType::Signed()) && !arg->IsA(AsmType::Unsigned())) {
    return false;
  }

  return true;
}

namespace {
class AsmMinMaxType final : public AsmCallableType {
 private:
  friend AsmType;

  AsmMinMaxType(AsmType* dest, AsmType* src)
      : AsmCallableType(), return_type_(dest), arg_(src) {}

  bool CanBeInvokedWith(AsmType* return_type,
                        const ZoneVector<AsmType*>& args) override {
    if (!return_type_->IsExactly(return_type)) {
      return false;
    }

    if (args.size() < 2) {
      return false;
    }

    for (size_t ii = 0; ii < args.size(); ++ii) {
      if (!args[ii]->IsA(arg_)) {
        return false;
      }
    }

    return true;
  }

  std::string Name() override {
    return "(" + arg_->Name() + ", " + arg_->Name() + "...) -> " +
           return_type_->Name();
  }

  AsmType* return_type_;
  AsmType* arg_;
};
}  // namespace

AsmType* AsmType::MinMaxType(Zone* zone, AsmType* dest, AsmType* src) {
  DCHECK(dest->AsValueType() != nullptr);
  DCHECK(src->AsValueType() != nullptr);
  auto* MinMax = new (zone) AsmMinMaxType(dest, src);
  return reinterpret_cast<AsmType*>(MinMax);
}

bool AsmFFIType::CanBeInvokedWith(AsmType* return_type,
                                  const ZoneVector<AsmType*>& args) {
  if (return_type->IsExactly(AsmType::Float())) {
    return false;
  }

  for (size_t ii = 0; ii < args.size(); ++ii) {
    if (!args[ii]->IsA(AsmType::Extern())) {
      return false;
    }
  }

  return true;
}

bool AsmFunctionType::IsA(AsmType* other) {
  auto* that = other->AsFunctionType();
  if (that == nullptr) {
    return false;
  }
  if (!return_type_->IsExactly(that->return_type_)) {
    return false;
  }

  if (args_.size() != that->args_.size()) {
    return false;
  }

  for (size_t ii = 0; ii < args_.size(); ++ii) {
    if (!args_[ii]->IsExactly(that->args_[ii])) {
      return false;
    }
  }

  return true;
}

bool AsmFunctionType::CanBeInvokedWith(AsmType* return_type,
                                       const ZoneVector<AsmType*>& args) {
  if (!return_type_->IsExactly(return_type)) {
    return false;
  }

  if (args_.size() != args.size()) {
    return false;
  }

  for (size_t ii = 0; ii < args_.size(); ++ii) {
    if (!args[ii]->IsA(args_[ii])) {
      return false;
    }
  }

  return true;
}

std::string AsmOverloadedFunctionType::Name() {
  std::string ret;

  for (size_t ii = 0; ii < overloads_.size(); ++ii) {
    if (ii != 0) {
      ret += " /\\ ";
    }
    ret += overloads_[ii]->Name();
  }

  return ret;
}

bool AsmOverloadedFunctionType::CanBeInvokedWith(
    AsmType* return_type, const ZoneVector<AsmType*>& args) {
  for (size_t ii = 0; ii < overloads_.size(); ++ii) {
    if (overloads_[ii]->AsCallableType()->CanBeInvokedWith(return_type, args)) {
      return true;
    }
  }

  return false;
}

void AsmOverloadedFunctionType::AddOverload(AsmType* overload) {
  DCHECK(overload->AsCallableType() != nullptr);
  overloads_.push_back(overload);
}

AsmFunctionTableType::AsmFunctionTableType(size_t length, AsmType* signature)
    : length_(length), signature_(signature) {
  DCHECK(signature_ != nullptr);
  DCHECK(signature_->AsFunctionType() != nullptr);
}

namespace {
// ToString is used for reporting function tables' names. It converts its
// argument to uint32_t because asm.js integers are 32-bits, thus effectively
// limiting the max function table's length.
std::string ToString(size_t s) {
  auto u32 = static_cast<uint32_t>(s);
  // 16 bytes is more than enough to represent a 32-bit integer as a base 10
  // string.
  char digits[16];
  int length = base::OS::SNPrintF(digits, arraysize(digits), "%" PRIu32, u32);
  DCHECK_NE(length, -1);
  return std::string(digits, length);
}
}  // namespace

std::string AsmFunctionTableType::Name() {
  return "(" + signature_->Name() + ")[" + ToString(length_) + "]";
}

bool AsmFunctionTableType::CanBeInvokedWith(AsmType* return_type,
                                            const ZoneVector<AsmType*>& args) {
  return signature_->AsCallableType()->CanBeInvokedWith(return_type, args);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
