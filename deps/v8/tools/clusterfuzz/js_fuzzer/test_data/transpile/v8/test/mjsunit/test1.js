// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Input for testing the Babel transpile option for classes from ES6->ES5.
// This attempts to cover all features for which an own Babel transformation
// plugin exists, like private properties, computed methods, static blocks,
// getters, etc. Also ensure this can be combined with V8's native syntax.

class A {
  a = 1;
  b = 2;
  foo() {
    throw 42;
  }
  bar() {
    return 42;
  }
}

class B extends A {
  #privateryan = "huh";
  c = 3;
  baz() {
    %OptimizeFunctionOnNextCall(dontchokeonthis);
    return 42;
  }
  [Symbol.dispose]() {
    return 0;
  }
  get 0() {return 1}
  get [0]() {return 1}
  get #ryan() {return this.#privateryan}
  static meh() {}
  static { console.log(42); }
}

const a = new A();
const b = new B();
