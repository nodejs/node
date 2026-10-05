// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --cache=code --compile-only

import { helper } from './d8-code-cache-module-dep.mjs';

export const x = 1;

// If this module is executed, it will throw and fail the test.
throw new Error("Module should only be compiled, not executed");
