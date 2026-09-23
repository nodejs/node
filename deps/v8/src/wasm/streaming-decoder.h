// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_STREAMING_DECODER_H_
#define V8_WASM_STREAMING_DECODER_H_

#if !V8_ENABLE_WEBASSEMBLY
#error This header should only be included if WebAssembly is enabled.
#endif  // !V8_ENABLE_WEBASSEMBLY

#include <memory>
#include <vector>

#include "include/v8-wasm.h"  // For WasmStreaming::ModuleCachingInterface.
#include "src/base/macros.h"
#include "src/base/vector.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-result.h"

namespace v8::internal::wasm {

class NativeModule;
class CompilationResultResolver;

// This class is an interface for the StreamingDecoder to start the processing
// of the incoming module bytes. The StreamingDecoder will call the methods
// of the StreamingProcessor as the corresponding parts of the module are
// decoded.
//
// In the case of asynchronous streaming compilation, the
// {AsyncStreamingProcessor} (in module-compiler.cc) is used to connect the
// decoder to an {AsyncCompileJob}.
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
  explicit StreamingDecoder(std::unique_ptr<StreamingProcessor> processor);
  StreamingDecoder(const StreamingDecoder&) = delete;
  StreamingDecoder& operator=(const StreamingDecoder&) = delete;
  virtual ~StreamingDecoder();

  // Initialize anything isolate-specific in this decoder. This can happen late
  // (after passing in bytes already), but must happen before calling `Finish`.
  void InitializeIsolateSpecificInfo(Isolate* isolate);

  // The buffer passed into OnBytesReceived is owned by the caller.
  void OnBytesReceived(base::Vector<const uint8_t> bytes);

  void Finish(const WasmStreaming::ModuleCachingCallback& caching_callback);

  void Abort();

  // Notify the StreamingDecoder that the job is finished and the
  // StreamingProcessor should not be called anymore.
  void NotifyCompilationStopped();

  // Caching support.
  // Sets the callback that is called after a new chunk of the module is tiered
  // up.
  using MoreFunctionsCanBeSerializedCallback =
      std::function<void(const std::shared_ptr<NativeModule>&)>;

  void SetMoreFunctionsCanBeSerializedCallback(
      MoreFunctionsCanBeSerializedCallback callback) {
    more_functions_can_be_serialized_callback_ = std::move(callback);
  }

  void SetHasCompiledModuleBytes();

  void NotifyNativeModuleCreated(
      const std::shared_ptr<NativeModule>& native_module);

  const std::string& url() const { return *url_; }
  std::shared_ptr<const std::string> shared_url() const { return url_; }

  void SetUrl(base::Vector<const char> url) {
    url_->assign(url.begin(), url.size());
  }

  static std::unique_ptr<StreamingDecoder> Create(
      std::unique_ptr<StreamingProcessor> processor);

 private:
  // Use a private nested class to hide implementation details from the header.
  class SectionBuffer;
  class DecodingState;
  class DecodeVarInt32;
  class DecodeModuleHeader;
  class DecodeSectionID;
  class DecodeSectionLength;
  class DecodeSectionPayload;
  class DecodeNumberOfFunctions;
  class DecodeFunctionLength;
  class DecodeFunctionBody;

  // Creates a buffer for the next section of the module.
  SectionBuffer* CreateNewBuffer(uint32_t module_offset, uint8_t section_id,
                                 size_t length,
                                 base::Vector<const uint8_t> length_bytes);

  std::unique_ptr<DecodingState> ToErrorState();
  void ProcessModuleHeader();
  void ProcessSection(SectionBuffer* buffer);
  void StartCodeSection(int num_functions,
                        std::shared_ptr<WireBytesStorage> wire_bytes_storage,
                        size_t code_section_start, size_t code_section_length);
  void ProcessFunctionBody(base::Vector<const uint8_t> bytes,
                           uint32_t module_offset);

  void Fail();
  bool ok() const {
    DCHECK_EQ(processor_ == nullptr, failed_processor_ != nullptr);
    return processor_ != nullptr;
  }

  uint32_t module_offset() const { return module_offset_; }

  enum StreamState { kReceivingBytes, kAborted, kFinished, kDiscarded };
  StreamState stream_state_{kReceivingBytes};

  std::unique_ptr<StreamingProcessor> processor_;
  std::unique_ptr<StreamingProcessor> failed_processor_;
  std::unique_ptr<DecodingState> state_;
  std::vector<std::shared_ptr<SectionBuffer>> section_buffers_;
  bool code_section_processed_ = false;
  uint32_t module_offset_ = 0;
  std::vector<std::vector<uint8_t>> full_wire_bytes_{{}};
  bool has_compiled_module_bytes_ = false;

  const std::shared_ptr<std::string> url_ = std::make_shared<std::string>();
  MoreFunctionsCanBeSerializedCallback
      more_functions_can_be_serialized_callback_;
};

}  // namespace v8::internal::wasm

#endif  // V8_WASM_STREAMING_DECODER_H_
