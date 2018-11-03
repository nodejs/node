// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class C extends Object {
  constructor() {
    try { super(); } catch (e) { };
    return 1;
  }
}

class A extends C {
  constructor() {
    super();
    throw new Error();
    return { get: () => this };
  }
}

var D = new Proxy(A, { get() { %DeoptimizeFunction(A); } });

try { Reflect.construct(A, [], D); } catch(e) {}
%OptimizeFunctionOnNextCall(A);
try { Reflect.construct(A, [], D); } catch(e) {}
