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

#ifndef INCLUDE_PERFETTO_PROTOZERO_CPP_MESSAGE_OBJ_H_
#define INCLUDE_PERFETTO_PROTOZERO_CPP_MESSAGE_OBJ_H_

#include <stdint.h>

#include <string>
#include <vector>

#include "perfetto/base/export.h"

namespace protozero {

// Base class for generated .gen.h classes, which are full C++ objects that
// support both ser and deserialization (but are not zero-copy).
// This is only used by the "cpp" targets not the "pbzero" ones.
class PERFETTO_EXPORT CppMessageObj {
 public:
  virtual ~CppMessageObj();
  virtual std::string SerializeAsString() const = 0;
  virtual std::vector<uint8_t> SerializeAsArray() const = 0;
  virtual bool ParseFromArray(const void*, size_t) = 0;

  bool ParseFromString(const std::string& str) {
    return ParseFromArray(str.data(), str.size());
  }
};

}  // namespace protozero

#endif  // INCLUDE_PERFETTO_PROTOZERO_CPP_MESSAGE_OBJ_H_
