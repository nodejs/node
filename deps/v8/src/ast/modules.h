// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_AST_MODULES_H_
#define V8_AST_MODULES_H_

#include "src/zone.h"

namespace v8 {
namespace internal {


class AstRawString;


class ModuleDescriptor : public ZoneObject {
 public:
  // ---------------------------------------------------------------------------
  // Factory methods.

  static ModuleDescriptor* New(Zone* zone) {
    return new (zone) ModuleDescriptor(zone);
  }

  // ---------------------------------------------------------------------------
  // Mutators.

  // Add a name to the list of exports. If it already exists, that's an error.
  void AddLocalExport(const AstRawString* export_name,
                      const AstRawString* local_name, Zone* zone, bool* ok);

  // Add module_specifier to the list of requested modules,
  // if not already present.
  void AddModuleRequest(const AstRawString* module_specifier, Zone* zone);

  // Assign an index.
  void Allocate(int index) {
    DCHECK_EQ(-1, index_);
    index_ = index;
  }

  // ---------------------------------------------------------------------------
  // Accessors.

  int Length() {
    ZoneHashMap* exports = exports_;
    return exports ? exports->occupancy() : 0;
  }

  // The context slot in the hosting script context pointing to this module.
  int Index() {
    return index_;
  }

  const AstRawString* LookupLocalExport(const AstRawString* export_name,
                                        Zone* zone);

  const ZoneList<const AstRawString*>& requested_modules() const {
    return requested_modules_;
  }

  // ---------------------------------------------------------------------------
  // Iterators.

  // Use like:
  //   for (auto it = descriptor->iterator(); !it.done(); it.Advance()) {
  //     ... it.name() ...
  //   }
  class Iterator {
   public:
    bool done() const { return entry_ == NULL; }
    const AstRawString* export_name() const {
      DCHECK(!done());
      return static_cast<const AstRawString*>(entry_->key);
    }
    const AstRawString* local_name() const {
      DCHECK(!done());
      return static_cast<const AstRawString*>(entry_->value);
    }
    void Advance() { entry_ = exports_->Next(entry_); }

   private:
    friend class ModuleDescriptor;
    explicit Iterator(const ZoneHashMap* exports)
        : exports_(exports), entry_(exports ? exports->Start() : NULL) {}

    const ZoneHashMap* exports_;
    ZoneHashMap::Entry* entry_;
  };

  Iterator iterator() const { return Iterator(this->exports_); }

  // ---------------------------------------------------------------------------
  // Implementation.
 private:
  explicit ModuleDescriptor(Zone* zone)
      : exports_(NULL), requested_modules_(1, zone), index_(-1) {}

  ZoneHashMap* exports_;   // Module exports and their types (allocated lazily)
  ZoneList<const AstRawString*> requested_modules_;
  int index_;
};

}  // namespace internal
}  // namespace v8

#endif  // V8_AST_MODULES_H_
