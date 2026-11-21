// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_NODE_COPY_EXTENSION_BATCH_H_
#define V8_OBJECTS_NODE_COPY_EXTENSION_BATCH_H_

#include "src/objects/js-array-buffer.h"

namespace v8::internal {

struct NodeCopyExtensionGroup;

class NodeCopyExtensionBatch final {
 public:
  NodeCopyExtensionBatch() = default;
  ~NodeCopyExtensionBatch();
  NodeCopyExtensionBatch(const NodeCopyExtensionBatch&) = delete;
  NodeCopyExtensionBatch& operator=(const NodeCopyExtensionBatch&) = delete;

  ArrayBufferExtension* Install(
      Tagged<JSArrayBuffer> shell, Isolate* isolate,
      std::unique_ptr<v8::BackingStore> store,
      const std::shared_ptr<NodeCopyStoreRecycler>& recycler,
      const DisallowGarbageCollection& no_gc, unsigned remaining);

 private:
  NodeCopyExtensionGroup* group_ = nullptr;
  unsigned used_ = 0;
  bool allocation_failed_ = false;
};

}  // namespace v8::internal

#endif  // V8_OBJECTS_NODE_COPY_EXTENSION_BATCH_H_
