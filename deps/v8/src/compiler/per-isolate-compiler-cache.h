// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_COMPILER_PER_ISOLATE_COMPILER_CACHE_H_
#define V8_COMPILER_PER_ISOLATE_COMPILER_CACHE_H_

#include "src/compiler/refs-map.h"
#include "src/execution/isolate.h"
#include "src/zone/zone-containers.h"

namespace v8 {
namespace internal {

class Isolate;
class Zone;

namespace compiler {

class ObjectData;

// This class serves as a per-isolate container of data that should be
// persisted between compiler runs. For now it stores the code builtins
// so they are not serialized on each compiler run.
class PerIsolateCompilerCache : public ZoneObject {
 public:
  explicit PerIsolateCompilerCache(Zone* zone)
      : zone_(zone), refs_snapshot_(nullptr) {}

  RefsMap* GetSnapshot() { return refs_snapshot_; }
  void SetSnapshot(RefsMap* refs) {
    DCHECK_NULL(refs_snapshot_);
    DCHECK(!refs->IsEmpty());
    refs_snapshot_ = new (zone_) RefsMap(refs, zone_);
  }

  bool HasSnapshot() const { return refs_snapshot_; }

  Zone* zone() const { return zone_; }

  static void Setup(Isolate* isolate) {
    if (isolate->compiler_cache()) return;

    // The following zone is supposed to contain compiler-related objects
    // that should live through all compilations, as opposed to the
    // broker_zone which holds per-compilation data. It's not meant for
    // per-compilation or heap broker data.
    Zone* compiler_zone = new Zone(isolate->allocator(), "Compiler zone");
    PerIsolateCompilerCache* compiler_cache =
        new (compiler_zone) PerIsolateCompilerCache(compiler_zone);
    isolate->set_compiler_utils(compiler_cache, compiler_zone);
  }

 private:
  Zone* const zone_;

  RefsMap* refs_snapshot_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PER_ISOLATE_COMPILER_CACHE_H_
