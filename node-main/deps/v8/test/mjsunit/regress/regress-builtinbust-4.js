// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var o = { __proto__:Array.prototype, 0:"x" };
function boomer() { return 0; }
Object.defineProperty(o, "length", { get:boomer, set:boomer });
Object.seal(o);

assertDoesNotThrow(function() { o.push(1); });
assertEquals(0, o.length);
assertEquals(1, o[0]);

assertDoesNotThrow(function() { o.unshift(2); });
assertEquals(0, o.length);
assertEquals(2, o[0]);
