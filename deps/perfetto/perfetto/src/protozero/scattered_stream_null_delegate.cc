/*
 * Copyright (C) 2018 The Android Open Source Project
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

#include "perfetto/protozero/scattered_stream_null_delegate.h"

namespace protozero {

// An implementation of ScatteredStreamWriter::Delegate which always returns
// the same piece of memory.
// This is used when we need to no-op the writers (e.g. during teardown or in
// case of resource exhaustion), avoiding that the clients have to deal with
// nullptr checks.
ScatteredStreamWriterNullDelegate::ScatteredStreamWriterNullDelegate(
    size_t chunk_size)
    : chunk_size_(chunk_size),
      chunk_(std::unique_ptr<uint8_t[]>(new uint8_t[chunk_size_])) {}

ScatteredStreamWriterNullDelegate::~ScatteredStreamWriterNullDelegate() {}

ContiguousMemoryRange ScatteredStreamWriterNullDelegate::GetNewBuffer() {
  return {chunk_.get(), chunk_.get() + chunk_size_};
}

}  // namespace protozero
