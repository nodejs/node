// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// The translation unit metagen parses. Its include closure is the set of class
// declarations the harvest can see, so it has to reach every heap object;
// src/objects/all-objects.h is maintained to do exactly that and PRESUBMIT.py
// checks it still does. The declaration-only aggregate avoids parsing inline
// implementations that depend on metagen's not-yet-generated output.
//
// A .cc rather than parsing the header directly: clang infers the input
// language from the extension, and a .h is a C header.

#include "src/objects/all-objects.h"
