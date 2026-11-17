// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_BUILTINS_BYTECODES_BUILTINS_LIST_H_
#define V8_BUILTINS_BYTECODES_BUILTINS_LIST_H_

// Includes the bytecode builtins list: the checked-in copy under src/gen/ if
// v8_use_checked_in_generated_headers, the generated header otherwise.
// Include this header, not either of those; src/DEPS enforces that. See
// src/gen/README.md.
#ifdef V8_USE_CHECKED_IN_GENERATED_HEADERS
#include "src/gen/intl-wasm/bytecodes-builtins-list.h"  // IWYU pragma: export
#else
#include "builtins-generated/bytecodes-builtins-list.h"  // IWYU pragma: export, nogncheck
#endif

#endif  // V8_BUILTINS_BYTECODES_BUILTINS_LIST_H_
