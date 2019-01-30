// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INSPECTOR_PROTOCOL_ENCODING_PLATFORM_H_
#define INSPECTOR_PROTOCOL_ENCODING_PLATFORM_H_

#include <memory>

namespace inspector_protocol {
// Client code must provide an instance. Implementation should delegate
// to whatever is appropriate.
class Platform {
 public:
  virtual ~Platform() = default;
  // Parses |str| into |result|. Returns false iff there are
  // leftover characters or parsing errors.
  virtual bool StrToD(const char* str, double* result) const = 0;

  // Prints |value| in a format suitable for JSON.
  virtual std::unique_ptr<char[]> DToStr(double value) const = 0;
};
}  // namespace inspector_protocol

#endif  // INSPECTOR_PROTOCOL_ENCODING_PLATFORM_H_
