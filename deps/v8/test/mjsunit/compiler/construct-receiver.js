// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class C {
  bla() {};
  constructor() { %TurbofanStaticAssert(this.bla === bla); }
}
const bla = C.prototype.bla;
function bar(f) { return new f; }
var CC = C;
function foo() { return bar(CC); }

%PrepareFunctionForOptimization(C);
%PrepareFunctionForOptimization(bla);
%PrepareFunctionForOptimization(foo);
%PrepareFunctionForOptimization(bar);

// Make {this.bla} in C megamorphic
new class extends C { constructor() { super(); this.a = 1 } }
new class extends C { constructor() { super(); this.b = 1 } }
new class extends C { constructor() { super(); this.c = 1 } }
new class extends C { constructor() { super(); this.d = 1 } }

foo();
%OptimizeFunctionOnNextCall(foo);
foo();
