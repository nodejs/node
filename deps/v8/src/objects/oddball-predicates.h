// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_OBJECTS_ODDBALL_PREDICATES_H_
#define V8_OBJECTS_ODDBALL_PREDICATES_H_

#include "src/common/globals.h"
#include "src/objects/object-list-macros.h"
#include "src/objects/tagged.h"

namespace v8::internal {

class EarlyReadOnlyRoots;
class Object;

#define IS_TYPE_FUNCTION_DECL(Type, ...)                                 \
  V8_INLINE bool Is##Type(Tagged<Object> obj, EarlyReadOnlyRoots roots); \
  V8_INLINE bool Is##Type(Tagged<Object> obj);

ODDBALL_LIST(IS_TYPE_FUNCTION_DECL)
HOLE_LIST(IS_TYPE_FUNCTION_DECL)
IS_TYPE_FUNCTION_DECL(UndefinedContextCell)
IS_TYPE_FUNCTION_DECL(NullOrUndefined)
#undef IS_TYPE_FUNCTION_DECL

}  // namespace v8::internal

#endif  // V8_OBJECTS_ODDBALL_PREDICATES_H_
