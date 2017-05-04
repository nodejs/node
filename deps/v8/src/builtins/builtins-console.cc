// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/builtins/builtins-utils.h"
#include "src/builtins/builtins.h"

#include "src/debug/interface-types.h"
#include "src/objects-inl.h"

namespace v8 {
namespace internal {

// -----------------------------------------------------------------------------
// Console

#define CONSOLE_METHOD_LIST(V) \
  V(Debug)                     \
  V(Error)                     \
  V(Info)                      \
  V(Log)                       \
  V(Warn)                      \
  V(Dir)                       \
  V(DirXml)                    \
  V(Table)                     \
  V(Trace)                     \
  V(Group)                     \
  V(GroupCollapsed)            \
  V(GroupEnd)                  \
  V(Clear)                     \
  V(Count)                     \
  V(Assert)                    \
  V(MarkTimeline)              \
  V(Profile)                   \
  V(ProfileEnd)                \
  V(Timeline)                  \
  V(TimelineEnd)               \
  V(Time)                      \
  V(TimeEnd)                   \
  V(TimeStamp)

#define CONSOLE_BUILTIN_IMPLEMENTATION(name)      \
  BUILTIN(Console##name) {                        \
    HandleScope scope(isolate);                   \
    if (isolate->console_delegate()) {            \
      debug::ConsoleCallArguments wrapper(args);  \
      isolate->console_delegate()->name(wrapper); \
      CHECK(!isolate->has_pending_exception());   \
      CHECK(!isolate->has_scheduled_exception()); \
    }                                             \
    return isolate->heap()->undefined_value();    \
  }
CONSOLE_METHOD_LIST(CONSOLE_BUILTIN_IMPLEMENTATION)
#undef CONSOLE_BUILTIN_IMPLEMENTATION

#undef CONSOLE_METHOD_LIST

}  // namespace internal
}  // namespace v8
