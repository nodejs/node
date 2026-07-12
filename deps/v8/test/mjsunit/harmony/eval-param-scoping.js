// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function f0(a = eval("var b"), b) { }
assertThrows(f0, SyntaxError);

function f1(a = eval("var b = 0"), b) { }
assertThrows(f1, SyntaxError);

function f2(a = eval("function b(){}"), b) { }
assertThrows(f2, SyntaxError);

function f3(a = eval("{ function b(){} }"), b) { return b }
assertEquals(undefined, f3());

function f4(b, a = eval("var b = 0")) { return b }
assertThrows(f4, SyntaxError);

function f5(b, a = eval("function b(){}")) { return b }
assertThrows(f5, SyntaxError);

function f6(b, a = eval("{ function b(){} }")) { return b }
assertEquals(42, f6(42));
