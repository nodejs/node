// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_STREAMING_DECODER_H_
#define V8_WASM_STREAMING_DECODER_H_

#include <memory>
#include <vector>

#include "src/base/macros.h"
#include "src/utils/vector.h"
#include "src/wasm/compilation-environment.h"
#include "src/wasm/wasm-constants.h"
#include "src/wasm/wasm-result.h"

namespace v8 {
namespace internal {
namespace wasm {
class NativeModule;

// This class is an interface for the StreamingDecoder to start the processing
// of the incoming module bytes.
class V8_EXPORT_PRIVATE StreamingProcessor {
 public:
  virtual ~StreamingProcessor() = default;
  // Process the first 8 bytes of a WebAssembly module. Returns true if the
  // processing finished successfully and the decoding should continue.
  virtual bool ProcessModuleHeader(Vector<const uint8_t> bytes,
                                   uint32_t offset) = 0;

  // Process all sections but the code section. Returns true if the processing
  // finished successfully and the decoding should continue.
  virtual bool ProcessSection(SectionCode section_code,
                              Vector<const uint8_t> bytes, uint32_t offset) = 0;

  // Process the start of the code section. Returns true if the processing
  // finished successfully and the decoding should continue.
  virtual bool ProcessCodeSectionHeader(int num_functions, uint32_t offset,
                                        std::shared_ptr<WireBytesStorage>,
                                        int code_section_length) = 0;

  // Process a function body. Returns true if the processing finished
  // successfully and the decoding should continue.
  virtual bool ProcessFunctionBody(Vector<const uint8_t> bytes,
                                   uint32_t offset) = 0;

  // Report the end of a chunk.
  virtual void OnFinishedChunk() = 0;
  // Report the end of the stream. If the stream was successful, all
  // received bytes are passed by parameter. If there has been an error, an
  // empty array is passed.
  virtual void OnFinishedStream(OwnedVector<uint8_t> bytes) = 0;
  // Report an error detected in the StreamingDecoder.
  virtual void OnError(const WasmError&) = 0;
  // Report the abortion of the stream.
  virtual void OnAbort() = 0;

  // Attempt to deserialize the module. Supports embedder caching.
  virtual bool Deserialize(Vector<const uint8_t> module_bytes,
                           Vector<const uint8_t> wire_bytes) = 0;
};

// The StreamingDecoder takes a sequence of byte arrays, each received by a call
// of {OnBytesReceived}, and extracts the bytes which belong to section payloads
// and function bodies.
class V8_EXPORT_PRIVATE StreamingDecoder {
 public:
  explicit StreamingDecoder(std::unique_ptr<StreamingProcessor> processor);

  // The buffer passed into OnBytesReceived is owned by the caller.
  void OnBytesReceived(Vector<const uint8_t> bytes);

  void Finish();

  void Abort();

  // Notify the StreamingDecoder that compilation ended and the
  // StreamingProcessor should not be called anymore.
  void NotifyCompilationEnded() { Fail(); }

  // Caching support.
  // Sets the callback that is called after the module is fully compiled.
  using ModuleCompiledCallback =
      std::function<void(const std::shared_ptr<NativeModule>&)>;
  void SetModuleCompiledCallback(ModuleCompiledCallback callback);
  // Passes previously compiled module bytes from the embedder's cache.
  bool SetCompiledModuleBytes(Vector<const uint8_t> compiled_module_bytes);

  void NotifyNativeModuleCreated(
      const std::shared_ptr<NativeModule>& native_module);

  Vector<const char> url() { return VectorOf(url_); }
  void SetUrl(Vector<const char> url) {
    url_.assign(url.begin(), url.length());
  }

 private:
  // TODO(ahaas): Put the whole private state of the StreamingDecoder into the
  // cc file (PIMPL design pattern).

  // The SectionBuffer is the data object for the content of a single section.
  // It stores all bytes of the section (including section id and section
  // length), and the offset where the actual payload starts.
  class SectionBuffer : public WireBytesStorage {
   public:
    // id: The section id.
    // payload_length: The length of the payload.
    // length_bytes: The section length, as it is encoded in the module bytes.
    SectionBuffer(uint32_t module_offset, uint8_t id, size_t payload_length,
                  Vector<const uint8_t> length_bytes)
        :  // ID + length + payload
          module_offset_(module_offset),
          bytes_(OwnedVector<uint8_t>::New(1 + length_bytes.length() +
                                           payload_length)),
          payload_offset_(1 + length_bytes.length()) {
      bytes_.start()[0] = id;
      memcpy(bytes_.start() + 1, &length_bytes.first(), length_bytes.length());
    }

    SectionCode section_code() const {
      return static_cast<SectionCode>(bytes_.start()[0]);
    }

    Vector<const uint8_t> GetCode(WireBytesRef ref) const final {
      DCHECK_LE(module_offset_, ref.offset());
      uint32_t offset_in_code_buffer = ref.offset() - module_offset_;
      return bytes().SubVector(offset_in_code_buffer,
                               offset_in_code_buffer + ref.length());
    }

