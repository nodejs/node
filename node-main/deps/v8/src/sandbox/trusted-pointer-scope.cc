// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/sandbox/trusted-pointer-scope.h"

#include "src/objects/heap-object-inl.h"

#ifdef V8_ENABLE_SANDBOX

namespace v8::internal {

TrustedPointerPublishingScope::TrustedPointerPublishingScope(
    Isolate* isolate, const DisallowJavascriptExecution& no_js)
    : isolate_(isolate) {
  // Nesting TrustedPointerPublishingScopes is not supported for now.
  DCHECK_NULL(isolate->trusted_pointer_publishing_scope());
  isolate->set_trusted_pointer_publishing_scope(this);
}

TrustedPointerPublishingScope::~TrustedPointerPublishingScope() {
  if (state_ == State::kFailure) {
    if (storage_ == Storage::kSingleton) {
      singleton_->OverwriteTag(kUnpublishedIndirectPointerTag);
    } else if (storage_ == Storage::kVector) {
      for (TrustedPointerTableEntry* entry : *vector_) {
        entry->OverwriteTag(kUnpublishedIndirectPointerTag);
      }
    }
  } else {
    // If this DCHECK fails, you probably forgot to call {MarkSuccess()}.
    DCHECK_EQ(state_, State::kSuccess);
  }
  if (storage_ == Storage::kVector) delete vector_;
  DCHECK_EQ(this, isolate_->trusted_pointer_publishing_scope());
  isolate_->set_trusted_pointer_publishing_scope(nullptr);
}

void TrustedPointerPublishingScope::TrackPointer(
    TrustedPointerTableEntry* entry) {
  if (storage_ == Storage::kEmpty) {
    singleton_ = entry;
    storage_ = Storage::kSingleton;
    return;
  }
  if (storage_ == Storage::kSingleton) {
    TrustedPointerTableEntry* previous = singleton_;
    vector_ = new std::vector<TrustedPointerTableEntry*>();
    vector_->reserve(4);
    vector_->push_back(previous);
    storage_ = Storage::kVector;
  }
  vector_->push_back(entry);
}

DisableTrustedPointerPublishingScope::DisableTrustedPointerPublishingScope(
    Isolate* isolate)
    : isolate_(isolate) {
  saved_ = isolate->trusted_pointer_publishing_scope();
  if (saved_) {
    isolate->set_trusted_pointer_publishing_scope(nullptr);
  }
}
DisableTrustedPointerPublishingScope::~DisableTrustedPointerPublishingScope() {
  if (saved_) {
    isolate_->set_trusted_pointer_publishing_scope(saved_);
  }
}

}  // namespace v8::internal

#endif  // V8_ENABLE_SANDBOX
