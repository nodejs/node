// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_EXECUTION_EMBEDDER_STATE_H_
#define V8_EXECUTION_EMBEDDER_STATE_H_

#include "include/v8-local-handle.h"
#include "src/execution/isolate.h"

namespace v8 {

enum class EmbedderStateTag : uint8_t;

namespace internal {
class V8_EXPORT_PRIVATE EmbedderState {
 public:
  EmbedderState(v8::Isolate* isolate, Local<v8::Context> context,
                EmbedderStateTag tag);

  ~EmbedderState();

  EmbedderStateTag GetState() const { return tag_; }

  Address native_context_address() const { return native_context_address_; }

  void OnMoveEvent(Address from, Address to);

 private:
  Isolate* isolate_;
  EmbedderStateTag tag_;
  Address native_context_address_ = kNullAddress;
  EmbedderState* previous_embedder_state_;
};
}  // namespace internal

}  // namespace v8

#endif  // V8_EXECUTION_EMBEDDER_STATE_H_
