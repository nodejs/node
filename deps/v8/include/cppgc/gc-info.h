// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef INCLUDE_CPPGC_GC_INFO_H_
#define INCLUDE_CPPGC_GC_INFO_H_

#include <stdint.h>

#include "include/cppgc/finalizer-trait.h"
#include "include/v8config.h"

namespace cppgc {
namespace internal {

using GCInfoIndex = uint16_t;

class V8_EXPORT RegisteredGCInfoIndex final {
 public:
  RegisteredGCInfoIndex(FinalizationCallback finalization_callback,
                        bool has_v_table);
  GCInfoIndex GetIndex() const { return index_; }

 private:
  const GCInfoIndex index_;
};

// Trait determines how the garbage collector treats objects wrt. to traversing,
// finalization, and naming.
template <typename T>
struct GCInfoTrait {
  static GCInfoIndex Index() {
    static_assert(sizeof(T), "T must be fully defined");
    static const RegisteredGCInfoIndex registered_index(
        FinalizerTrait<T>::kCallback, std::is_polymorphic<T>::value);
    return registered_index.GetIndex();
  }
};

}  // namespace internal
}  // namespace cppgc

#endif  // INCLUDE_CPPGC_GC_INFO_H_
