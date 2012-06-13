// Copyright 2012 the V8 project authors. All rights reserved.
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

#ifndef V8_INTERFACE_H_
#define V8_INTERFACE_H_

#include "zone-inl.h"  // For operator new.

namespace v8 {
namespace internal {


// This class implements the following abstract grammar of interfaces
// (i.e. module types):
//   interface ::= UNDETERMINED | VALUE | MODULE(exports)
//   exports ::= {name : interface, ...}
// A frozen module type is one that is fully determined. Unification does not
// allow adding additional exports to frozen interfaces.
// Otherwise, unifying modules merges their exports.
// Undetermined types are unification variables that can be unified freely.

class Interface : public ZoneObject {
 public:
  // ---------------------------------------------------------------------------
  // Factory methods.

  static Interface* NewValue() {
    static Interface value_interface(VALUE + FROZEN);  // Cached.
    return &value_interface;
  }

  static Interface* NewUnknown(Zone* zone) {
    return new(zone) Interface(NONE);
  }

  static Interface* NewModule(Zone* zone) {
    return new(zone) Interface(MODULE);
  }

  // ---------------------------------------------------------------------------
  // Mutators.

  // Add a name to the list of exports. If it already exists, unify with
  // interface, otherwise insert unless this is closed.
  void Add(Handle<String> name, Interface* interface, Zone* zone, bool* ok) {
    DoAdd(name.location(), name->Hash(), interface, zone, ok);
  }

  // Unify with another interface. If successful, both interface objects will
  // represent the same type, and changes to one are reflected in the other.
  void Unify(Interface* that, Zone* zone, bool* ok);

  // Determine this interface to be a value interface.
  void MakeValue(bool* ok) {
    *ok = !IsModule();
    if (*ok) Chase()->flags_ |= VALUE;
  }

  // Determine this interface to be a module interface.
  void MakeModule(bool* ok) {
    *ok = !IsValue();
    if (*ok) Chase()->flags_ |= MODULE;
  }

  // Set associated instance object.
  void MakeSingleton(Handle<JSModule> instance, bool* ok) {
    *ok = IsModule() && Chase()->instance_.is_null();
    if (*ok) Chase()->instance_ = instance;
  }

  // Do not allow any further refinements, directly or through unification.
  void Freeze(bool* ok) {
    *ok = IsValue() || IsModule();
    if (*ok) Chase()->flags_ |= FROZEN;
  }

  // ---------------------------------------------------------------------------
  // Accessors.

  // Check whether this is still a fully undetermined type.
  bool IsUnknown() { return Chase()->flags_ == NONE; }

  // Check whether this is a value type.
  bool IsValue() { return Chase()->flags_ & VALUE; }

  // Check whether this is a module type.
  bool IsModule() { return Chase()->flags_ & MODULE; }

  // Check whether this is closed (i.e. fully determined).
  bool IsFrozen() { return Chase()->flags_ & FROZEN; }

  Handle<JSModule> Instance() { return Chase()->instance_; }

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
    Handle<String> name() const {
      ASSERT(!done());
      return Handle<String>(*static_cast<String**>(entry_->key));
    }
    Interface* interface() const {
      ASSERT(!done());
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
  enum Flags {    // All flags are monotonic
    NONE = 0,
    VALUE = 1,    // This type describes a value
    MODULE = 2,   // This type describes a module
    FROZEN = 4    // This type is fully determined
  };

  int flags_;
  Interface* forward_;     // Unification link
  ZoneHashMap* exports_;   // Module exports and their types (allocated lazily)
  Handle<JSModule> instance_;

  explicit Interface(int flags)
    : flags_(flags),
      forward_(NULL),
      exports_(NULL) {
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

  void DoAdd(void* name, uint32_t hash, Interface* interface, Zone* zone,
             bool* ok);
  void DoUnify(Interface* that, bool* ok, Zone* zone);
};

} }  // namespace v8::internal

#endif  // V8_INTERFACE_H_
