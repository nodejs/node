// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_WASM_STREAMING_DECODER_H_
#define V8_WASM_STREAMING_DECODER_H_

#include <vector>
#include "src/isolate.h"
#include "src/wasm/module-decoder.h"
#include "src/wasm/wasm-objects.h"

namespace v8 {
namespace internal {
namespace wasm {

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
  virtual bool ProcessCodeSectionHeader(size_t num_functions,
                                        uint32_t offset) = 0;

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
  virtual void OnError(DecodeResult result) = 0;
  // Report the abortion of the stream.
  virtual void OnAbort() = 0;
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
  void NotifyCompilationEnded() {
    // We set {ok_} to false to turn all future calls to the StreamingDecoder
    // into no-ops.
    ok_ = false;
  }

 private:
  // TODO(ahaas): Put the whole private state of the StreamingDecoder into the
  // cc file (PIMPL design pattern).

  // The SectionBuffer is the data object for the content of a single section.
  // It stores all bytes of the section (including section id and section
  // length), and the offset where the actual payload starts.
  class SectionBuffer {
   public:
    // id: The section id.
    // payload_length: The length of the payload.
    // length_bytes: The section length, as it is encoded in the module bytes.
    SectionBuffer(uint32_t module_offset, uint8_t id, size_t payload_length,
                  Vector<const uint8_t> length_bytes)
        :  // ID + length + payload
          module_offset_(module_offset),
          length_(1 + length_bytes.length() + payload_length),
          bytes_(new uint8_t[length_]),
          payload_offset_(1 + length_bytes.length()) {
      bytes_[0] = id;
      memcpy(bytes_.get() + 1, &length_bytes.first(), length_bytes.length());
    }

    SectionCode section_code() const {
      return static_cast<SectionCode>(bytes_[0]);
    }

    uint32_t module_offset() const { return module_offset_; }
    uint8_t* bytes() const { return bytes_.get(); }
    size_t length() const { return length_; }
    size_t payload_offset() const { return payload_offset_; }
    size_t payload_length() const { return length_ - payload_offset_; }
    Vector<const uint8_t> payload() const {
      return Vector<const uint8_t>(bytes() + payload_offset(),
                                   payload_length());
    }

   private:
    uint32_t module_offset_;
    size_t length_;
    std::unique_ptr<uint8_t[]> bytes_;
    size_t payload_offset_;
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
    // The number of bytes to be received.
    virtual size_t size() const = 0;
    // The buffer to store the received bytes.
    virtual uint8_t* buffer() = 0;
    // The number of bytes which were already received.
    size_t offset() const { return offset_; }
    void set_offset(size_t value) { offset_ = value; }
    // The number of bytes which are still needed.
    size_t remaining() const { return size() - offset(); }
    bool is_finished() const { return offset() == size(); }
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
  SectionBuffer* CreateNewBuffer(uint32_t module_offset, uint8_t id,
                                 size_t length,
                                 Vector<const uint8_t> length_bytes) {
    // Check the order of sections. Unknown sections can appear at any position.
    if (id != kUnknownSectionCode) {
      if (id < next_section_id_) {
        Error("Unexpected section");
        return nullptr;
      }
      next_section_id_ = id + 1;
    }
    section_buffers_.emplace_back(
        new SectionBuffer(module_offset, id, length, length_bytes));
    return section_buffers_.back().get();
  }

  std::unique_ptr<DecodingState> Error(DecodeResult result) {
    if (ok_) processor_->OnError(std::move(result));
    ok_ = false;
    return std::unique_ptr<DecodingState>(nullptr);
  }

  std::unique_ptr<DecodingState> Error(std::string message) {
    DecodeResult result(nullptr);
    result.error(module_offset_ - 1, std::move(message));
    return Error(std::move(result));
  }

  void ProcessModuleHeader() {
    if (!ok_) return;
    if (!processor_->ProcessModuleHeader(
            Vector<const uint8_t>(state_->buffer(),
                                  static_cast<int>(state_->size())),
            0)) {
      ok_ = false;
    }
  }

  void ProcessSection(SectionBuffer* buffer) {
    if (!ok_) return;
    if (!processor_->ProcessSection(
            buffer->section_code(), buffer->payload(),
            buffer->module_offset() +
                static_cast<uint32_t>(buffer->payload_offset()))) {
      ok_ = false;
    }
  }

  void StartCodeSection(size_t num_functions) {
    if (!ok_) return;
    // The offset passed to {ProcessCodeSectionHeader} is an error offset and
    // not the start offset of a buffer. Therefore we need the -1 here.
    if (!processor_->ProcessCodeSectionHeader(num_functions,
                                              module_offset() - 1)) {
      ok_ = false;
    }
  }

  void ProcessFunctionBody(Vector<const uint8_t> bytes,
                           uint32_t module_offset) {
    if (!ok_) return;
    if (!processor_->ProcessFunctionBody(bytes, module_offset)) ok_ = false;
  }

  bool ok() const { return ok_; }

  uint32_t module_offset() const { return module_offset_; }

  std::unique_ptr<StreamingProcessor> processor_;
  bool ok_ = true;
  std::unique_ptr<DecodingState> state_;
  std::vector<std::unique_ptr<SectionBuffer>> section_buffers_;
  uint32_t module_offset_ = 0;
  size_t total_size_ = 0;
  uint8_t next_section_id_ = kFirstSectionInModule;

  DISALLOW_COPY_AND_ASSIGN(StreamingDecoder);
};

}  // namespace wasm
}  // namespace internal
}  // namespace v8

#endif  // V8_WASM_STREAMING_DECODER_H_
