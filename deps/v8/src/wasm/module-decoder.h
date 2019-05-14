// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_DECODER_H_
#define V8_WASM_MODULE_DECODER_H_

#include "src/globals.h"
#include "src/wasm/function-body-decoder.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-features.h"
#include "src/wasm/wasm-module.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {

class Counters;

namespace wasm {

struct CompilationEnv;

inline bool IsValidSectionCode(uint8_t byte) {
  return kTypeSectionCode <= byte && byte <= kLastKnownModuleSection;
}

const char* SectionName(SectionCode code);

typedef Result<std::shared_ptr<WasmModule>> ModuleResult;
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
V8_EXPORT_PRIVATE ModuleResult DecodeWasmModule(
    const WasmFeatures& enabled, const byte* module_start,
    const byte* module_end, bool verify_functions, ModuleOrigin origin,
    Counters* counters, AccountingAllocator* allocator);

// Exposed for testing. Decodes a single function signature, allocating it
// in the given zone. Returns {nullptr} upon failure.
V8_EXPORT_PRIVATE FunctionSig* DecodeWasmSignatureForTesting(
    const WasmFeatures& enabled, Zone* zone, const byte* start,
    const byte* end);

// Decodes the bytes of a wasm function between
// {function_start} and {function_end}.
V8_EXPORT_PRIVATE FunctionResult DecodeWasmFunctionForTesting(
    const WasmFeatures& enabled, Zone* zone, const ModuleWireBytes& wire_bytes,
    const WasmModule* module, const byte* function_start,
    const byte* function_end, Counters* counters);

V8_EXPORT_PRIVATE WasmInitExpr DecodeWasmInitExprForTesting(
    const WasmFeatures& enabled, const byte* start, const byte* end);

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

// Decode the function names from the name section.
// Returns the result as an unordered map. Only names with valid utf8 encoding
// are stored and conflicts are resolved by choosing the last name read.
void DecodeFunctionNames(const byte* module_start, const byte* module_end,
                         std::unordered_map<uint32_t, WireBytesRef>* names);

// Decode the local names assignment from the name section.
// Stores the result in the given {LocalNames} structure. The result will be
// empty if no name section is present. On encountering an error in the name
// section, returns all information decoded up to the first error.
void DecodeLocalNames(const byte* module_start, const byte* module_end,
                      LocalNames* result);

class ModuleDecoderImpl;

class ModuleDecoder {
 public:
  explicit ModuleDecoder(const WasmFeatures& enabled);
  ~ModuleDecoder();

  void StartDecoding(Counters* counters, AccountingAllocator* allocator,
                     ModuleOrigin origin = ModuleOrigin::kWasmOrigin);

  void DecodeModuleHeader(Vector<const uint8_t> bytes, uint32_t offset);

  void DecodeSection(SectionCode section_code, Vector<const uint8_t> bytes,
                     uint32_t offset, bool verify_functions = true);

  bool CheckFunctionsCount(uint32_t functions_count, uint32_t offset);

  void DecodeFunctionBody(uint32_t index, uint32_t size, uint32_t offset,
                          bool verify_functions = true);

  ModuleResult FinishDecoding(bool verify_functions = true);

  const std::shared_ptr<WasmModule>& shared_module() const;
  WasmModule* module() const { return shared_module().get(); }

  bool ok();

  // Translates the unknown section that decoder is pointing to to an extended
  // SectionCode if the unknown section is known to decoder. Currently this only
  // handles the name section.
  // The decoder is expected to point after the section lenght and just before
  // the identifier string of the unknown section.
  // If a SectionCode other than kUnknownSectionCode is returned, the decoder
  // will point right after the identifier string. Otherwise, the position is
  // undefined.
  static SectionCode IdentifyUnknownSection(Decoder& decoder, const byte* end);

 private:
  const WasmFeatures enabled_features_;
  std::unique_ptr<ModuleDecoderImpl> impl_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_DECODER_H_
