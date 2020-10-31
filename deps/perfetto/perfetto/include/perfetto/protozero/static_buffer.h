/*
 * Copyright (C) 2019 The Android Open Source Project
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

#ifndef INCLUDE_PERFETTO_PROTOZERO_STATIC_BUFFER_H_
#define INCLUDE_PERFETTO_PROTOZERO_STATIC_BUFFER_H_

#include <memory>
#include <string>
#include <vector>

#include "perfetto/base/export.h"
#include "perfetto/protozero/scattered_stream_writer.h"

namespace protozero {

class Message;

// A simple implementation of ScatteredStreamWriter::Delegate backed by a
// fixed-size buffer. It doesn't support expansion. The caller needs to ensure
// to never write more than the size of the buffer. Will CHECK() otherwise.
class PERFETTO_EXPORT StaticBufferDelegate
    : public ScatteredStreamWriter::Delegate {
 public:
  StaticBufferDelegate(uint8_t* buf, size_t len) : range_{buf, buf + len} {}
  ~StaticBufferDelegate() override;

  // ScatteredStreamWriter::Delegate implementation.
  ContiguousMemoryRange GetNewBuffer() override;

  ContiguousMemoryRange const range_;
  bool get_new_buffer_called_once_ = false;
};

// Helper function to create protozero messages backed by a fixed-size buffer
// in one line. You can write:
//   protozero::Static<protozero::MyMessage> msg(buf.data(), buf.size());
//   msg->set_stuff(...);
//   size_t bytes_encoded = msg.Finalize();
template <typename T /* protozero::Message */>
class StaticBuffered {
 public:
  StaticBuffered(void* buf, size_t len)
      : delegate_(reinterpret_cast<uint8_t*>(buf), len), writer_(&delegate_) {
    msg_.Reset(&writer_);
  }

  // This can't be neither copied nor moved because Message hands out pointers
  // to itself when creating submessages.
  StaticBuffered(const StaticBuffered&) = delete;
  StaticBuffered& operator=(const StaticBuffered&) = delete;
  StaticBuffered(StaticBuffered&&) = delete;
  StaticBuffered& operator=(StaticBuffered&&) = delete;

  T* get() { return &msg_; }
  T* operator->() { return &msg_; }

  // The lack of a size() method is deliberate. It's to prevent that one
  // accidentally calls size() before Finalize().

  // Returns the number of encoded bytes (<= the size passed in the ctor).
  size_t Finalize() {
    msg_.Finalize();
    return static_cast<size_t>(writer_.write_ptr() - delegate_.range_.begin);
  }

 private:
  StaticBufferDelegate delegate_;
  ScatteredStreamWriter writer_;
  T msg_;
};

// Helper function to create stack-based protozero messages in one line.
// You can write:
//   protozero::StackBuffered<protozero::MyMessage, 16> msg;
//   msg->set_stuff(...);
//   size_t bytes_encoded = msg.Finalize();
template <typename T /* protozero::Message */, size_t N>
class StackBuffered : public StaticBuffered<T> {
 public:
  StackBuffered() : StaticBuffered<T>(&buf_[0], N) {}

 private:
  uint8_t buf_[N];  // Deliberately not initialized.
};

}  // namespace protozero

#endif  // INCLUDE_PERFETTO_PROTOZERO_STATIC_BUFFER_H_
