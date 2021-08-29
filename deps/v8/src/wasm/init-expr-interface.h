// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#ifndef V8_WASM_INIT_EXPR_INTERFACE_H_
#define V8_WASM_INIT_EXPR_INTERFACE_H_

#include "src/wasm/decoder.h"
#include "src/wasm/function-body-decoder-impl.h"
#include "src/wasm/wasm-value.h"

namespace v8 {
namespace internal {

class WasmInstanceObject;
class JSArrayBuffer;

namespace wasm {

// An interface for WasmFullDecoder used to decode initializer expressions. This
// interface has two modes: only validation (when {isolate_ == nullptr}), which
// is used in module-decoder, and code-generation (when {isolate_ != nullptr}),
// which is used in module-instantiate. We merge two distinct functionalities
// in one class to reduce the number of WasmFullDecoder instantiations, and thus
// V8 binary code size.
class InitExprInterface {
 public:
  static constexpr Decoder::ValidateFlag validate = Decoder::kFullValidation;
  static constexpr DecodingMode decoding_mode = kInitExpression;

  struct Value : public ValueBase<validate> {
    WasmValue runtime_value;

    template <typename... Args>
    explicit Value(Args&&... args) V8_NOEXCEPT
        : ValueBase(std::forward<Args>(args)...) {}
  };

  using Control = ControlBase<Value, validate>;
  using FullDecoder =
      WasmFullDecoder<validate, InitExprInterface, decoding_mode>;

  InitExprInterface(const WasmModule* module, Isolate* isolate,
                    Handle<WasmInstanceObject> instance,
                    Handle<FixedArray> tagged_globals,
                    Handle<JSArrayBuffer> untagged_globals)
      : module_(module),
        outer_module_(nullptr),
        isolate_(isolate),
        instance_(instance),
        tagged_globals_(tagged_globals),
        untagged_globals_(untagged_globals) {
    DCHECK_NOT_NULL(isolate);
  }

  explicit InitExprInterface(WasmModule* outer_module)
      : module_(nullptr), outer_module_(outer_module), isolate_(nullptr) {}

#define EMPTY_INTERFACE_FUNCTION(name, ...) \
  V8_INLINE void name(FullDecoder* decoder, ##__VA_ARGS__) {}
  INTERFACE_META_FUNCTIONS(EMPTY_INTERFACE_FUNCTION)
  INTERFACE_NON_CONSTANT_FUNCTIONS(EMPTY_INTERFACE_FUNCTION)
#undef EMPTY_INTERFACE_FUNCTION

#define DECLARE_INTERFACE_FUNCTION(name, ...) \
  void name(FullDecoder* decoder, ##__VA_ARGS__);
  INTERFACE_CONSTANT_FUNCTIONS(DECLARE_INTERFACE_FUNCTION)
#undef DECLARE_INTERFACE_FUNCTION

  WasmValue result() {
    DCHECK_NOT_NULL(isolate_);
    return result_;
  }
  bool end_found() { return end_found_; }

 private:
  byte* GetRawUntaggedGlobalPtr(const WasmGlobal& global);

  bool end_found_ = false;
  WasmValue result_;
  const WasmModule* module_;
  WasmModule* outer_module_;
  Isolate* isolate_;
  Handle<WasmInstanceObject> instance_;
  Handle<FixedArray> tagged_globals_;
  Handle<JSArrayBuffer> untagged_globals_;
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_INIT_EXPR_INTERFACE_H_
