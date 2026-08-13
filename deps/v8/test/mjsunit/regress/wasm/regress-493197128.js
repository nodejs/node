// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

d8.file.execute("test/mjsunit/mjsunit.js");

// We should not accidentally truncate the page delta to 32 bits and succeed
// with a small memory growth.
const mem = new WebAssembly.Memory({ initial: 0n, address: "i64" });
const expected = "WebAssembly.Memory.grow(): Unable to grow instance memory";
assertThrows(() => mem.grow(4294967295n), RangeError, expected);
assertThrows(() => mem.grow(4294967296n), RangeError, expected);
assertThrows(() => mem.grow(4294967297n), RangeError, expected);
