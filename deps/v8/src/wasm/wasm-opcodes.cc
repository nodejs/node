// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-opcodes.h"

#include <array>

#include "src/codegen/signature.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-opcodes-inl.h"

namespace v8 {
namespace internal {
namespace wasm {

std::ostream& operator<<(std::ostream& os, const FunctionSig& sig) {
  if (sig.return_count() == 0) os << "v";
  for (auto ret : sig.returns()) {
    os << ret.short_name();
  }
  os << "_";
  if (sig.parameter_count() == 0) os << "v";
  for (auto param : sig.parameters()) {
    os << param.short_name();
  }
  return os;
}

bool IsJSCompatibleSignature(const FunctionSig* sig,
                             const WasmFeatures& enabled_features) {
  if (!enabled_features.has_mv() && sig->return_count() > 1) {
    return false;
  }
  for (auto type : sig->all()) {
    if (!enabled_features.has_bigint() && type == kWasmI64) {
      return false;
    }

    if (type == kWasmS128) return false;

    if (type.is_object_reference_type()) {
      uint32_t representation = type.heap_representation();
      // TODO(7748): Once there's a story for JS interop for struct/array types,
      // allow them here.
      if (!(representation == HeapType::kExtern ||
            representation == HeapType::kExn ||
            representation == HeapType::kFunc ||
            representation == HeapType::kEq)) {
        return false;
      }
    }
  }
  return true;
}

// Define constexpr arrays.
constexpr uint8_t LoadType::kLoadSizeLog2[];
constexpr ValueType LoadType::kValueType[];
constexpr MachineType LoadType::kMemType[];
constexpr uint8_t StoreType::kStoreSizeLog2[];
constexpr ValueType StoreType::kValueType[];
constexpr MachineRepresentation StoreType::kMemRep[];

}  // namespace wasm
}  // namespace internal
}  // namespace v8
