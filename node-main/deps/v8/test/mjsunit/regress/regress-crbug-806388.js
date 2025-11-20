// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --enable-slow-asserts --expose-gc

class Derived extends Array {
    constructor(a) { throw "error" }
}

// Derived is not a subclass of RegExp
let o = Reflect.construct(RegExp, [], Derived);
o.lastIndex = 0x1234;
%HeapObjectVerify(o);

gc();
%HeapObjectVerify(o);
