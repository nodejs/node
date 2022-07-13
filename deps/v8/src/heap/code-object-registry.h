// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_CODE_OBJECT_REGISTRY_H_
#define V8_HEAP_CODE_OBJECT_REGISTRY_H_

#include <vector>

#include "src/base/macros.h"
#include "src/base/platform/mutex.h"
#include "src/common/globals.h"

namespace v8 {
namespace internal {

// The CodeObjectRegistry holds all start addresses of code objects of a given
// MemoryChunk. Each MemoryChunk owns a separate CodeObjectRegistry. The
// CodeObjectRegistry allows fast lookup from an inner pointer of a code object
// to the actual code object.
class V8_EXPORT_PRIVATE CodeObjectRegistry {
 public:
  void RegisterNewlyAllocatedCodeObject(Address code);
  void RegisterAlreadyExistingCodeObject(Address code);
  void Clear();
  void Finalize();
  bool Contains(Address code) const;
  Address GetCodeObjectStartFromInnerAddress(Address address) const;

 private:
  // A vector of addresses, which may be sorted. This is set to 'mutable' so
  // that it can be lazily sorted during GetCodeObjectStartFromInnerAddress.
  mutable std::vector<Address> code_object_registry_;
  mutable bool is_sorted_ = true;
  mutable base::Mutex code_object_registry_mutex_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_CODE_OBJECT_REGISTRY_H_
