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

#include "src/protozero/test/fake_scattered_buffer.h"

#include <sstream>
#include <utility>

#include "test/gtest_and_gmock.h"

namespace protozero {

namespace {

std::string ToHex(const void* data, size_t length) {
  std::ostringstream ss;
  ss << std::hex << std::setfill('0');
  ss << std::uppercase;
  for (size_t i = 0; i < length; i++) {
    char c = reinterpret_cast<const char*>(data)[i];
    ss << std::setw(2) << (static_cast<unsigned>(c) & 0xFF);
  }
  return ss.str();
}

}  // namespace

FakeScatteredBuffer::FakeScatteredBuffer(size_t chunk_size)
    : chunk_size_(chunk_size) {}

FakeScatteredBuffer::~FakeScatteredBuffer() {}

ContiguousMemoryRange FakeScatteredBuffer::GetNewBuffer() {
  std::unique_ptr<uint8_t[]> chunk(new uint8_t[chunk_size_]);
  uint8_t* begin = chunk.get();
  memset(begin, 0, chunk_size_);
  chunks_.push_back(std::move(chunk));
  return {begin, begin + chunk_size_};
}

std::string FakeScatteredBuffer::GetChunkAsString(size_t chunk_index) {
  return ToHex(chunks_[chunk_index].get(), chunk_size_);
}

void FakeScatteredBuffer::GetBytes(size_t start, size_t length, uint8_t* buf) {
  ASSERT_LE(start + length, chunks_.size() * chunk_size_);
  for (size_t pos = 0; pos < length; ++pos) {
    size_t chunk_index = (start + pos) / chunk_size_;
    size_t chunk_offset = (start + pos) % chunk_size_;
    buf[pos] = chunks_[chunk_index].get()[chunk_offset];
  }
}

std::string FakeScatteredBuffer::GetBytesAsString(size_t start, size_t length) {
  std::unique_ptr<uint8_t[]> buffer(new uint8_t[length]);
  GetBytes(start, length, buffer.get());
  return ToHex(buffer.get(), length);
}

}  // namespace protozero
