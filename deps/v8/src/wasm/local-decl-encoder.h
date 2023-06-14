// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_LOCAL_DECL_ENCODER_H_
#define V8_WASM_LOCAL_DECL_ENCODER_H_

#include "src/common/globals.h"
#include "src/wasm/wasm-opcodes.h"
#include "src/zone/zone-containers.h"
#include "src/zone/zone.h"

namespace v8 {
namespace internal {
namespace wasm {

// A helper for encoding local declarations prepended to the body of a function.
class V8_EXPORT_PRIVATE LocalDeclEncoder {
 public:
  explicit LocalDeclEncoder(Zone* zone, const FunctionSig* s = nullptr)
      : sig(s), local_decls(zone), total(0) {}

  // Prepend local declarations by creating a new buffer and copying data
  // over. The new buffer must be delete[]'d by the caller.
  void Prepend(Zone* zone, const uint8_t** start, const uint8_t** end) const;

  size_t Emit(uint8_t* buffer) const;

  // Add locals declarations to this helper. Return the index of the newly added
  // local(s), with an optional adjustment for the parameters.
  uint32_t AddLocals(uint32_t count, ValueType type);

  size_t Size() const;

  bool has_sig() const { return sig != nullptr; }
  const FunctionSig* get_sig() const { return sig; }
  void set_sig(const FunctionSig* s) { sig = s; }

 private:
  const FunctionSig* sig;
  ZoneVector<std::pair<uint32_t, ValueType>> local_decls;
  size_t total;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_LOCAL_DECL_ENCODER_H_
