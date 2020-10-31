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

#include <stddef.h>
#include <stdint.h>

#include "perfetto/base/logging.h"
#include "perfetto/protozero/proto_decoder.h"

namespace protozero {
namespace {

int FuzzProtoDecoder(const uint8_t* data, size_t size) {
  volatile uint64_t value = 0;
  ProtoDecoder dec(data, size);
  for (auto field = dec.ReadField(); field.valid(); field = dec.ReadField()) {
    value += field.raw_int_value();
  }
  TypedProtoDecoder<1, 0> typed_decoder_1(data, size);
  TypedProtoDecoder<999, 0> typed_decoder_2(data, size);
  return 0;
}

}  // namespace
}  // namespace protozero

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size);

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size) {
  return protozero::FuzzProtoDecoder(data, size);
}
