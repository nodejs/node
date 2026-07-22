// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class C {}
class D extends C { constructor() { super(...unresolved, 75) } }
D.__proto__ = null;

%PrepareFunctionForOptimization(D);
// ReferenceError because argument evaluation happens before calling the super
// constructor.
assertThrows(() => new D(), ReferenceError);
assertThrows(() => new D(), ReferenceError);
%OptimizeFunctionOnNextCall(D);
assertThrows(() => new D(), ReferenceError);
