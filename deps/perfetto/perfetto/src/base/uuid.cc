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

#include "perfetto/ext/base/uuid.h"

#include <random>

#include "perfetto/base/time.h"

namespace perfetto {
namespace base {
namespace {

constexpr char kHexmap[] = {'0', '1', '2', '3', '4', '5', '6', '7',
                            '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
}  // namespace

// See https://www.ietf.org/rfc/rfc4122.txt
Uuid Uuidv4() {
  static std::minstd_rand rng(static_cast<uint32_t>(GetBootTimeNs().count()));
  Uuid uuid;
  auto& data = *uuid.data();

  for (size_t i = 0; i < 16; ++i)
    data[i] = static_cast<uint8_t>(rng());

  // version:
  data[6] = (data[6] & 0x0f) | 0x40;
  // clock_seq_hi_and_reserved:
  data[8] = (data[8] & 0x3f) | 0x80;

  return uuid;
}

Uuid::Uuid() {}

Uuid::Uuid(const std::string& s) {
  PERFETTO_CHECK(s.size() == data_.size());
  memcpy(data_.data(), s.data(), s.size());
}

Uuid::Uuid(int64_t lsb, int64_t msb) {
  set_lsb_msb(lsb, msb);
}

std::string Uuid::ToString() const {
  return std::string(reinterpret_cast<const char*>(data_.data()), data_.size());
}

std::string Uuid::ToPrettyString() const {
  std::string s(data_.size() * 2 + 4, '-');
  // Format is 123e4567-e89b-12d3-a456-426655443322.
  size_t j = 0;
  for (size_t i = 0; i < data_.size(); ++i) {
    if (i == 4 || i == 6 || i == 8 || i == 10)
      j++;
    s[2 * i + j] = kHexmap[(data_[data_.size() - i - 1] & 0xf0) >> 4];
    s[2 * i + 1 + j] = kHexmap[(data_[data_.size() - i - 1] & 0x0f)];
  }
  return s;
}

}  // namespace base
}  // namespace perfetto
