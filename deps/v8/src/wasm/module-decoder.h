// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_MODULE_DECODER_H_
#define V8_WASM_MODULE_DECODER_H_

#include <memory>

#include "src/common/globals.h"
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

using ModuleResult = Result<std::shared_ptr<WasmModule>>;
using FunctionResult = Result<std::unique_ptr<WasmFunction>>;
using FunctionOffsets = std::vector<std::pair<int, int>>;
using FunctionOffsetsResult = Result<FunctionOffsets>;

struct AsmJsOffsetEntry {
  int byte_offset;
  int source_position_call;
  int source_position_number_conversion;
};
struct AsmJsOffsetFunctionEntries {
  int start_offset;
  int end_offset;
  std::vector<AsmJsOffsetEntry> entries;
};
struct AsmJsOffsets {
  std::vector<AsmJsOffsetFunctionEntries> functions;
};
using AsmJsOffsetsResult = Result<AsmJsOffsets>;

class LocalName {
 public:
  LocalName(int index, WireBytesRef name) : index_(index), name_(name) {}

  int index() const { return index_; }
  WireBytesRef name() const { return name_; }

  struct IndexLess {
    bool operator()(const LocalName& a, const LocalName& b) const {
      return a.index() < b.index();
    }
  };

 private:
  int index_;
  WireBytesRef name_;
};

class LocalNamesPerFunction {
 public:
  // For performance reasons, {LocalNamesPerFunction} should not be copied.
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(LocalNamesPerFunction);

  LocalNamesPerFunction(int function_index, std::vector<LocalName> names)
      : function_index_(function_index), names_(std::move(names)) {
    DCHECK(
        std::is_sorted(names_.begin(), names_.end(), LocalName::IndexLess{}));
  }

  int function_index() const { return function_index_; }

  WireBytesRef GetName(int local_index) {
    auto it =
        std::lower_bound(names_.begin(), names_.end(),
                         LocalName{local_index, {}}, LocalName::IndexLess{});
    if (it == names_.end()) return {};
    if (it->index() != local_index) return {};
    return it->name();
  }

  struct FunctionIndexLess {
    bool operator()(const LocalNamesPerFunction& a,
                    const LocalNamesPerFunction& b) const {
      return a.function_index() < b.function_index();
    }
  };

 private:
  int function_index_;
  std::vector<LocalName> names_;
};

class LocalNames {
 public:
  // For performance reasons, {LocalNames} should not be copied.
  MOVE_ONLY_NO_DEFAULT_CONSTRUCTOR(LocalNames);

  explicit LocalNames(std::vector<LocalNamesPerFunction> functions)
      : functions_(std::move(functions)) {
    DCHECK(std::is_sorted(functions_.begin(), functions_.end(),
                          LocalNamesPerFunction::FunctionIndexLess{}));
  }

  WireBytesRef GetName(int function_index, int local_index) {
    auto it = std::lower_bound(functions_.begin(), functions_.end(),
                               LocalNamesPerFunction{function_index, {}},
                               LocalNamesPerFunction::FunctionIndexLess{});
    if (it == functions_.end()) return {};
    if (it->function_index() != function_index) return {};
    return it->GetName(local_index);
  }

 private:
  std::vector<LocalNamesPerFunction> functions_;
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
AsmJsOffsetsResult DecodeAsmJsOffsets(Vector<const uint8_t> encoded_offsets);

// Decode the function names from the name section.
// Returns the result as an unordered map. Only names with valid utf8 encoding
// are stored and conflicts are resolved by choosing the last name read.
void DecodeFunctionNames(const byte* module_start, const byte* module_end,
                         std::unordered_map<uint32_t, WireBytesRef>* names);

// Decode the local names assignment from the name section.
// The result will be empty if no name section is present. On encountering an
// error in the name section, returns all information decoded up to the first
// error.
LocalNames DecodeLocalNames(Vector<const uint8_t> module_bytes);

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
  // SectionCode if the unknown section is known to decoder.
  // The decoder is expected to point after the section length and just before
  // the identifier string of the unknown section.
  // If a SectionCode other than kUnknownSectionCode is returned, the decoder
  // will point right after the identifier string. Otherwise, the position is
  // undefined.
  static SectionCode IdentifyUnknownSection(Decoder* decoder, const byte* end);

 private:
  const WasmFeatures enabled_features_;
  std::unique_ptr<ModuleDecoderImpl> impl_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_DECODER_H_
