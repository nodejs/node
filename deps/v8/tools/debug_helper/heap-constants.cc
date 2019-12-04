// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "heap-constants.h"
#include "src/common/globals.h"

namespace d = v8::debug_helper;

namespace v8_debug_helper_internal {

std::string FindKnownObject(uintptr_t address, const d::Roots& roots) {
  uintptr_t containing_page = address & ~i::kPageAlignmentMask;
  uintptr_t offset_in_page = address & i::kPageAlignmentMask;

  // If there's a match with a known root, then search only that page.
  if (containing_page == roots.map_space) {
    return FindKnownObjectInMapSpace(offset_in_page);
  }
  if (containing_page == roots.old_space) {
    return FindKnownObjectInOldSpace(offset_in_page);
  }
  if (containing_page == roots.read_only_space) {
    return FindKnownObjectInReadOnlySpace(offset_in_page);
  }

  // For any unknown roots, compile a list of things this object might be.
  std::string result;
  if (roots.map_space == 0) {
    std::string sub_result = FindKnownObjectInMapSpace(offset_in_page);
    if (!sub_result.empty()) {
      result += "maybe " + sub_result;
    }
  }
  if (roots.old_space == 0) {
    std::string sub_result = FindKnownObjectInOldSpace(offset_in_page);
    if (!sub_result.empty()) {
      result = (result.empty() ? "" : result + ", ") + "maybe " + sub_result;
    }
  }
  if (roots.read_only_space == 0) {
    std::string sub_result = FindKnownObjectInReadOnlySpace(offset_in_page);
    if (!sub_result.empty()) {
      result = (result.empty() ? "" : result + ", ") + "maybe " + sub_result;
    }
  }

  return result;
}

}  // namespace v8_debug_helper_internal
