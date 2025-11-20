// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --force-slow-path --deopt-every-n-times=500

const v0 = [9007199254740991,268435441,5228,-33279,65536,-4294967296,19163,-2147483647,33451];
function f1(a2) {
    return { e: a2, 536870888: a2 };
}
f1();
f1(f1(f1));
f1();
v0.sort(f1);
