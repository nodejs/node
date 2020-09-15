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

// This class serves as a container of data that should persist across all
// (optimizing) compiler runs in an isolate. For now it stores serialized data
// for various common objects such as builtins, so that these objects don't have
// to be serialized in each compilation job. See JSHeapBroker::InitializeRefsMap
// for details.
class PerIsolateCompilerCache : public ZoneObject {
 public:
  explicit PerIsolateCompilerCache(Zone* zone)
      : zone_(zone), refs_snapshot_(nullptr) {}

  bool HasSnapshot() const { return refs_snapshot_ != nullptr; }
  RefsMap* GetSnapshot() {
    DCHECK(HasSnapshot());
    return refs_snapshot_;
  }
  void SetSnapshot(RefsMap* refs) {
    DCHECK(!HasSnapshot());
    DCHECK(!refs->IsEmpty());
    refs_snapshot_ = zone_->New<RefsMap>(refs, zone_);
    DCHECK(HasSnapshot());
  }

  Zone* zone() const { return zone_; }

  static void Setup(Isolate* isolate) {
    if (isolate->compiler_cache() == nullptr) {
      Zone* zone = new Zone(isolate->allocator(), "Compiler zone");
      PerIsolateCompilerCache* cache = zone->New<PerIsolateCompilerCache>(zone);
      isolate->set_compiler_utils(cache, zone);
    }
    DCHECK_NOT_NULL(isolate->compiler_cache());
  }

 private:
  Zone* const zone_;
  RefsMap* refs_snapshot_;
};

}  // namespace compiler
}  // namespace internal
}  // namespace v8

#endif  // V8_COMPILER_PER_ISOLATE_COMPILER_CACHE_H_
