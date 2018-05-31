// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_CONSTANTS_TABLE_BUILDER_H_
#define V8_BUILTINS_CONSTANTS_TABLE_BUILDER_H_

#include "src/allocation.h"
#include "src/base/macros.h"
#include "src/handles.h"
#include "src/identity-map.h"

namespace v8 {
namespace internal {

class Isolate;
class Object;

// Utility class to build the builtins constants table and store it on the root
// list. The constants table contains constants used by builtins, and is there
// to avoid directly embedding them into code objects, which would not be
// possible for off-heap (and thus immutable) code objects.
class BuiltinsConstantsTableBuilder final {
 public:
  explicit BuiltinsConstantsTableBuilder(Isolate* isolate);

  // Returns the index within the builtins constants table for the given
  // object, possibly adding the object to the table. Objects are deduplicated.
  uint32_t AddObject(Handle<Object> object);

  // Should be called after all affected code (e.g. builtins and bytecode
  // handlers) has been generated.
  void Finalize();

 private:
  Isolate* isolate_;

  // Maps objects to corresponding indices within the constants list.
  typedef IdentityMap<uint32_t, FreeStoreAllocationPolicy> ConstantsMap;
  ConstantsMap map_;

  DISALLOW_COPY_AND_ASSIGN(BuiltinsConstantsTableBuilder)
};

}  // namespace internal
}  // namespace v8

#endif  // V8_BUILTINS_CONSTANTS_TABLE_BUILDER_H_
