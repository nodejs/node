// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_HEAP_EPHEMERON_REMEMBERED_SET_H_
#define V8_HEAP_EPHEMERON_REMEMBERED_SET_H_

#if defined(_MSVC_STL_VERSION)
#include <map>
#else
#include <unordered_map>
#endif
#include <unordered_set>

#include "src/base/platform/mutex.h"
#include "src/heap/base/worklist.h"
#include "src/objects/hash-table.h"

namespace v8::internal {

// Stores ephemeron entries where the EphemeronHashTable is in old-space,
// and the key of the entry is in new-space. Such keys do not appear in the
// usual OLD_TO_NEW remembered set. The remembered set is used to avoid
// strongifying keys in such hash tables in young generation garbage
// collections.
// This remembered set is exclusively used for Minor GCs. Once we start
// incremental marking for a Full GC this remembered set is cleared. The markers
// and write barriers for the Full GC will record these slots in the regular
// OLD_TO_NEW remembered set then. In addition after a Full GC new space is
// empty because all live objects were promoted, so we have the guaranteee that
// there are no old-to-new refs anymore.
class EphemeronRememberedSet final {
 public:
  static constexpr int kEphemeronTableListSegmentSize = 128;
  using TableList = ::heap::base::Worklist<Tagged<EphemeronHashTable>,
                                           kEphemeronTableListSegmentSize>;

  using IndicesSet = std::unordered_set<int>;
#if defined(_MSVC_STL_VERSION)
  using TableMap = std::map<Tagged<EphemeronHashTable>, IndicesSet,
                            Object::Comparer>;
#else
  using TableMap = std::unordered_map<Tagged<EphemeronHashTable>, IndicesSet,
                                      Object::Hasher>;
#endif

  void RecordEphemeronKeyWrite(Tagged<EphemeronHashTable> table,
                               Address key_slot);
  void RecordEphemeronKeyWrites(Tagged<EphemeronHashTable> table,
                                IndicesSet indices);

  TableMap* tables() { return &tables_; }

 private:
  base::Mutex insertion_mutex_;
  TableMap tables_;
};

}  // namespace v8::internal

#endif  // V8_HEAP_EPHEMERON_REMEMBERED_SET_H_
