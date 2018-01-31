// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_DECODER_H_
#define V8_WASM_MODULE_DECODER_H_

#include "src/globals.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

namespace compiler {
struct ModuleEnv;
}

namespace wasm {

const uint8_t kWasmFunctionTypeForm = 0x60;
const uint8_t kWasmAnyFunctionTypeForm = 0x70;
const uint8_t kHasMaximumFlag = 1;
const uint8_t kNoMaximumFlag = 0;

enum MemoryFlags : uint8_t {
  kNoMaximum = 0,
  kMaximum = 1,
  kSharedNoMaximum = 2,
  kSharedAndMaximum = 3
};

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

enum NameSectionType : uint8_t { kModule = 0, kFunction = 1, kLocal = 2 };

inline bool IsValidSectionCode(uint8_t byte) {
  return kTypeSectionCode <= byte && byte <= kLastKnownModuleSection;
}

const char* SectionName(SectionCode code);

typedef Result<std::unique_ptr<WasmModule>> ModuleResult;
typedef Result<std::unique_ptr<WasmFunction>> FunctionResult;
typedef std::vector<std::pair<int, int>> FunctionOffsets;
typedef Result<FunctionOffsets> FunctionOffsetsResult;

struct AsmJsOffsetEntry {
  int byte_offset;
  int source_position_call;
  int source_position_number_conversion;
};
typedef std::vector<std::vector<AsmJsOffsetEntry>> AsmJsOffsets;
typedef Result<AsmJsOffsets> AsmJsOffsetsResult;

struct LocalName {
  int local_index;
  WireBytesRef name;
  LocalName(int local_index, WireBytesRef name)
      : local_index(local_index), name(name) {}
};
struct LocalNamesPerFunction {
  int function_index;
  int max_local_index = -1;
  std::vector<LocalName> names;
  explicit LocalNamesPerFunction(int function_index)
      : function_index(function_index) {}
};
struct LocalNames {
  int max_function_index = -1;
  std::vector<LocalNamesPerFunction> names;
};

// Decodes the bytes of a wasm module between {module_start} and {module_end}.
V8_EXPORT_PRIVATE ModuleResult SyncDecodeWasmModule(Isolate* isolate,
                                                    const byte* module_start,
                                                    const byte* module_end,
                                                    bool verify_functions,
                                                    ModuleOrigin origin);

V8_EXPORT_PRIVATE ModuleResult AsyncDecodeWasmModule(
    Isolate* isolate, const byte* module_start, const byte* module_end,
    bool verify_functions, ModuleOrigin origin,
    const std::shared_ptr<Counters> async_counters);

// Exposed for testing. Decodes a single function signature, allocating it
// in the given zone. Returns {nullptr} upon failure.
V8_EXPORT_PRIVATE FunctionSig* DecodeWasmSignatureForTesting(Zone* zone,
                                                             const byte* start,
                                                             const byte* end);

// Decodes the bytes of a wasm function between
// {function_start} and {function_end}.
V8_EXPORT_PRIVATE FunctionResult SyncDecodeWasmFunction(
    Isolate* isolate, Zone* zone, const ModuleWireBytes& wire_bytes,
    const WasmModule* module, const byte* function_start,
    const byte* function_end);

V8_EXPORT_PRIVATE FunctionResult
AsyncDecodeWasmFunction(Isolate* isolate, Zone* zone, compiler::ModuleEnv* env,
                        const byte* function_start, const byte* function_end,
                        const std::shared_ptr<Counters> async_counters);

V8_EXPORT_PRIVATE WasmInitExpr DecodeWasmInitExprForTesting(const byte* start,
                                                            const byte* end);

struct CustomSectionOffset {
  WireBytesRef section;
  WireBytesRef name;
  WireBytesRef payload;
};

V8_EXPORT_PRIVATE std::vector<CustomSectionOffset> DecodeCustomSections(
    const byte* start, const byte* end);

// Extracts the mapping from wasm byte offset to asm.js source position per
// function.
// Returns a vector of vectors with <byte_offset, source_position> entries, or
// failure if the wasm bytes are detected as invalid. Note that this validation
// is not complete.
AsmJsOffsetsResult DecodeAsmJsOffsets(const byte* module_start,
                                      const byte* module_end);

// Decode the local names assignment from the name section.
// Stores the result in the given {LocalNames} structure. The result will be
// empty if no name section is present. On encountering an error in the name
// section, returns all information decoded up to the first error.
void DecodeLocalNames(const byte* module_start, const byte* module_end,
                      LocalNames* result);

class ModuleDecoderImpl;

class ModuleDecoder {
 public:
  ModuleDecoder();
  ~ModuleDecoder();

  void StartDecoding(Isolate* isolate,
                     ModuleOrigin origin = ModuleOrigin::kWasmOrigin);

  void DecodeModuleHeader(Vector<const uint8_t> bytes, uint32_t offset);

  void DecodeSection(SectionCode section_code, Vector<const uint8_t> bytes,
                     uint32_t offset, bool verify_functions = true);

  bool CheckFunctionsCount(uint32_t functions_count, uint32_t offset);

  void DecodeFunctionBody(uint32_t index, uint32_t size, uint32_t offset,
                          bool verify_functions = true);

  ModuleResult FinishDecoding(bool verify_functions = true);

  WasmModule* module() const;

  bool ok();

 private:
  std::unique_ptr<ModuleDecoderImpl> impl_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_DECODER_H_
