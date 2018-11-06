// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_DEBUG_DEBUG_PROPERTY_ITERATOR_H_
#define V8_DEBUG_DEBUG_PROPERTY_ITERATOR_H_

#include "src/debug/debug-interface.h"
#include "src/handles.h"
#include "src/isolate.h"
#include "src/prototype.h"

#include "include/v8.h"

namespace v8 {
namespace internal {

class JSReceiver;

class DebugPropertyIterator final : public debug::PropertyIterator {
 public:
  DebugPropertyIterator(Isolate* isolate, Handle<JSReceiver> receiver);
  ~DebugPropertyIterator() override = default;

  bool Done() const override;
  void Advance() override;

  v8::Local<v8::Name> name() const override;
  bool is_native_accessor() override;
  bool has_native_getter() override;
  bool has_native_setter() override;
  v8::Maybe<v8::PropertyAttribute> attributes() override;
  v8::Maybe<v8::debug::PropertyDescriptor> descriptor() override;

  bool is_own() override;
  bool is_array_index() override;

 private:
  void FillKeysForCurrentPrototypeAndStage();
  bool should_move_to_next_stage() const;
  void CalculateNativeAccessorFlags();
  Handle<Name> raw_name() const;

  Isolate* isolate_;
  PrototypeIterator prototype_iterator_;
  enum Stage { kExoticIndices = 0, kEnumerableStrings = 1, kAllProperties = 2 };
  Stage stage_ = kExoticIndices;

  uint32_t current_key_index_ = 0;
  Handle<FixedArray> keys_;
  uint32_t exotic_length_ = 0;

  bool calculated_native_accessor_flags_ = false;
  int native_accessor_flags_ = 0;
  bool is_own_ = true;

  DISALLOW_COPY_AND_ASSIGN(DebugPropertyIterator);
};
}  // namespace internal
}  // namespace v8

#endif  // V8_DEBUG_DEBUG_PROPERTY_ITERATOR_H_
