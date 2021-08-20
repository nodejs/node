// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_LOCAL_FACTORY_H_
#define V8_HEAP_LOCAL_FACTORY_H_

#include <map>
#include <vector>

#include "src/base/logging.h"
#include "src/common/globals.h"
#include "src/handles/handles.h"
#include "src/heap/factory-base.h"
#include "src/heap/heap.h"
#include "src/heap/read-only-heap.h"
#include "src/heap/spaces.h"
#include "src/objects/heap-object.h"
#include "src/objects/map.h"
#include "src/objects/objects.h"
#include "src/objects/shared-function-info.h"
#include "src/roots/roots.h"

namespace v8 {
namespace internal {

class AstValueFactory;
class AstRawString;
class AstConsString;
class LocalIsolate;

class V8_EXPORT_PRIVATE LocalFactory : public FactoryBase<LocalFactory> {
 public:
  explicit LocalFactory(Isolate* isolate);

  ReadOnlyRoots read_only_roots() const { return roots_; }

#define ROOT_ACCESSOR(Type, name, CamelName) inline Handle<Type> name();
  READ_ONLY_ROOT_LIST(ROOT_ACCESSOR)
  // AccessorInfos appear mutable, but they're actually not mutated once they
  // finish initializing. In particular, the root accessors are not mutated and
  // are safe to access (as long as the off-thread job doesn't try to mutate
  // them).
  ACCESSOR_INFO_ROOT_LIST(ROOT_ACCESSOR)
#undef ROOT_ACCESSOR

  // The parser shouldn't allow the LocalFactory to get into a state where
  // it generates errors.
  Handle<Object> NewInvalidStringLengthError() { UNREACHABLE(); }
  Handle<Object> NewRangeError(MessageTemplate template_index) {
    UNREACHABLE();
  }

 private:
  friend class FactoryBase<LocalFactory>;

  // ------
  // Customization points for FactoryBase.
  HeapObject AllocateRaw(int size, AllocationType allocation,
                         AllocationAlignment alignment = kWordAligned);

  LocalIsolate* isolate() {
    // Downcast to the privately inherited sub-class using c-style casts to
    // avoid undefined behavior (as static_cast cannot cast across private
    // bases).
    // NOLINTNEXTLINE (google-readability-casting)
    return (LocalIsolate*)this;  // NOLINT(readability/casting)
  }
  inline bool CanAllocateInReadOnlySpace() { return false; }
  inline bool EmptyStringRootIsInitialized() { return true; }
  // ------

  void AddToScriptList(Handle<Script> shared);
  // ------

  ReadOnlyRoots roots_;
#ifdef DEBUG
  bool a_script_was_added_to_the_script_list_ = false;
#endif
};

}  // namespace internal
}  // namespace v8

#endif  // V8_HEAP_LOCAL_FACTORY_H_
