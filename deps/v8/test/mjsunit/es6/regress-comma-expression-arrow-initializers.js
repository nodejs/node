// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let b = 0;
([1], [b] = [42]);
assertEquals(42, b);

for (let i = ([1], [b] = [99]); false; ) {}
assertEquals(99, b);

assertThrows(
    () => eval("for (let i = ([{a = 42}.f()], [b] = [1]); false; ) {}"),
    SyntaxError,
    "Invalid shorthand property initializer");
