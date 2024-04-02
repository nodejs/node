// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Include heap-inl.h to make sure that neither it nor its transitive includes
// pull in windows.h.
#include "src/heap/heap-inl.h"

// This should be after all other #includes.
#if defined(_WINDOWS_)  // Detect whether windows.h was included.
// See base/win/windows_h_disallowed.h for details.
#error Windows.h was included unexpectedly.
#endif  // defined(_WINDOWS_)
