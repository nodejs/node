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

#ifndef SRC_PROTOZERO_TEST_FAKE_SCATTERED_BUFFER_H_
#define SRC_PROTOZERO_TEST_FAKE_SCATTERED_BUFFER_H_

#include <memory>
#include <string>
#include <vector>

#include "perfetto/protozero/scattered_stream_writer.h"

namespace protozero {

// A simple ScatteredStreamWriter::Delegate implementation which just allocates
// chunks of a fixed size.
class FakeScatteredBuffer : public ScatteredStreamWriter::Delegate {
 public:
  explicit FakeScatteredBuffer(size_t chunk_size);
  ~FakeScatteredBuffer() override;

  // ScatteredStreamWriter::Delegate implementation.
  ContiguousMemoryRange GetNewBuffer() override;

  std::string GetChunkAsString(size_t chunk_index);

  void GetBytes(size_t start, size_t length, uint8_t* buf);
  std::string GetBytesAsString(size_t start, size_t length);

  const std::vector<std::unique_ptr<uint8_t[]>>& chunks() const {
    return chunks_;
  }

 private:
  const size_t chunk_size_;
  std::vector<std::unique_ptr<uint8_t[]>> chunks_;
};

}  // namespace protozero

#endif  // SRC_PROTOZERO_TEST_FAKE_SCATTERED_BUFFER_H_
