// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXPERIMENTAL_REDLINE_GENERATORS_WASM_MODULE_H_
#define V8_EXPERIMENTAL_REDLINE_GENERATORS_WASM_MODULE_H_

#include <cstdint>
#include <string>
#include <vector>

#include "experimental/redline/core/random.h"

namespace redline {

// Value type kinds of generated Wasm modules with their binary encodings.
enum class WasmType : uint8_t {
  kI32 = 0x7F,
  kI64 = 0x7E,
  kF32 = 0x7D,
  kF64 = 0x7C,
  kV128 = 0x7B,
  kFuncRef = 0x70,
  kExternRef = 0x6F,
  kAnyRef = 0x6E,
  kEqRef = 0x6D,
  kI31Ref = 0x6C,
  kStructRef = 0x6B,
  kArrayRef = 0x6A,
  kExnRef = 0x69,
  kRefNull = 0x63,
  kRef = 0x64,
};

// Concrete or abstract WebAssembly value type.
struct WasmValType {
  WasmType kind = WasmType::kI32;
  // Concrete type index when `kind` is `kRef` or `kRefNull` and
  // `heap_type` is 0.
  uint32_t type_idx = 0;
  // Abstract heap type byte when `kind` is `kRef` for `(ref ht)`.
  uint8_t heap_type = 0;

  constexpr WasmValType() = default;
  constexpr WasmValType(WasmType k) : kind(k) {}  // NOLINT(runtime/explicit)
  constexpr WasmValType(WasmType k, uint32_t idx, uint8_t ht = 0)
      : kind(k), type_idx(idx), heap_type(ht) {}

  static constexpr WasmValType RefNull(uint32_t idx) {
    return WasmValType(WasmType::kRefNull, idx, 0);
  }
  static constexpr WasmValType Ref(uint32_t idx) {
    return WasmValType(WasmType::kRef, idx, 0);
  }
  static constexpr WasmValType RefAbstract(WasmType ht) {
    return WasmValType(WasmType::kRef, 0, static_cast<uint8_t>(ht));
  }

  bool operator==(const WasmValType& other) const {
    return kind == other.kind && type_idx == other.type_idx &&
           heap_type == other.heap_type;
  }
  bool operator!=(const WasmValType& other) const { return !(*this == other); }

  bool IsScalarNumeric() const {
    return kind == WasmType::kI32 || kind == WasmType::kI64 ||
           kind == WasmType::kF32 || kind == WasmType::kF64;
  }
  bool IsNumericOrSimd() const {
    return IsScalarNumeric() || kind == WasmType::kV128;
  }
  bool IsRef() const { return !IsNumericOrSimd(); }
  bool IsNullable() const { return IsRef() && kind != WasmType::kRef; }
  bool IsDefaultable() const { return kind != WasmType::kRef; }
  bool IsConcreteRef() const {
    return kind == WasmType::kRefNull ||
           (kind == WasmType::kRef && heap_type == 0);
  }
  bool IsJsCompatible() const {
    return kind != WasmType::kV128 && kind != WasmType::kExnRef;
  }
};

struct WasmSignature {
  std::vector<WasmValType> params;
  std::vector<WasmValType> results;  // Empty or one value.
};

struct WasmMemoryInfo {
  bool is_memory64 = false;
  bool is_shared = false;
  bool imported = false;
  uint32_t min_pages = 1;
  uint32_t max_pages = 5;
  std::string export_name;
};

struct WasmGlobalExport {
  std::string name;
  WasmValType type;
};

// Random WebAssembly module that is valid by construction, plus metadata that
// the JavaScript caller needs.
struct WasmModule {
  std::vector<uint8_t> bytes;
  // Imported JS functions `m.f0`, `m.f1`, ... with their signatures.
  std::vector<WasmSignature> imports;
  // Exported functions `e0`, `e1`, ... with their signatures.
  std::vector<WasmSignature> functions;
  // Exported mutable JS-compatible globals `g0`, `g1`, ... with their types.
  std::vector<WasmGlobalExport> globals;
  // Declared and imported memories exported as `mem`, `mem1`, ...
  std::vector<WasmMemoryInfo> memories;
  bool memory = false;  // True when `memories` is not empty.
  bool table = false;   // Exported as `tab`.
  bool table_is_64 = false;
  // Number of `funcref` slots in `tab`.
  uint32_t table_size = 0;
  // Optional GC reference table exported as `tab1`.
  bool has_gc_table = false;
  bool gc_table_is_64 = false;
  uint32_t gc_table_size = 0;
  // Number of callees in `tab` that share the `call_indirect` signature.
  uint32_t table_callees = 0;
  // Index in `functions` of an export that executes `call_indirect` with
  // parameter 0 as table slot, or -1 if none exists.
  int indirect_caller = -1;
  // Stage 2 feature flags present in the generated module.
  bool has_gc = false;
  bool has_simd = false;
  bool has_eh = false;
  bool has_js_string_builtins = false;
  bool has_imported_jstag = false;
  // Imported JS tags `m.t0`, `m.t1`, ... with their parameter signatures.
  std::vector<WasmSignature> imported_tags;
  // Exported module tags `tag0`, ... with their parameter signatures.
  std::vector<WasmSignature> exported_tags;
  // Index in `imports` wrapped with `WebAssembly.Suspending`, or -1.
  int jspi_import = -1;
  // Index in `functions` wrapped with `WebAssembly.promising`, or -1.
  int jspi_export = -1;
  // GC concrete type indices defined in the module.
  std::vector<uint32_t> struct_types;
  std::vector<uint32_t> array_types;
};

// Builds a spec-valid WebAssembly module with MVP features, memory64,
// multi-memory, shared memory atomics, bulk memory, WasmGC types and
// instructions, SIMD, `try_table` exception handling, and JSPI metadata.
WasmModule RandomWasmModule(Random& r);

}  // namespace redline

#endif  // V8_EXPERIMENTAL_REDLINE_GENERATORS_WASM_MODULE_H_
