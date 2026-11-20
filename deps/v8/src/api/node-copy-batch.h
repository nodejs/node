// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_SRC_API_NODE_COPY_BATCH_H_
#define V8_SRC_API_NODE_COPY_BATCH_H_

#include <cstddef>
#include <memory>

#include "../../include/v8-local-handle.h"
#include "../../include/v8config.h"

namespace v8 {
class Array;
class BackingStore;
class Isolate;

namespace internal {
class NodeCopyStoreRecycler {
 public:
  virtual ~NodeCopyStoreRecycler() = default;
  virtual void Return(std::unique_ptr<v8::BackingStore> store) = 0;
};

#if V8_HAS_ATTRIBUTE_VISIBILITY && !defined(V8_OS_WIN)
__attribute__((visibility("hidden")))
#endif
Local<v8::Array> NewNodeCopyBatch(
    v8::Isolate* isolate, std::unique_ptr<v8::BackingStore>* stores,
    unsigned count, size_t size,
    const std::shared_ptr<NodeCopyStoreRecycler>& recycler);
}  // namespace internal
}  // namespace v8

#endif  // V8_SRC_API_NODE_COPY_BATCH_H_
