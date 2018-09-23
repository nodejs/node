// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ARM64_INTERFACE_DESCRIPTORS_ARM64_H_
#define V8_ARM64_INTERFACE_DESCRIPTORS_ARM64_H_

#include "src/interface-descriptors.h"

namespace v8 {
namespace internal {

class PlatformInterfaceDescriptor {
 public:
  explicit PlatformInterfaceDescriptor(TargetAddressStorageMode storage_mode)
      : storage_mode_(storage_mode) {}

  TargetAddressStorageMode storage_mode() { return storage_mode_; }

 private:
  TargetAddressStorageMode storage_mode_;
};
}
}  // namespace v8::internal

#endif  // V8_ARM64_INTERFACE_DESCRIPTORS_ARM64_H_
