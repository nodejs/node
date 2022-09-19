// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/asmjs/asm-types.h"

#include <cinttypes>

#include "src/init/v8.h"
#include "src/utils/utils.h"

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

bool AsmType::IsExactly(AsmType* x, AsmType* y) {
  // TODO(jpp): maybe this can become x == y.
  if (x == nullptr) return y == nullptr;
  AsmValueType* avt = x->AsValueType();
  if (avt != nullptr) {
    AsmValueType* tavt = y->AsValueType();
    if (tavt == nullptr) {
      return false;
    }
    return avt->Bitset() == tavt->Bitset();
  }

  // TODO(jpp): is it useful to allow non-value types to be tested with
  // IsExactly?
  return x == y;
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
  auto* Fround = zone->New<AsmFroundType>();
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
  friend Zone;

  AsmMinMaxType(AsmType* dest, AsmType* src)
      : AsmCallableType(), return_type_(dest), arg_(src) {}

  bool CanBeInvokedWith(AsmType* return_type,
                        const ZoneVector<AsmType*>& args) override {
    if (!AsmType::IsExactly(return_type_, return_type)) {
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
  DCHECK_NOT_NULL(dest->AsValueType());
  DCHECK_NOT_NULL(src->AsValueType());
  auto* MinMax = zone->New<AsmMinMaxType>(dest, src);
  return reinterpret_cast<AsmType*>(MinMax);
}

bool AsmFunctionType::IsA(AsmType* other) {
  auto* that = other->AsFunctionType();
  if (that == nullptr) {
    return false;
  }
  if (!AsmType::IsExactly(return_type_, that->return_type_)) {
    return false;
  }

  if (args_.size() != that->args_.size()) {
    return false;
  }

  for (size_t ii = 0; ii < args_.size(); ++ii) {
    if (!AsmType::IsExactly(args_[ii], that->args_[ii])) {
      return false;
    }
  }

  return true;
}

bool AsmFunctionType::CanBeInvokedWith(AsmType* return_type,
                                       const ZoneVector<AsmType*>& args) {
  if (!AsmType::IsExactly(return_type_, return_type)) {
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
  DCHECK_NOT_NULL(overload->AsCallableType());
  overloads_.push_back(overload);
}

}  // namespace wasm
}  // namespace internal
}  // namespace v8
