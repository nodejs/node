// Copyright 2011 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "src/char-predicates.h"

#ifdef V8_INTL_SUPPORT
#include "unicode/uchar.h"
#include "unicode/urename.h"
#endif  // V8_INTL_SUPPORT

namespace v8 {
namespace internal {

bool SupplementaryPlanes::IsIDStart(uc32 c) {
  DCHECK(c > 0xFFFF);
#ifdef V8_INTL_SUPPORT
  // This only works for code points in the SMPs, since ICU does not exclude
  // code points with properties 'Pattern_Syntax' or 'Pattern_White_Space'.
  // Code points in the SMP do not have those properties.
  return u_isIDStart(c);
#else
  // This is incorrect, but if we don't have ICU, use this as fallback.
  return false;
#endif  // V8_INTL_SUPPORT
}


bool SupplementaryPlanes::IsIDPart(uc32 c) {
  DCHECK(c > 0xFFFF);
#ifdef V8_INTL_SUPPORT
  // This only works for code points in the SMPs, since ICU does not exclude
  // code points with properties 'Pattern_Syntax' or 'Pattern_White_Space'.
  // Code points in the SMP do not have those properties.
  return u_isIDPart(c);
#else
  // This is incorrect, but if we don't have ICU, use this as fallback.
  return false;
#endif  // V8_INTL_SUPPORT
}
}  // namespace internal
}  // namespace v8
