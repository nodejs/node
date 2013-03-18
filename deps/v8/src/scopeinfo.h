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
    class ModeField:  public BitField<VariableMode,       0, 4> {};
    class InitField:  public BitField<InitializationFlag, 4, 1> {};
    class IndexField: public BitField<int,                5, 32-5> {};

   private:
    uint32_t value_;
  };

  Key keys_[kLength];
  uint32_t values_[kLength];

  friend class Isolate;
  DISALLOW_COPY_AND_ASSIGN(ContextSlotCache);
};




//---------------------------------------------------------------------------
// Auxiliary class used for the description of module instances.
// Used by Runtime_DeclareModules.

class ModuleInfo: public FixedArray {
 public:
  static ModuleInfo* cast(Object* description) {
    return static_cast<ModuleInfo*>(FixedArray::cast(description));
  }

  static Handle<ModuleInfo> Create(
      Isolate* isolate, Interface* interface, Scope* scope);

  // Index of module's context in host context.
  int host_index() { return Smi::cast(get(HOST_OFFSET))->value(); }

  // Name, mode, and index of the i-th export, respectively.
  // For value exports, the index is the slot of the value in the module
  // context, for exported modules it is the slot index of the
  // referred module's context in the host context.
  // TODO(rossberg): This format cannot yet handle exports of modules declared
  // in earlier scripts.
  String* name(int i) { return String::cast(get(name_offset(i))); }
  VariableMode mode(int i) {
    return static_cast<VariableMode>(Smi::cast(get(mode_offset(i)))->value());
  }
  int index(int i) { return Smi::cast(get(index_offset(i)))->value(); }

  int length() { return (FixedArray::length() - HEADER_SIZE) / ITEM_SIZE; }

 private:
  // The internal format is: Index, (Name, VariableMode, Index)*
  enum {
    HOST_OFFSET,
    NAME_OFFSET,
    MODE_OFFSET,
    INDEX_OFFSET,
    HEADER_SIZE = NAME_OFFSET,
    ITEM_SIZE = INDEX_OFFSET - NAME_OFFSET + 1
  };
  inline int name_offset(int i) { return NAME_OFFSET + i * ITEM_SIZE; }
  inline int mode_offset(int i) { return MODE_OFFSET + i * ITEM_SIZE; }
  inline int index_offset(int i) { return INDEX_OFFSET + i * ITEM_SIZE; }

  static Handle<ModuleInfo> Allocate(Isolate* isolate, int length) {
    return Handle<ModuleInfo>::cast(
        isolate->factory()->NewFixedArray(HEADER_SIZE + ITEM_SIZE * length));
  }
  void set_host_index(int index) { set(HOST_OFFSET, Smi::FromInt(index)); }
  void set_name(int i, String* name) { set(name_offset(i), name); }
  void set_mode(int i, VariableMode mode) {
    set(mode_offset(i), Smi::FromInt(mode));
  }
  void set_index(int i, int index) {
    set(index_offset(i), Smi::FromInt(index));
  }
};


} }  // namespace v8::internal

#endif  // V8_SCOPEINFO_H_
