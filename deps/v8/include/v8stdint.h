// Copyright 2012 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Load definitions of standard types.

#ifndef V8STDINT_H_
#define V8STDINT_H_

#include <stddef.h>
#include <stdio.h>

#include "v8config.h"

#if V8_OS_WIN && !V8_CC_MINGW

typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef short int16_t;  // NOLINT
typedef unsigned short uint16_t;  // NOLINT
typedef int int32_t;
typedef unsigned int uint32_t;
typedef __int64 int64_t;
typedef unsigned __int64 uint64_t;
// intptr_t and friends are defined in crtdefs.h through stdio.h.

#else

#include <stdint.h>  // NOLINT

#endif

#endif  // V8STDINT_H_
