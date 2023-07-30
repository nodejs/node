// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "include/cppgc/internal/member-storage.h"

#include "include/cppgc/garbage-collected.h"
#include "include/cppgc/member.h"
#include "src/base/compiler-specific.h"
#include "src/base/macros.h"

namespace cppgc {
namespace internal {

#if defined(CPPGC_POINTER_COMPRESSION)
alignas(api_constants::kCachelineSize) CageBaseGlobal::Base
    CageBaseGlobal::g_base_ = {CageBaseGlobal::kLowerHalfWordMask};
#endif  // defined(CPPGC_POINTER_COMPRESSION)

// Debugging helpers.

#if defined(CPPGC_POINTER_COMPRESSION)
extern "C" V8_DONT_STRIP_SYMBOL V8_EXPORT_PRIVATE void*
_cppgc_internal_Decompress_Compressed_Pointer(uint32_t cmprsd) {
  return CompressedPointer::Decompress(cmprsd);
}
#endif  // !defined(CPPGC_POINTER_COMPRESSION)

class MemberDebugHelper final {
 public:
  static void* Uncompress(MemberBase<DefaultMemberStorage>* m) {
    return const_cast<void*>(m->GetRaw());
  }
};

extern "C" V8_DONT_STRIP_SYMBOL V8_EXPORT_PRIVATE void*
_cppgc_internal_Uncompress_Member(void* m) {
  return MemberDebugHelper::Uncompress(
      static_cast<MemberBase<DefaultMemberStorage>*>(m));
}

}  // namespace internal
}  // namespace cppgc
