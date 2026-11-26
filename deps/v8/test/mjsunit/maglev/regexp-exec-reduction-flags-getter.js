// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --turbolev

// RegExp.prototype.exec reads the flags off the regexp itself, so the reduced
// call ignores the accessors on RegExp.prototype as well.

const re = /a/;
function f(re, s) { return re.exec(s); }
%PrepareFunctionForOptimization(f);
f(re, "ba");
f(re, "zz");
%OptimizeFunctionOnNextCall(f);
f(re, "ba");
assertOptimized(f);

Object.defineProperty(RegExp.prototype, "global", { get() { return true; } });
re.lastIndex = 1;
assertEquals(1, f(re, "ba").index);
assertEquals(1, re.lastIndex);
assertNull(f(re, "zz"));
assertEquals(1, re.lastIndex);
