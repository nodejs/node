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

#include "perfetto/protozero/static_buffer.h"

#include "perfetto/base/logging.h"

namespace protozero {

StaticBufferDelegate::~StaticBufferDelegate() = default;

ContiguousMemoryRange StaticBufferDelegate::GetNewBuffer() {
  if (get_new_buffer_called_once_) {
    // This is the 2nd time GetNewBuffer is called. The estimate is wrong. We
    // shouldn't try to grow the buffer after the initial call.
    PERFETTO_FATAL("Static buffer too small");
  }
  get_new_buffer_called_once_ = true;
  return range_;
}

}  // namespace protozero
