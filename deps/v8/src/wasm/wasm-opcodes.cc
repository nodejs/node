// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/wasm/wasm-opcodes.h"

#include <array>

#include "src/codegen/signature.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-module.h"
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

// TODO(7748): Once we have a story for JS interaction of structs/arrays, this
// function should become independent of module. Remove 'module' parameter in
// this function as well as all transitive callees that no longer need it
// (In essence, revert
// https://chromium-review.googlesource.com/c/v8/v8/+/2413251).
bool IsJSCompatibleSignature(const FunctionSig* sig, const WasmModule* module,
                             const WasmFeatures& enabled_features) {
  for (auto type : sig->all()) {
    // Structs and arrays may only be passed via externref.
    // Rtts are implicit and can not be used explicitly.
    if (type == kWasmS128 || type.is_rtt() ||
        (type.has_index() && !module->has_signature(type.ref_index()))) {
      return false;
    }
    if (type.is_object_reference()) {
      switch (type.heap_type().representation()) {
        case HeapType::kStringViewWtf8:
        case HeapType::kStringViewWtf16:
        case HeapType::kStringViewIter:
        case HeapType::kNone:
        case HeapType::kNoFunc:
        case HeapType::kNoExtern:
        case HeapType::kAny:
        case HeapType::kI31:
          return false;
        default:
          break;
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
