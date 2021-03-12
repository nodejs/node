// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CONSTANTS_H_
#define V8_WASM_WASM_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

#include "src/common/globals.h"

namespace v8 {
namespace internal {
namespace wasm {

// Binary encoding of the module header.
constexpr uint32_t kWasmMagic = 0x6d736100;
constexpr uint32_t kWasmVersion = 0x01;

// Binary encoding of value and heap types.
enum ValueTypeCode : uint8_t {
  // Current wasm types
  kVoidCode = 0x40,
  kI32Code = 0x7f,
  kI64Code = 0x7e,
  kF32Code = 0x7d,
  kF64Code = 0x7c,
  // Simd proposal
  kS128Code = 0x7b,
  // reftypes, typed-funcref, and GC proposals
  kI8Code = 0x7a,
  kI16Code = 0x79,
  kFuncRefCode = 0x70,
  kExternRefCode = 0x6f,
  kAnyRefCode = 0x6e,
  kEqRefCode = 0x6d,
  kOptRefCode = 0x6c,
  kRefCode = 0x6b,
  kI31RefCode = 0x6a,
  kRttWithDepthCode = 0x69,
  kRttCode = 0x68,
  kDataRefCode = 0x67,
};
// Binary encoding of other types.
constexpr uint8_t kWasmFunctionTypeCode = 0x60;
constexpr uint8_t kWasmStructTypeCode = 0x5f;
constexpr uint8_t kWasmArrayTypeCode = 0x5e;

// Binary encoding of import/export kinds.
enum ImportExportKindCode : uint8_t {
  kExternalFunction = 0,
  kExternalTable = 1,
  kExternalMemory = 2,
  kExternalGlobal = 3,
  kExternalException = 4
};

enum LimitsFlags : uint8_t {
  kNoMaximum = 0x00,           // Also valid for table limits.
  kWithMaximum = 0x01,         // Also valid for table limits.
  kSharedNoMaximum = 0x02,     // Only valid for memory limits.
  kSharedWithMaximum = 0x03,   // Only valid for memory limits.
  kMemory64NoMaximum = 0x04,   // Only valid for memory limits.
  kMemory64WithMaximum = 0x05  // Only valid for memory limits.
};

// Flags for data and element segments.
enum SegmentFlags : uint8_t {
  kActiveNoIndex = 0,    // Active segment with a memory/table index of zero.
  kPassive = 1,          // Passive segment.
  kActiveWithIndex = 2,  // Active segment with a given memory/table index.
};

// Binary encoding of sections identifiers.
enum SectionCode : int8_t {
  kUnknownSectionCode = 0,     // code for unknown sections
  kTypeSectionCode = 1,        // Function signature declarations
  kImportSectionCode = 2,      // Import declarations
  kFunctionSectionCode = 3,    // Function declarations
  kTableSectionCode = 4,       // Indirect function table and others
  kMemorySectionCode = 5,      // Memory attributes
  kGlobalSectionCode = 6,      // Global declarations
  kExportSectionCode = 7,      // Exports
  kStartSectionCode = 8,       // Start function declaration
  kElementSectionCode = 9,     // Elements section
  kCodeSectionCode = 10,       // Function code
  kDataSectionCode = 11,       // Data segments
  kDataCountSectionCode = 12,  // Number of data segments
  kExceptionSectionCode = 13,  // Exception section

  // The following sections are custom sections, and are identified using a
  // string rather than an integer. Their enumeration values are not guaranteed
  // to be consistent.
  kNameSectionCode,               // Name section (encoded as a string)
  kSourceMappingURLSectionCode,   // Source Map URL section
  kDebugInfoSectionCode,          // DWARF section .debug_info
  kExternalDebugInfoSectionCode,  // Section encoding the external symbol path
  kCompilationHintsSectionCode,   // Compilation hints section

  // Helper values
  kFirstSectionInModule = kTypeSectionCode,
  kLastKnownModuleSection = kCompilationHintsSectionCode,
  kFirstUnorderedSection = kDataCountSectionCode,
};

// Binary encoding of compilation hints.
constexpr uint8_t kDefaultCompilationHint = 0x0;
constexpr uint8_t kNoCompilationHint = kMaxUInt8;

// Binary encoding of name section kinds.
enum NameSectionKindCode : uint8_t { kModule = 0, kFunction = 1, kLocal = 2 };

constexpr size_t kWasmPageSize = 0x10000;
constexpr uint32_t kWasmPageSizeLog2 = 16;
static_assert(kWasmPageSize == size_t{1} << kWasmPageSizeLog2, "consistency");

// TODO(wasm): Wrap WasmCodePosition in a struct.
using WasmCodePosition = int;
constexpr WasmCodePosition kNoCodePosition = -1;

constexpr uint32_t kExceptionAttribute = 0;

constexpr int kAnonymousFuncIndex = -1;

// The number of calls to an exported Wasm function that will be handled
// by the generic wrapper. Once the budget is exhausted, a specific wrapper
// is to be compiled for the function's signature.
// The abstract goal of the tiering strategy for the js-to-wasm wrappers is to
// use the generic wrapper as much as possible (less space, no need to compile),
// but fall back to compiling a specific wrapper for any function (signature)
// that is used often enough for the generic wrapper's small execution penalty
// to start adding up.
// So, when choosing a value for the initial budget, we are interested in a
// value that skips on tiering up functions that are called only a few times and
// the tier-up only wastes resources, but triggers compilation of specific
// wrappers early on for those functions that have the potential to be called
// often enough.
constexpr uint32_t kGenericWrapperBudget = 1000;

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_CONSTANTS_H_
