// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_STREAMING_DECODER_H_
#define V8_WASM_STREAMING_DECODER_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <memory>

#include "include/v8-wasm.h"  // For WasmStreaming::ModuleCachingInterface.
#include "src/base/macros.h"
#include "src/base/vector.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-engine.h"
#include "src/wasm/wasm-result.h"

namespace v8::internal::wasm {

class NativeModule;

// This class is an interface for the StreamingDecoder to start the processing
// of the incoming module bytes.
class V8_EXPORT_PRIVATE StreamingProcessor {
 public:
  virtual ~StreamingProcessor() = default;

  // Initialize anything isolate-specific in this processor. This can happen
  // late (after passing in bytes already), but must happen before calling
  // `Finish`.
  virtual void InitializeIsolateSpecificInfo(Isolate*) = 0;

  // Process the first 8 bytes of a WebAssembly module. Returns true if the
  // processing finished successfully and the decoding should continue.
  virtual bool ProcessModuleHeader(base::Vector<const uint8_t> bytes) = 0;

  // Process all sections but the code section. Returns true if the processing
  // finished successfully and the decoding should continue.
  virtual bool ProcessSection(SectionCode section_code,
                              base::Vector<const uint8_t> bytes,
                              uint32_t offset) = 0;

  // Process the start of the code section. Returns true if the processing
  // finished successfully and the decoding should continue.
  virtual bool ProcessCodeSectionHeader(int num_functions, uint32_t offset,
                                        std::shared_ptr<WireBytesStorage>,
                                        size_t code_section_start,
                                        size_t code_section_length) = 0;

  // Process a function body. Returns true if the processing finished
  // successfully and the decoding should continue.
  virtual bool ProcessFunctionBody(base::Vector<const uint8_t> bytes,
                                   uint32_t offset) = 0;

  // Report the end of a chunk.
  virtual void OnFinishedChunk() = 0;
  // Report the end of the stream. This will be called even after an error has
  // been detected. In any case, the parameter is the total received bytes.
  virtual void OnFinishedStream(base::OwnedVector<const uint8_t> bytes,
                                bool after_error) = 0;
  // Report the abortion of the stream.
  virtual void OnAbort() = 0;

  // Attempt to deserialize the module. Supports embedder caching.
  // On successful deserialization, ownership of the `wire_bytes` vector is
  // taken over by the deserialized module (the parameter will be reset to an
  // empty vector); otherwise ownership stays with the caller.
  virtual bool Deserialize(base::Vector<const uint8_t> module_bytes,
                           base::OwnedVector<const uint8_t>& wire_bytes) = 0;
};

// The StreamingDecoder takes a sequence of byte arrays, each received by a call
// of {OnBytesReceived}, and extracts the bytes which belong to section payloads
// and function bodies.
class V8_EXPORT_PRIVATE StreamingDecoder {
 public:
  virtual ~StreamingDecoder() = default;

  // Initialize anything isolate-specific in this decoder. This can happen late
  // (after passing in bytes already), but must happen before calling `Finish`.
  virtual void InitializeIsolateSpecificInfo(Isolate*) = 0;

  // The buffer passed into OnBytesReceived is owned by the caller.
  virtual void OnBytesReceived(base::Vector<const uint8_t> bytes) = 0;

  // The argument matches WasmStreaming::GetCachedModuleFn, but we avoid the
  // include and just repeat the full type instead.
  virtual void Finish(const WasmStreaming::ModuleCachingCallback&) = 0;

  virtual void Abort() = 0;

  // Notify the StreamingDecoder that the job was discarded and the
  // StreamingProcessor should not be called anymore.
  virtual void NotifyCompilationDiscarded() = 0;

  // Caching support.
  // Sets the callback that is called after a new chunk of the module is tiered
  // up.
  using MoreFunctionsCanBeSerializedCallback =
      std::function<void(const std::shared_ptr<NativeModule>&)>;

  void SetMoreFunctionsCanBeSerializedCallback(
      MoreFunctionsCanBeSerializedCallback callback) {
    more_functions_can_be_serialized_callback_ = std::move(callback);
  }

  virtual void SetHasCompiledModuleBytes() = 0;

  virtual void NotifyNativeModuleCreated(
      const std::shared_ptr<NativeModule>& native_module) = 0;

  const std::string& url() const { return *url_; }
  std::shared_ptr<const std::string> shared_url() const { return url_; }

  void SetUrl(base::Vector<const char> url) {
    url_->assign(url.begin(), url.size());
  }

  static std::unique_ptr<StreamingDecoder> CreateAsyncStreamingDecoder(
      std::unique_ptr<StreamingProcessor> processor);

  static std::unique_ptr<StreamingDecoder> CreateSyncStreamingDecoder(
      WasmEnabledFeatures enabled, CompileTimeImports compile_imports,
      const char* api_method_name_for_errors,
      std::shared_ptr<CompilationResultResolver> resolver);

 protected:
  const std::shared_ptr<std::string> url_ = std::make_shared<std::string>();
  MoreFunctionsCanBeSerializedCallback
      more_functions_can_be_serialized_callback_;
};

}  // namespace v8::internal::wasm

#endif  // V8_WASM_STREAMING_DECODER_H_
