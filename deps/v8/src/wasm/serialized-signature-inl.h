// Copyright 2023 the V8 project authors. All rights reserved.  Use of
// this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_SERIALIZED_SIGNATURE_INL_H_
#define V8_WASM_SERIALIZED_SIGNATURE_INL_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include "src/handles/handles.h"
#include "src/objects/fixed-array-inl.h"
#include "src/objects/tagged.h"
#include "src/wasm/value-type.h"

namespace v8::internal::wasm {

// The SerializedSignatureHelper helps with the translation of a
// wasm::FunctionSig into a PodArray<wasm::ValueType> and back. The serialized
// format in the PodArray starts with the return count, followed by the return
// types array and the parameter types array.
class SerializedSignatureHelper {
 public:
  // Allocates a PodArray for the serialized signature and sets the return
  // count, but does not fill in the return types and parameter types yet.
  static inline Handle<PodArray<wasm::ValueType>> NewEmptyPodArrayForSignature(
      Isolate* isolate, size_t return_count, size_t param_count) {
    size_t sig_size = return_count + param_count + 1;
    Handle<PodArray<wasm::ValueType>> result = PodArray<wasm::ValueType>::New(
        isolate, static_cast<int>(sig_size), AllocationType::kOld);

    result->set(0, wasm::ValueType::FromRawBitField(
                       static_cast<uint32_t>(return_count)));
    return result;
  }

  static inline Handle<PodArray<wasm::ValueType>> SerializeSignature(
      Isolate* isolate, const wasm::FunctionSig* sig) {
    Handle<PodArray<wasm::ValueType>> result = NewEmptyPodArrayForSignature(
        isolate, sig->return_count(), sig->parameter_count());
    if (!sig->all().empty()) {
      result->copy_in(1, sig->all().begin(),
                      static_cast<int>(sig->all().size()));
    }
    return result;
  }

  static inline wasm::FunctionSig* DeserializeSignature(
      Zone* zone, Tagged<PodArray<wasm::ValueType>> sig) {
    int sig_size = sig->length() - 1;
    int return_count = ReturnCount(sig);
    int param_count = sig_size - return_count;
    wasm::ValueType* types = zone->AllocateArray<wasm::ValueType>(sig_size);
    if (sig_size > 0) {
      sig->copy_out(1, types, sig_size);
    }
    return zone->New<wasm::FunctionSig>(return_count, param_count, types);
  }

  // Deserializes a PodArray signature into a wasm::FunctionSig. The memory
  // allocated for the reps array is set in {out_reps} so that the caller has
  // ownership of the allocated data.
  static inline wasm::FunctionSig DeserializeSignature(
      Tagged<PodArray<wasm::ValueType>> sig,
      std::unique_ptr<wasm::ValueType[]>* out_reps) {
    int sig_size = sig->length() - 1;
    int return_count = ReturnCount(sig);
    int param_count = sig_size - return_count;
    out_reps->reset(new wasm::ValueType[sig_size]);
    if (sig_size > 0) {
      sig->copy_out(1, out_reps->get(), sig_size);
    }
    return wasm::FunctionSig(return_count, param_count, out_reps->get());
  }

  static inline int ReturnCount(Tagged<PodArray<wasm::ValueType>> sig) {
    return sig->get(0).raw_bit_field();
  }

  static inline int ParamCount(Tagged<PodArray<wasm::ValueType>> sig) {
    return sig->length() - ReturnCount(sig) - 1;
  }

  static inline void SetReturn(Tagged<PodArray<wasm::ValueType>> sig,
                               size_t index, wasm::ValueType type) {
    sig->set(static_cast<int>(1 + index), type);
  }

  static inline void SetParam(Tagged<PodArray<wasm::ValueType>> sig,
                              size_t index, wasm::ValueType type) {
    sig->set(static_cast<int>(1 + index + ReturnCount(sig)), type);
  }

  static inline wasm::ValueType GetReturn(Tagged<PodArray<wasm::ValueType>> sig,
                                          size_t index) {
    return sig->get(static_cast<int>(1 + index));
  }

  static inline wasm::ValueType GetParam(Tagged<PodArray<wasm::ValueType>> sig,
                                         size_t index) {
    return sig->get(static_cast<int>(1 + index + ReturnCount(sig)));
  }
};

}  // namespace v8::internal::wasm

#endif  // V8_WASM_SERIALIZED_SIGNATURE_INL_H_
