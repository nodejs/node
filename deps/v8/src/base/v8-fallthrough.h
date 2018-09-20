// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BASE_V8_FALLTHROUGH_H_
#define V8_BASE_V8_FALLTHROUGH_H_

// When clang suggests inserting [[clang::fallthrough]], it first checks if
// it knows of a macro expanding to it, and if so suggests inserting the
// macro.  This means that this macro must be used only in code internal
// to v8, so that v8's user code doesn't end up getting suggestions
// for V8_FALLTHROUGH instead of the user-specific fallthrough macro.
// So do not include this header in any of v8's public headers -- only
// use it in src/, not in include/.
#if defined(__clang__)
#define V8_FALLTHROUGH [[clang::fallthrough]]  // NOLINT(whitespace/braces)
#else
#define V8_FALLTHROUGH
#endif

#endif  // V8_BASE_V8_FALLTHROUGH_H_
