// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CONSTANTS_H_
#define V8_WASM_WASM_CONSTANTS_H_

#include <cstddef>
#include <cstdint>

namespace v8 {
namespace internal {
namespace wasm {

// Binary encoding of the module header.
constexpr uint32_t kWasmMagic = 0x6d736100;
constexpr uint32_t kWasmVersion = 0x01;

// Binary encoding of local types.
enum ValueTypeCode : uint8_t {
  kLocalVoid = 0x40,
  kLocalI32 = 0x7f,
  kLocalI64 = 0x7e,
  kLocalF32 = 0x7d,
  kLocalF64 = 0x7c,
  kLocalS128 = 0x7b,
  kLocalAnyFunc = 0x70,
  kLocalAnyRef = 0x6f,
  kLocalExceptRef = 0x68,
};
// Binary encoding of other types.
constexpr uint8_t kWasmFunctionTypeCode = 0x60;

// Binary encoding of import/export kinds.
enum ImportExportKindCode : uint8_t {
  kExternalFunction = 0,
  kExternalTable = 1,
  kExternalMemory = 2,
  kExternalGlobal = 3,
  kExternalException = 4
};

// Binary encoding of maximum and shared flags for memories.
enum MaximumFlag : uint8_t { kNoMaximumFlag = 0, kHasMaximumFlag = 1 };

enum MemoryFlags : uint8_t {
  kNoMaximum = 0,
  kMaximum = 1,
  kSharedNoMaximum = 2,
  kSharedAndMaximum = 3
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
  kNameSectionCode,              // Name section (encoded as a string)
  kSourceMappingURLSectionCode,  // Source Map URL section
  kCompilationHintsSectionCode,  // Compilation hints section

  // Helper values
  kFirstSectionInModule = kTypeSectionCode,
  kLastKnownModuleSection = kCompilationHintsSectionCode,
  kFirstUnorderedSection = kDataCountSectionCode,
};

// Binary encoding of name section kinds.
enum NameSectionKindCode : uint8_t { kModule = 0, kFunction = 1, kLocal = 2 };

constexpr size_t kWasmPageSize = 0x10000;
constexpr uint32_t kWasmPageSizeLog2 = 16;
static_assert(kWasmPageSize == size_t{1} << kWasmPageSizeLog2, "consistency");

// TODO(wasm): Wrap WasmCodePosition in a struct.
using WasmCodePosition = int;
constexpr WasmCodePosition kNoCodePosition = -1;

constexpr uint32_t kExceptionAttribute = 0;

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_CONSTANTS_H_
