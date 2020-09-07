// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/code-object-registry.h"

#include <algorithm>

#include "src/base/logging.h"

namespace v8 {
namespace internal {

void CodeObjectRegistry::RegisterNewlyAllocatedCodeObject(Address code) {
  auto result = code_object_registry_newly_allocated_.insert(code);
  USE(result);
  DCHECK(result.second);
}

void CodeObjectRegistry::RegisterAlreadyExistingCodeObject(Address code) {
  code_object_registry_already_existing_.push_back(code);
}

void CodeObjectRegistry::Clear() {
  code_object_registry_already_existing_.clear();
  code_object_registry_newly_allocated_.clear();
}

void CodeObjectRegistry::Finalize() {
  code_object_registry_already_existing_.shrink_to_fit();
}

bool CodeObjectRegistry::Contains(Address object) const {
  return (code_object_registry_newly_allocated_.find(object) !=
          code_object_registry_newly_allocated_.end()) ||
         (std::binary_search(code_object_registry_already_existing_.begin(),
                             code_object_registry_already_existing_.end(),
                             object));
}

Address CodeObjectRegistry::GetCodeObjectStartFromInnerAddress(
    Address address) const {
  // Let's first find the object which comes right before address in the vector
  // of already existing code objects.
  Address already_existing_set_ = 0;
  Address newly_allocated_set_ = 0;
  if (!code_object_registry_already_existing_.empty()) {
    auto it =
        std::upper_bound(code_object_registry_already_existing_.begin(),
                         code_object_registry_already_existing_.end(), address);
    if (it != code_object_registry_already_existing_.begin()) {
      already_existing_set_ = *(--it);
    }
  }

  // Next, let's find the object which comes right before address in the set
  // of newly allocated code objects.
  if (!code_object_registry_newly_allocated_.empty()) {
    auto it = code_object_registry_newly_allocated_.upper_bound(address);
    if (it != code_object_registry_newly_allocated_.begin()) {
      newly_allocated_set_ = *(--it);
    }
  }

  // The code objects which contains address has to be in one of the two
  // data structures.
  DCHECK(already_existing_set_ != 0 || newly_allocated_set_ != 0);

  // The address which is closest to the given address is the code object.
  return already_existing_set_ > newly_allocated_set_ ? already_existing_set_
                                                      : newly_allocated_set_;
}

}  // namespace internal
}  // namespace v8
