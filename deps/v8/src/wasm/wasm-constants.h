// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_WASM_CONSTANTS_H_
#define V8_WASM_WASM_CONSTANTS_H_

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
  kLocalAnyRef = 0x6f
};
// Binary encoding of other types.
constexpr uint8_t kWasmFunctionTypeCode = 0x60;
constexpr uint8_t kWasmAnyFunctionTypeCode = 0x70;

// Binary encoding of import/export kinds.
enum ImportExportKindCode : uint8_t {
  kExternalFunction = 0,
  kExternalTable = 1,
  kExternalMemory = 2,
  kExternalGlobal = 3
};

// Binary encoding of maximum and shared flags for memories.
enum MaximumFlag : uint8_t { kNoMaximumFlag = 0, kHasMaximumFlag = 1 };

enum MemoryFlags : uint8_t {
  kNoMaximum = 0,
  kMaximum = 1,
  kSharedNoMaximum = 2,
  kSharedAndMaximum = 3
};

// Binary encoding of sections identifiers.
enum SectionCode : int8_t {
  kUnknownSectionCode = 0,     // code for unknown sections
  kTypeSectionCode = 1,        // Function signature declarations
  kImportSectionCode = 2,      // Import declarations
  kFunctionSectionCode = 3,    // Function declarations
  kTableSectionCode = 4,       // Indirect function table and other tables
  kMemorySectionCode = 5,      // Memory attributes
  kGlobalSectionCode = 6,      // Global declarations
  kExportSectionCode = 7,      // Exports
  kStartSectionCode = 8,       // Start function declaration
  kElementSectionCode = 9,     // Elements section
  kCodeSectionCode = 10,       // Function code
  kDataSectionCode = 11,       // Data segments
  kNameSectionCode = 12,       // Name section (encoded as a string)
  kExceptionSectionCode = 13,  // Exception section

  // Helper values
  kFirstSectionInModule = kTypeSectionCode,
  kLastKnownModuleSection = kExceptionSectionCode,
};

// Binary encoding of name section kinds.
enum NameSectionKindCode : uint8_t { kModule = 0, kFunction = 1, kLocal = 2 };

constexpr uint32_t kWasmPageSize = 0x10000;
constexpr int kInvalidExceptionTag = -1;

// TODO(wasm): Wrap WasmCodePosition in a struct.
using WasmCodePosition = int;
constexpr WasmCodePosition kNoCodePosition = -1;

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_WASM_CONSTANTS_H_
