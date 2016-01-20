// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

"use strict";

class B {
  foo() { return 23 }
}

class C extends B {
  bar() { return super[%DeoptimizeFunction(C.prototype.bar), "foo"]() }
}

assertEquals(23, new C().bar());
assertEquals(23, new C().bar());
%OptimizeFunctionOnNextCall(C.prototype.bar);
assertEquals(23, new C().bar());
