/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef SRC_IPC_BUFFERED_FRAME_DESERIALIZER_H_
#define SRC_IPC_BUFFERED_FRAME_DESERIALIZER_H_

#include <stddef.h>

#include <list>
#include <memory>

#include <sys/mman.h>

#include "perfetto/ext/base/paged_memory.h"
#include "perfetto/ext/base/utils.h"
#include "perfetto/ext/ipc/basic_types.h"

namespace perfetto {

namespace protos {
namespace gen {
class IPCFrame;
}  // namespace gen
}  // namespace protos

namespace ipc {

using Frame = ::perfetto::protos::gen::IPCFrame;

// Deserializes incoming frames, taking care of buffering and tokenization.
// Used by both host and client to decode incoming frames.
//
// Which problem does it solve?
// ----------------------------
// The wire protocol is as follows:
// [32-bit frame size][proto-encoded Frame], e.g:
// [06 00 00 00][00 11 22 33 44 55 66]
// [02 00 00 00][AA BB]
// [04 00 00 00][CC DD EE FF]
// However, given that the socket works in SOCK_STREAM mode, the recv() calls
// might see the following:
// 06 00 00
// 00 00 11 22 33 44 55
// 66 02 00 00 00 ...
// This class takes care of buffering efficiently the data received, without
// making any assumption on how the incoming data will be chunked by the socket.
// For instance, it is possible that a recv() doesn't produce any frame (because
// it received only a part of the frame) or produces more than one frame.
//
// Usage
// -----
// Both host and client use this as follows:
//
// auto buf = rpc_frame_decoder.BeginReceive();
// size_t rsize = socket.recv(buf.first, buf.second);
// rpc_frame_decoder.EndReceive(rsize);
// while (Frame frame = rpc_frame_decoder.PopNextFrame()) {
//   ... process |frame|
// }
//
// Design goals:
// -------------
// - Optimize for the realistic case of each recv() receiving one or more
//   whole frames. In this case no memmove is performed.
// - Guarantee that frames lay in a virtually contiguous memory area.
//   This allows to use the protobuf-lite deserialization API (scattered
//   deserialization is supported only by libprotobuf-full).
// - Put a hard boundary to the size of the incoming buffer. This is to prevent
//   that a malicious sends an abnormally large frame and OOMs us.
// - Simplicity: just use a linear mmap region. No reallocations or scattering.
//   Takes care of madvise()-ing unused memory.

class BufferedFrameDeserializer {
 public:
  struct ReceiveBuffer {
    char* data;
    size_t size;
  };

  // |max_capacity| is overridable only for tests.
  explicit BufferedFrameDeserializer(size_t max_capacity = kIPCBufferSize);
  ~BufferedFrameDeserializer();

  // This function doesn't really belong here as it does Serialization, unlike
  // the rest of this class. However it is so small and has so many dependencies
  // in common that doesn't justify having its own class.
  static std::string Serialize(const Frame&);

  // Returns a buffer that can be passed to recv(). The buffer is deliberately
  // not initialized.
  ReceiveBuffer BeginReceive();

  // Must be called soon after BeginReceive().
  // |recv_size| is the number of valid bytes that have been written into the
  // buffer previously returned by BeginReceive() (the return value of recv()).
  // Returns false if a header > |max_capacity| is received, in which case the
  // caller is expected to shutdown the socket and terminate the ipc.
  bool EndReceive(size_t recv_size) PERFETTO_WARN_UNUSED_RESULT;

  // Decodes and returns the next decoded frame in the buffer if any, nullptr
  // if no further frames have been decoded.
  std::unique_ptr<Frame> PopNextFrame();

  size_t capacity() const { return capacity_; }
  size_t size() const { return size_; }

 private:
  BufferedFrameDeserializer(const BufferedFrameDeserializer&) = delete;
  BufferedFrameDeserializer& operator=(const BufferedFrameDeserializer&) =
      delete;

  // If a valid frame is decoded it is added to |decoded_frames_|.
  void DecodeFrame(const char*, size_t);

  char* buf() { return reinterpret_cast<char*>(buf_.Get()); }

  base::PagedMemory buf_;
  const size_t capacity_ = 0;  // sizeof(|buf_|).

  // THe number of bytes in |buf_| that contain valid data (as a result of
  // EndReceive()). This is always <= |capacity_|.
  size_t size_ = 0;

  std::list<std::unique_ptr<Frame>> decoded_frames_;
};

}  // namespace ipc
}  // namespace perfetto

#endif  // SRC_IPC_BUFFERED_FRAME_DESERIALIZER_H_
