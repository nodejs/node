// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef V8_ROOTS_STATIC_ROOTS_H_
#define V8_ROOTS_STATIC_ROOTS_H_

#if V8_STATIC_ROOTS_BOOL

#ifdef V8_ENABLE_WEBASSEMBLY
#ifdef V8_INTL_SUPPORT
#include "src/roots/static-roots-intl-wasm.h"  // IWYU pragma: export
#else
#include "src/roots/static-roots-nointl-wasm.h"  // IWYU pragma: export
#endif
#else
#ifdef V8_INTL_SUPPORT
#include "src/roots/static-roots-intl-nowasm.h"  // IWYU pragma: export
#else
#include "src/roots/static-roots-nointl-nowasm.h"  // IWYU pragma: export
#endif
#endif

#endif  // V8_STATIC_ROOTS_BOOL
#endif  // V8_ROOTS_STATIC_ROOTS_H_
