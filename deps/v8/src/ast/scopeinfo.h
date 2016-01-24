// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_SCOPEINFO_H_
#define V8_AST_SCOPEINFO_H_

#include "src/allocation.h"
#include "src/ast/modules.h"
#include "src/ast/variables.h"

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
      values_[i] = kNotFound;
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




//---------------------------------------------------------------------------
// Auxiliary class used for the description of module instances.
// Used by Runtime_DeclareModules.

class ModuleInfo: public FixedArray {
 public:
  static ModuleInfo* cast(Object* description) {
    return static_cast<ModuleInfo*>(FixedArray::cast(description));
  }

  static Handle<ModuleInfo> Create(Isolate* isolate,
                                   ModuleDescriptor* descriptor, Scope* scope);

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


}  // namespace internal
}  // namespace v8

#endif  // V8_AST_SCOPEINFO_H_
