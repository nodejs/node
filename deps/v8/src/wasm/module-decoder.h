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
namespace wasm {

typedef Result<const WasmModule*> ModuleResult;
typedef Result<WasmFunction*> FunctionResult;
typedef std::vector<std::pair<int, int>> FunctionOffsets;
typedef Result<FunctionOffsets> FunctionOffsetsResult;
struct AsmJsOffsetEntry {
  int byte_offset;
  int source_position_call;
  int source_position_number_conversion;
};
typedef std::vector<std::vector<AsmJsOffsetEntry>> AsmJsOffsets;
typedef Result<AsmJsOffsets> AsmJsOffsetsResult;

// Decodes the bytes of a WASM module between {module_start} and {module_end}.
V8_EXPORT_PRIVATE ModuleResult DecodeWasmModule(Isolate* isolate,
                                                const byte* module_start,
                                                const byte* module_end,
                                                bool verify_functions,
                                                ModuleOrigin origin);

// Exposed for testing. Decodes a single function signature, allocating it
// in the given zone. Returns {nullptr} upon failure.
V8_EXPORT_PRIVATE FunctionSig* DecodeWasmSignatureForTesting(Zone* zone,
                                                             const byte* start,
                                                             const byte* end);

// Decodes the bytes of a WASM function between
// {function_start} and {function_end}.
V8_EXPORT_PRIVATE FunctionResult DecodeWasmFunction(Isolate* isolate,
                                                    Zone* zone,
                                                    ModuleBytesEnv* env,
                                                    const byte* function_start,
                                                    const byte* function_end);

// Extracts the function offset table from the wasm module bytes.
// Returns a vector with <offset, length> entries, or failure if the wasm bytes
// are detected as invalid. Note that this validation is not complete.
FunctionOffsetsResult DecodeWasmFunctionOffsets(const byte* module_start,
                                                const byte* module_end);

V8_EXPORT_PRIVATE WasmInitExpr DecodeWasmInitExprForTesting(const byte* start,
                                                            const byte* end);

struct CustomSectionOffset {
  uint32_t section_start;
  uint32_t name_offset;
  uint32_t name_length;
  uint32_t payload_offset;
  uint32_t payload_length;
  uint32_t section_length;
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

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_MODULE_DECODER_H_