    uint32_t module_offset() const { return module_offset_; }
    Vector<uint8_t> bytes() const { return bytes_.as_vector(); }
    Vector<uint8_t> payload() const { return bytes() + payload_offset_; }
    size_t length() const { return bytes_.size(); }
    size_t payload_offset() const { return payload_offset_; }

   private:
    const uint32_t module_offset_;
    const OwnedVector<uint8_t> bytes_;
    const size_t payload_offset_;
  };

  // The decoding of a stream of wasm module bytes is organized in states. Each
  // state provides a buffer to store the bytes required for the current state,
  // information on how many bytes have already been received, how many bytes
  // are needed, and a {Next} function which starts the next state once all
  // bytes of the current state were received.
  //
  // The states change according to the following state diagram:
  //
  //       Start
  //         |
  //         |
  //         v
  // DecodeModuleHeader
  //         |   _________________________________________
  //         |   |                                        |
  //         v   v                                        |
  //  DecodeSectionID --> DecodeSectionLength --> DecodeSectionPayload
  //         A                  |
  //         |                  | (if the section id == code)
  //         |                  v
  //         |      DecodeNumberOfFunctions -- > DecodeFunctionLength
  //         |                                          A    |
  //         |                                          |    |
  //         |  (after all functions were read)         |    v
  //         ------------------------------------- DecodeFunctionBody
  //
  class DecodingState {
   public:
    virtual ~DecodingState() = default;

    // Reads the bytes for the current state and returns the number of read
    // bytes.
    virtual size_t ReadBytes(StreamingDecoder* streaming,
                             Vector<const uint8_t> bytes);

    // Returns the next state of the streaming decoding.
    virtual std::unique_ptr<DecodingState> Next(
        StreamingDecoder* streaming) = 0;
    // The buffer to store the received bytes.
    virtual Vector<uint8_t> buffer() = 0;
    // The number of bytes which were already received.
    size_t offset() const { return offset_; }
    void set_offset(size_t value) { offset_ = value; }
    // A flag to indicate if finishing the streaming decoder is allowed without
    // error.
    virtual bool is_finishing_allowed() const { return false; }

   private:
    size_t offset_ = 0;
  };

  // Forward declarations of the concrete states. This is needed so that they
  // can access private members of the StreamingDecoder.
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
                                 Vector<const uint8_t> length_bytes);

  std::unique_ptr<DecodingState> Error(const WasmError& error) {
    if (ok()) processor_->OnError(error);
    Fail();
    return std::unique_ptr<DecodingState>(nullptr);
  }

  std::unique_ptr<DecodingState> Error(std::string message) {
    return Error(WasmError{module_offset_ - 1, std::move(message)});
  }

  void ProcessModuleHeader() {
    if (!ok()) return;
    if (!processor_->ProcessModuleHeader(state_->buffer(), 0)) Fail();
  }

  void ProcessSection(SectionBuffer* buffer) {
    if (!ok()) return;
    if (!processor_->ProcessSection(
            buffer->section_code(), buffer->payload(),
            buffer->module_offset() +
                static_cast<uint32_t>(buffer->payload_offset()))) {
      Fail();
    }
  }

  void StartCodeSection(int num_functions,
                        std::shared_ptr<WireBytesStorage> wire_bytes_storage,
                        int code_section_length) {
    if (!ok()) return;
    // The offset passed to {ProcessCodeSectionHeader} is an error offset and
    // not the start offset of a buffer. Therefore we need the -1 here.
    if (!processor_->ProcessCodeSectionHeader(
            num_functions, module_offset() - 1, std::move(wire_bytes_storage),
            code_section_length)) {
      Fail();
    }
  }

  void ProcessFunctionBody(Vector<const uint8_t> bytes,
                           uint32_t module_offset) {
    if (!ok()) return;
    if (!processor_->ProcessFunctionBody(bytes, module_offset)) Fail();
  }

  void Fail() {
    // We reset the {processor_} field to represent failure. This also ensures
    // that we do not accidentally call further methods on the processor after
    // failure.
    processor_.reset();
  }

  bool ok() const { return processor_ != nullptr; }

  uint32_t module_offset() const { return module_offset_; }

  bool deserializing() const { return !compiled_module_bytes_.empty(); }

  std::unique_ptr<StreamingProcessor> processor_;
  std::unique_ptr<DecodingState> state_;
  std::vector<std::shared_ptr<SectionBuffer>> section_buffers_;
  bool code_section_processed_ = false;
  uint32_t module_offset_ = 0;
  size_t total_size_ = 0;
  std::string url_;

  // Caching support.
  ModuleCompiledCallback module_compiled_callback_ = nullptr;
  // We need wire bytes in an array for deserializing cached modules.
  std::vector<uint8_t> wire_bytes_for_deserializing_;
  Vector<const uint8_t> compiled_module_bytes_;

  DISALLOW_COPY_AND_ASSIGN(StreamingDecoder);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_STREAMING_DECODER_H_
