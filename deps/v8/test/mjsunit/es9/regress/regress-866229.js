// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var obj = { length: 1, 0: "spread" };
obj[Symbol.toStringTag] = "foo";
obj[Symbol.hasInstance] = function() { return true; }
obj[Symbol.isConcatSpreadable] = true;

var obj2 = { ...obj };

// Crash if fast result map bitfield is not set correctly, if verifying heap
%HeapObjectVerify(obj2);

// Ensure correct result for some well-known symbols
assertEquals("[object foo]", Object.prototype.toString.call(obj2));
assertTrue(Uint8Array instanceof obj2);
assertEquals(["spread"], [].concat(obj2));
