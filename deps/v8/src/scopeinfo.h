// Copyright 2011 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef V8_SCOPEINFO_H_
#define V8_SCOPEINFO_H_

#include "allocation.h"
#include "variables.h"
#include "zone-inl.h"

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
  int Lookup(Object* data,
             String* name,
             VariableMode* mode,
             InitializationFlag* init_flag);

  // Update an element in the cache.
  void Update(Object* data,
              String* name,
              VariableMode mode,
              InitializationFlag init_flag,
              int slot_index);

  // Clear the cache.
  void Clear();

  static const int kNotFound = -2;

 private:
  ContextSlotCache() {
    for (int i = 0; i < kLength; ++i) {
      keys_[i].data = NULL;
      keys_[i].name = NULL;
      values_[i] = kNotFound;
    }
  }

  inline static int Hash(Object* data, String* name);

#ifdef DEBUG
  void ValidateEntry(Object* data,
                     String* name,
                     VariableMode mode,
                     InitializationFlag init_flag,
                     int slot_index);
#endif

  static const int kLength = 256;
  struct Key {
    Object* data;
    String* name;
  };

  struct Value {
    Value(VariableMode mode,
          InitializationFlag init_flag,
          int index) {
      ASSERT(ModeField::is_valid(mode));
      ASSERT(InitField::is_valid(init_flag));
      ASSERT(IndexField::is_valid(index));
      value_ = ModeField::encode(mode) |
          IndexField::encode(index) |
          InitField::encode(init_flag);
      ASSERT(mode == this->mode());
      ASSERT(init_flag == this->initialization_flag());
      ASSERT(index == this->index());
    }

    explicit inline Value(uint32_t value) : value_(value) {}

    uint32_t raw() { return value_; }

    VariableMode mode() { return ModeField::decode(value_); }

    InitializationFlag initialization_flag() {
      return InitField::decode(value_);
    }

    int index() { return IndexField::decode(value_); }

    // Bit fields in value_ (type, shift, size). Must be public so the
    // constants can be embedded in generated code.
    class ModeField:  public BitField<VariableMode,       0, 3> {};
    class InitField:  public BitField<InitializationFlag, 3, 1> {};
    class IndexField: public BitField<int,                4, 32-4> {};

   private:
    uint32_t value_;
  };

  Key keys_[kLength];
  uint32_t values_[kLength];

  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(ContextSlotCache);
};


} }  // namespace v8::internal

#endif  // V8_SCOPEINFO_H_
