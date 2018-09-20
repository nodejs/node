// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

class Base {}
class Derived extends Base {
  constructor() { super(); }
}
var proxy = new Proxy(Base, { get() {} });
assertDoesNotThrow(() => Reflect.construct(Derived, []));
assertThrows(() => Reflect.construct(Derived, [], proxy), TypeError);
assertThrows(() => Reflect.construct(Derived, [], proxy), TypeError);
%OptimizeFunctionOnNextCall(Derived);
assertThrows(() => Reflect.construct(Derived, [], proxy), TypeError);
