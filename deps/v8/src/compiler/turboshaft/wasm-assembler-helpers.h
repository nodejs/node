// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_TURBOSHAFT_WASM_ASSEMBLER_HELPERS_H_
#define V8_COMPILER_TURBOSHAFT_WASM_ASSEMBLER_HELPERS_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

namespace v8::internal::compiler::turboshaft {

#define LOAD_INSTANCE_FIELD(instance, name, representation)           \
  __ Load(instance, compiler::turboshaft::LoadOp::Kind::TaggedBase(), \
          representation, WasmTrustedInstanceData::k##name##Offset)

#define LOAD_PROTECTED_INSTANCE_FIELD(instance, name, type)       \
  V<type>::Cast(__ LoadProtectedPointerField(                     \
      instance, compiler::turboshaft::LoadOp::Kind::TaggedBase(), \
      WasmTrustedInstanceData::kProtected##name##Offset))

#define LOAD_IMMUTABLE_PROTECTED_INSTANCE_FIELD(instance, name, type)         \
  V<type>::Cast(__ LoadProtectedPointerField(                                 \
      instance, compiler::turboshaft::LoadOp::Kind::TaggedBase().Immutable(), \
      WasmTrustedInstanceData::kProtected##name##Offset))

#define LOAD_IMMUTABLE_INSTANCE_FIELD(instance, name, representation)   \
  __ Load(instance,                                                     \
          compiler::turboshaft::LoadOp::Kind::TaggedBase().Immutable(), \
          representation, WasmTrustedInstanceData::k##name##Offset)

}  // namespace v8::internal::compiler::turboshaft

#endif  // V8_COMPILER_TURBOSHAFT_WASM_ASSEMBLER_HELPERS_H_
