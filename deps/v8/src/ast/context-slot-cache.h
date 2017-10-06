// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_CONTEXT_SLOT_CACHE_H_
#define V8_AST_CONTEXT_SLOT_CACHE_H_

#include "src/allocation.h"
#include "src/ast/modules.h"

namespace v8 {
namespace internal {

// Cache for mapping (data, property name) into context slot index.
// The cache contains both positive and negative results.
// Slot index equals -1 means the property is absent.
// Cleared at startup and prior to mark sweep collection.
class ContextSlotCache {
 public:
  // Lookup context slot index for (data, name).
  // If absent, kNotFound is returned.
  int Lookup(Object* data, String* name, VariableMode* mode,
             InitializationFlag* init_flag,
             MaybeAssignedFlag* maybe_assigned_flag);

  // Update an element in the cache.
  void Update(Handle<Object> data, Handle<String> name, VariableMode mode,
              InitializationFlag init_flag,
              MaybeAssignedFlag maybe_assigned_flag, int slot_index);

  // Clear the cache.
  void Clear();

  static const int kNotFound = -2;

 private:
  ContextSlotCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].data = NULL;
      keys_[i].name = NULL;
      values_[i] = static_cast<uint32_t>(kNotFound);
    }
  }

  inline static int Hash(Object* data, String* name);

#ifdef DEBUG
  void ValidateEntry(Handle<Object> data, Handle<String> name,
                     VariableMode mode, InitializationFlag init_flag,
                     MaybeAssignedFlag maybe_assigned_flag, int slot_index);
#endif

  static const int kLength = 256;
  struct Key {
    Object* data;
    String* name;
  };

  struct Value {
    Value(VariableMode mode, InitializationFlag init_flag,
          MaybeAssignedFlag maybe_assigned_flag, int index) {
      DCHECK(ModeField::is_valid(mode));
      DCHECK(InitField::is_valid(init_flag));
      DCHECK(MaybeAssignedField::is_valid(maybe_assigned_flag));
      DCHECK(IndexField::is_valid(index));
      value_ = ModeField::encode(mode) | IndexField::encode(index) |
               InitField::encode(init_flag) |
               MaybeAssignedField::encode(maybe_assigned_flag);
      DCHECK(mode == this->mode());
      DCHECK(init_flag == this->initialization_flag());
      DCHECK(maybe_assigned_flag == this->maybe_assigned_flag());
      DCHECK(index == this->index());
    }

    explicit inline Value(uint32_t value) : value_(value) {}

    uint32_t raw() { return value_; }

    VariableMode mode() { return ModeField::decode(value_); }

    InitializationFlag initialization_flag() {
      return InitField::decode(value_);
    }

    MaybeAssignedFlag maybe_assigned_flag() {
      return MaybeAssignedField::decode(value_);
    }

    int index() { return IndexField::decode(value_); }

    // Bit fields in value_ (type, shift, size). Must be public so the
    // constants can be embedded in generated code.
    class ModeField : public BitField<VariableMode, 0, 4> {};
    class InitField : public BitField<InitializationFlag, 4, 1> {};
    class MaybeAssignedField : public BitField<MaybeAssignedFlag, 5, 1> {};
    class IndexField : public BitField<int, 6, 32 - 6> {};

   private:
    uint32_t value_;
  };

  Key keys_[kLength];
  uint32_t values_[kLength];

  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(ContextSlotCache);
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_CONTEXT_SLOT_CACHE_H_
