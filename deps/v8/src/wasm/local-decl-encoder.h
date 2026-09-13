// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_LOCAL_DECL_ENCODER_H_
#define V8_WASM_LOCAL_DECL_ENCODER_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

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
      : sig_(s), local_decls_(zone), total_(0) {}

  size_t Emit(uint8_t* buffer) const;

  // Add locals declarations to this helper. Return the index of the newly added
  // local(s), with an optional adjustment for the parameters.
  uint32_t AddLocals(uint32_t count, ValueType type);

  size_t Size() const;

  bool has_sig() const { return sig_ != nullptr; }
  void set_sig(const FunctionSig* s) { sig_ = s; }

 private:
  const FunctionSig* sig_;
  ZoneVector<std::pair<uint32_t, ValueType>> local_decls_;
  size_t total_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_LOCAL_DECL_ENCODER_H_
