// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

assertTrue(%TypedArrayLengthProtector());

let a = new Uint16Array(20);
assertEquals(20, a.length);

assertSame(a.__proto__.__proto__.__proto__, Object.prototype);

// This doesn't invalidate a.length, because a.__proto__.__proto__ already
// has the "length" property.
Object.defineProperty(Object.prototype, "length",
                      { get() { return 42; } });

assertTrue(%TypedArrayLengthProtector());

assertEquals(20, a.length);
