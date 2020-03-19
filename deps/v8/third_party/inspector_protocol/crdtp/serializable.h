// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_CRDTP_SERIALIZABLE_H_
#define V8_CRDTP_SERIALIZABLE_H_

#include <cstdint>
#include <vector>
#include "export.h"

namespace v8_crdtp {
// =============================================================================
// Serializable - An object to be emitted as a sequence of bytes.
// =============================================================================

class Serializable {
 public:
  // Convenience: Invokes |AppendSerialized| on an empty vector.
  std::vector<uint8_t> Serialize() const;

  virtual void AppendSerialized(std::vector<uint8_t>* out) const = 0;

  virtual ~Serializable() = default;
};
}  // namespace v8_crdtp

#endif  // V8_CRDTP_SERIALIZABLE_H_
