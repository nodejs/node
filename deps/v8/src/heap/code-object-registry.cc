// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/code-object-registry.h"

#include <algorithm>

#include "src/base/logging.h"

namespace v8 {
namespace internal {

void CodeObjectRegistry::RegisterNewlyAllocatedCodeObject(Address code) {
  if (is_sorted_) {
    is_sorted_ =
        (code_object_registry_.empty() || code_object_registry_.back() < code);
  }
  code_object_registry_.push_back(code);
}

void CodeObjectRegistry::RegisterAlreadyExistingCodeObject(Address code) {
  DCHECK(is_sorted_);
  DCHECK(code_object_registry_.empty() || code_object_registry_.back() < code);
  code_object_registry_.push_back(code);
}

void CodeObjectRegistry::Clear() {
  code_object_registry_.clear();
  is_sorted_ = true;
}

void CodeObjectRegistry::Finalize() {
  DCHECK(is_sorted_);
  code_object_registry_.shrink_to_fit();
}

bool CodeObjectRegistry::Contains(Address object) const {
  if (!is_sorted_) {
    std::sort(code_object_registry_.begin(), code_object_registry_.end());
    is_sorted_ = true;
  }
  return (std::binary_search(code_object_registry_.begin(),
                             code_object_registry_.end(), object));
}

Address CodeObjectRegistry::GetCodeObjectStartFromInnerAddress(
    Address address) const {
  if (!is_sorted_) {
    std::sort(code_object_registry_.begin(), code_object_registry_.end());
    is_sorted_ = true;
  }

  // The code registry can't be empty, else the code object can't exist.
  DCHECK(!code_object_registry_.empty());

  // std::upper_bound returns the first code object strictly greater than
  // address, so the code object containing the address has to be the previous
  // one.
  auto it = std::upper_bound(code_object_registry_.begin(),
                             code_object_registry_.end(), address);
  // The address has to be contained in a code object, so necessarily the
  // address can't be smaller than the first code object.
  DCHECK_NE(it, code_object_registry_.begin());
  return *(--it);
}

}  // namespace internal
}  // namespace v8
