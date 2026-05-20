// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-turbo-loop-peeling

function foo(base) {
  class klass extends base {
    constructor() {
      try {
        undefined();
      } catch (e) {}
        super();
        this.d = 4.2;
        this.o = {};
    }
  }
  var __v_58 = new klass();
  var __v_59 = new klass();
}

%PrepareFunctionForOptimization(foo);
foo(Uint8Array);
foo(Uint8ClampedArray);
foo(Int16Array);
foo(Uint16Array);
foo(Int32Array);
foo(Uint32Array);
%OptimizeFunctionOnNextCall(foo);
assertThrows(foo);
