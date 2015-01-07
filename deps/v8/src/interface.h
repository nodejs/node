// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_INTERFACE_H_
#define V8_INTERFACE_H_

#include "src/ast-value-factory.h"
#include "src/zone-inl.h"  // For operator new.

namespace v8 {
namespace internal {


// This class implements the following abstract grammar of interfaces
// (i.e. module types):
//   interface ::= UNDETERMINED | VALUE | CONST | MODULE(exports)
//   exports ::= {name : interface, ...}
// A frozen type is one that is fully determined. Unification does not
// allow to turn non-const values into const, or adding additional exports to
// frozen interfaces. Otherwise, unifying modules merges their exports.
// Undetermined types are unification variables that can be unified freely.
// There is a natural subsort lattice that reflects the increase of knowledge:
//
//            undetermined
//           //     |    \\                                                    .
//       value  (frozen)  module
//      //   \\  /    \  //
//  const   fr.value  fr.module
//      \\    /
//     fr.const
//
// where the bold lines are the only transitions allowed.

class Interface : public ZoneObject {
 public:
  // ---------------------------------------------------------------------------
  // Factory methods.

  static Interface* NewUnknown(Zone* zone) {
    return new(zone) Interface(NONE);
  }

  static Interface* NewValue();

  static Interface* NewConst();

  static Interface* NewModule(Zone* zone) {
    return new(zone) Interface(MODULE);
  }

  // ---------------------------------------------------------------------------
  // Mutators.

  // Add a name to the list of exports. If it already exists, unify with
  // interface, otherwise insert unless this is closed.
  void Add(const AstRawString* name, Interface* interface, Zone* zone,
           bool* ok) {
    DoAdd(name, name->hash(), interface, zone, ok);
  }

  // Unify with another interface. If successful, both interface objects will
  // represent the same type, and changes to one are reflected in the other.
  void Unify(Interface* that, Zone* zone, bool* ok);

  // Determine this interface to be a value interface.
  void MakeValue(bool* ok) {
    *ok = !IsModule();
    if (*ok) Chase()->flags_ |= VALUE;
  }

  // Determine this interface to be an immutable interface.
  void MakeConst(bool* ok) {
    *ok = !IsModule() && (IsConst() || !IsFrozen());
    if (*ok) Chase()->flags_ |= VALUE + CONST;
  }

  // Determine this interface to be a module interface.
  void MakeModule(bool* ok) {
    *ok = !IsValue();
    if (*ok) Chase()->flags_ |= MODULE;
  }

  // Do not allow any further refinements, directly or through unification.
  void Freeze(bool* ok) {
    *ok = IsValue() || IsModule();
    if (*ok) Chase()->flags_ |= FROZEN;
  }

  // Assign an index.
  void Allocate(int index) {
    DCHECK(IsModule() && IsFrozen() && Chase()->index_ == -1);
    Chase()->index_ = index;
  }

  // ---------------------------------------------------------------------------
  // Accessors.

  // Check whether this is still a fully undetermined type.
  bool IsUnknown() { return Chase()->flags_ == NONE; }

  // Check whether this is a value type.
  bool IsValue() { return Chase()->flags_ & VALUE; }

  // Check whether this is a constant type.
  bool IsConst() { return Chase()->flags_ & CONST; }

  // Check whether this is a module type.
  bool IsModule() { return Chase()->flags_ & MODULE; }

  // Check whether this is closed (i.e. fully determined).
  bool IsFrozen() { return Chase()->flags_ & FROZEN; }

  bool IsUnified(Interface* that) {
    return Chase() == that->Chase()
        || (this->IsValue() == that->IsValue() &&
            this->IsConst() == that->IsConst());
  }

  int Length() {
    DCHECK(IsModule() && IsFrozen());
    ZoneHashMap* exports = Chase()->exports_;
    return exports ? exports->occupancy() : 0;
  }

  // The context slot in the hosting script context pointing to this module.
  int Index() {
    DCHECK(IsModule() && IsFrozen());
    return Chase()->index_;
  }

  // Look up an exported name. Returns NULL if not (yet) defined.
  Interface* Lookup(Handle<String> name, Zone* zone);

  // ---------------------------------------------------------------------------
  // Iterators.

  // Use like:
  //   for (auto it = interface->iterator(); !it.done(); it.Advance()) {
  //     ... it.name() ... it.interface() ...
  //   }
  class Iterator {
   public:
    bool done() const { return entry_ == NULL; }
    const AstRawString* name() const {
      DCHECK(!done());
      return static_cast<const AstRawString*>(entry_->key);
    }
    Interface* interface() const {
      DCHECK(!done());
      return static_cast<Interface*>(entry_->value);
    }
    void Advance() { entry_ = exports_->Next(entry_); }

   private:
    friend class Interface;
    explicit Iterator(const ZoneHashMap* exports)
        : exports_(exports), entry_(exports ? exports->Start() : NULL) {}

    const ZoneHashMap* exports_;
    ZoneHashMap::Entry* entry_;
  };

  Iterator iterator() const { return Iterator(this->exports_); }

  // ---------------------------------------------------------------------------
  // Debugging.
#ifdef DEBUG
  void Print(int n = 0);  // n = indentation; n < 0 => don't print recursively
#endif

  // ---------------------------------------------------------------------------
  // Implementation.
 private:
  struct Cache;

  enum Flags {    // All flags are monotonic
    NONE = 0,
    VALUE = 1,    // This type describes a value
    CONST = 2,    // This type describes a constant
    MODULE = 4,   // This type describes a module
    FROZEN = 8    // This type is fully determined
  };

  int flags_;
  Interface* forward_;     // Unification link
  ZoneHashMap* exports_;   // Module exports and their types (allocated lazily)
  int index_;

  explicit Interface(int flags)
    : flags_(flags),
      forward_(NULL),
      exports_(NULL),
      index_(-1) {
#ifdef DEBUG
    if (FLAG_print_interface_details)
      PrintF("# Creating %p\n", static_cast<void*>(this));
#endif
  }

  Interface* Chase() {
    Interface* result = this;
    while (result->forward_ != NULL) result = result->forward_;
    if (result != this) forward_ = result;  // On-the-fly path compression.
    return result;
  }

  void DoAdd(const void* name, uint32_t hash, Interface* interface, Zone* zone,
             bool* ok);
  void DoUnify(Interface* that, bool* ok, Zone* zone);
};

} }  // namespace v8::internal

#endif  // V8_INTERFACE_H_
