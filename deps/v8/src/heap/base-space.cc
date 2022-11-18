// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/heap/base-space.h"

namespace v8 {
namespace internal {

const char* BaseSpace::GetSpaceName(AllocationSpace space) {
  switch (space) {
    case NEW_SPACE:
      return "new_space";
    case OLD_SPACE:
      return "old_space";
    case MAP_SPACE:
      return "map_space";
    case CODE_SPACE:
      return "code_space";
    case SHARED_SPACE:
      return "shared_space";
    case LO_SPACE:
      return "large_object_space";
    case NEW_LO_SPACE:
      return "new_large_object_space";
    case CODE_LO_SPACE:
      return "code_large_object_space";
    case SHARED_LO_SPACE:
      return "shared_large_object_space";
    case RO_SPACE:
      return "read_only_space";
  }
  UNREACHABLE();
}

}  // namespace internal
}  // namespace v8
