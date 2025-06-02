// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/value-type.h"

#include "src/codegen/signature.h"
#include "src/utils/utils.h"
#include "src/wasm/canonical-types.h"
#include "src/wasm/signature-hashing.h"

namespace v8::internal::wasm {

namespace value_type_impl {

// Check that {kGenericKindMask} is defined appropriately.
#define CHECK_MASK(kind, ...)                        \
  static_assert(                                     \
      static_cast<uint32_t>(GenericKind::k##kind) == \
      (static_cast<uint32_t>(GenericKind::k##kind) & kGenericKindMask));
FOREACH_GENERIC_TYPE(CHECK_MASK)
#undef CHECK_MASK
#define CHECK_MASK(kind, ...)                        \
  static_assert(                                     \
      static_cast<uint32_t>(NumericKind::k##kind) == \
      (static_cast<uint32_t>(NumericKind::k##kind) & kNumericKindMask));
FOREACH_NUMERIC_VALUE_TYPE(CHECK_MASK)
#undef CHECK_MASK

// Check correctness of enum conversions.
static_assert(0 == ToZeroBasedIndex(NumericKind::kI32));

}  // namespace value_type_impl

static_assert(kWasmBottom.is_bottom());
static_assert(kWasmTop.is_top());

ValueTypeCode ValueTypeBase::value_type_code_numeric() const {
  switch (numeric_kind()) {
#define NUMERIC_CASE(kind, ...) \
  case NumericKind::k##kind:    \
    return k##kind##Code;
    FOREACH_NUMERIC_VALUE_TYPE(NUMERIC_CASE)
#undef NUMERIC_CASE
  }
  UNREACHABLE();
}

ValueTypeCode ValueTypeBase::value_type_code_generic() const {
  switch (generic_kind()) {
#define ABSTRACT_TYPE_CASE(name, code, ...) \
  case GenericKind::k##name:                \
    return k##code##Code;
    FOREACH_ABSTRACT_TYPE(ABSTRACT_TYPE_CASE)
#undef ABSTRACT_TYPE_CASE
    // Among "internal" types, only "void" has a wire bytes encoding.
#define INTERNAL_TYPE_CASE(name, code, ...)                     \
  case GenericKind::k##name:                                    \
    if constexpr (GenericKind::k##name == GenericKind::kVoid) { \
      return k##code##Code;                                     \
    } else {                                                    \
      UNREACHABLE();                                            \
    }
    FOREACH_INTERNAL_TYPE(INTERNAL_TYPE_CASE)
#undef INTERNAL_TYPE_CASE
  }
  UNREACHABLE();
}

std::string ValueTypeBase::generic_heaptype_name() const {
  DCHECK(is_generic());
  std::ostringstream buf;
  if (is_shared()) buf << "shared ";
  switch (generic_kind()) {
#define GENERIC_CASE(kind, code, typekind, name) \
  case GenericKind::k##kind:                     \
    buf << name;                                 \
    return buf.str();
    FOREACH_GENERIC_TYPE(GENERIC_CASE)
#undef GENERIC_CASE
  }
  UNREACHABLE();
}

namespace {

void PrintGenericHeaptypeName(std::ostringstream& buf, ValueTypeBase type) {
  DCHECK(type.is_generic());
  if (type.is_nullable()) {
    GenericKind kind = type.generic_kind();
    if (kind == GenericKind::kNone) {
      // The code below would produce "noneref", and we need to keep it
      // that way for "(ref none)", so we need this special case.
      buf << (type.is_shared() ? "(shared nullref)" : "nullref");
      return;
    } else if (kind == GenericKind::kNoExn) {
      buf << (type.is_shared() ? "(shared nullexnref)" : "nullexnref");
      return;
    } else if (kind == GenericKind::kNoExtern) {
      buf << (type.is_shared() ? "(shared nullexternref)" : "nullexternref");
      return;
    } else if (kind == GenericKind::kNoFunc) {
      buf << (type.is_shared() ? "(shared nullfuncref)" : "nullfuncref");
      return;
    } else if (kind == GenericKind::kNoCont) {
      buf << (type.is_shared() ? "(shared nullcontref)" : "nullcontref");
      return;
    }
  }
  buf << type.generic_heaptype_name();
}

}  // namespace

std::string ValueTypeBase::name() const {
  if (is_numeric()) {
    switch (numeric_kind()) {
#define NUMERIC_CASE(kind, log2, code, mtype, shortName, name) \
  case NumericKind::k##kind:                                   \
    return name;
      FOREACH_NUMERIC_VALUE_TYPE(NUMERIC_CASE)
#undef NUMERIC_CASE
    }
    UNREACHABLE();
  }
  std::ostringstream buf;
  if (has_index()) {
    buf << "(ref ";
    if (is_nullable()) buf << "null ";
    if (is_exact()) buf << "exact ";
    buf << raw_index().index << ")";
  } else {
    DCHECK(is_generic());
    if (is_nullable()) {
      GenericKind kind = generic_kind();
      if (kind == GenericKind::kNone) {
        // The code below would produce "noneref", and we need to keep it
        // that way for "(ref none)", so we need this special case.
        return is_shared() ? "(shared nullref)" : "nullref";
      } else if (kind == GenericKind::kNoExn) {
        return is_shared() ? "(shared nullexnref)" : "nullexnref";
      } else if (kind == GenericKind::kNoExtern) {
        return is_shared() ? "(shared nullexternref)" : "nullexternref";
      } else if (kind == GenericKind::kNoFunc) {
        return is_shared() ? "(shared nullfuncref)" : "nullfuncref";
      } else if (kind == GenericKind::kNoCont) {
        return is_shared() ? "(shared nullcontref)" : "nullcontref";
      }
    }
    bool shorthand =
        (is_nullable() || is_sentinel() || is_string_view()) && !is_exact();
    bool append_ref = is_nullable() && !is_sentinel() && !is_exact();
    if (!shorthand) buf << "(ref ";
    if (!shorthand && is_nullable()) buf << "null ";
    PrintGenericHeaptypeName(buf, *this);
    if (append_ref) buf << "ref";
    if (!shorthand) buf << ")";
  }
  return buf.str();
}

std::optional<wasm::ValueKind> WasmReturnTypeFromSignature(
    const CanonicalSig* wasm_signature) {
  if (wasm_signature->return_count() == 0) return {};

  DCHECK_EQ(wasm_signature->return_count(), 1);
  CanonicalValueType return_type = wasm_signature->GetReturn(0);
  return {return_type.kind()};
}

bool EquivalentNumericSig(const CanonicalSig* a, const FunctionSig* b) {
  if (a->parameter_count() != b->parameter_count()) return false;
  if (a->return_count() != b->return_count()) return false;
  base::Vector<const CanonicalValueType> a_types = a->all();
  base::Vector<const ValueType> b_types = b->all();
  for (size_t i = 0; i < a_types.size(); i++) {
    if (!a_types[i].is_numeric()) return false;
    if (a_types[i].kind() != b_types[i].kind()) return false;
  }
  return true;
}

#if DEBUG
V8_EXPORT_PRIVATE extern void PrintFunctionSig(const wasm::FunctionSig* sig) {
  std::ostringstream os;
  os << sig->parameter_count() << " parameters:\n";
  for (size_t i = 0; i < sig->parameter_count(); i++) {
    os << "  " << i << ": " << sig->GetParam(i) << "\n";
  }
  os << sig->return_count() << " returns:\n";
  for (size_t i = 0; i < sig->return_count(); i++) {
    os << "  " << i << ": " << sig->GetReturn() << "\n";
  }
  PrintF("%s", os.str().c_str());
}
#endif

namespace {
const wasm::FunctionSig* ReplaceTypeInSig(Zone* zone,
                                          const wasm::FunctionSig* sig,
                                          wasm::ValueType from,
                                          wasm::ValueType to,
                                          size_t num_replacements) {
  size_t param_occurences =
      std::count(sig->parameters().begin(), sig->parameters().end(), from);
  size_t return_occurences =
      std::count(sig->returns().begin(), sig->returns().end(), from);
  if (param_occurences == 0 && return_occurences == 0) return sig;

  wasm::FunctionSig::Builder builder(
      zone, sig->return_count() + return_occurences * (num_replacements - 1),
      sig->parameter_count() + param_occurences * (num_replacements - 1));

  for (wasm::ValueType ret : sig->returns()) {
    if (ret == from) {
      for (size_t i = 0; i < num_replacements; i++) builder.AddReturn(to);
    } else {
      builder.AddReturn(ret);
    }
  }

  for (wasm::ValueType param : sig->parameters()) {
    if (param == from) {
      for (size_t i = 0; i < num_replacements; i++) builder.AddParam(to);
    } else {
      builder.AddParam(param);
    }
  }

  return builder.Get();
}
}  // namespace

const wasm::FunctionSig* GetI32Sig(Zone* zone, const wasm::FunctionSig* sig) {
  return ReplaceTypeInSig(zone, sig, wasm::kWasmI64, wasm::kWasmI32, 2);
}

CanonicalSig* CanonicalSig::Builder::Get() const {
  CanonicalSig* sig =
      reinterpret_cast<
          const SignatureBuilder<CanonicalSig, CanonicalValueType>*>(this)
          ->Get();
  sig->signature_hash_ = wasm::SignatureHasher::Hash(sig);
  return sig;
}

}  // namespace v8::internal::wasm
