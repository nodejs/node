// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/weak-object-worklists.h"

#include "src/heap/heap-inl.h"
#include "src/heap/heap-layout-inl.h"
#include "src/heap/heap.h"
#include "src/objects/hash-table.h"
#include "src/objects/heap-object.h"
#include "src/objects/js-function.h"
#include "src/objects/js-weak-refs-inl.h"
#include "src/objects/js-weak-refs.h"
#include "src/objects/shared-function-info.h"
#include "src/objects/transitions.h"

namespace v8 {

namespace internal {

WeakObjects::Local::Local(WeakObjects* weak_objects)
    : WeakObjects::UnusedBase()
#define INIT_LOCAL_WORKLIST(_, name, __) , name##_local(weak_objects->name)
          WEAK_OBJECT_WORKLISTS(INIT_LOCAL_WORKLIST)
#undef INIT_LOCAL_WORKLIST
{
}

void WeakObjects::Local::Publish() {
#define INVOKE_PUBLISH(_, name, __) name##_local.Publish();
  WEAK_OBJECT_WORKLISTS(INVOKE_PUBLISH)
#undef INVOKE_PUBLISH
}

void WeakObjects::Clear() {
#define INVOKE_CLEAR(_, name, __) name.Clear();
  WEAK_OBJECT_WORKLISTS(INVOKE_CLEAR)
#undef INVOKE_CLEAR
}

}  // namespace internal
}  // namespace v8
