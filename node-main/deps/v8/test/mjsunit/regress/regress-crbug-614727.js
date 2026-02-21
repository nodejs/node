// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

function f(a, b, c) { return arguments }
function g(...args) { return args }

// On 64-bit machine this produces a 768K array which is sufficiently small to
// not cause a stack overflow, but big enough to move the allocated arguments
// object into large object space (kMaxRegularHeapObjectSize == 600K).
var length = Math.pow(2, 15) * 3;
var args = new Array(length);
assertEquals(length, f.apply(null, args).length);
assertEquals(length, g.apply(null, args).length);

// On 32-bit machines this produces an equally sized array, however it might in
// turn trigger a stack overflow on 64-bit machines, which we need to catch.
var length = Math.pow(2, 16) * 3;
var args = new Array(length);
try { f.apply(null, args) } catch(e) {}
try { g.apply(null, args) } catch(e) {}
