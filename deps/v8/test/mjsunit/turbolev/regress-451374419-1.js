// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling

class Baz {
  set #p(x) {
  }
}

class Bar extends Baz {}
class Foo extends Bar {
  constructor(...x) {
    super(1, ...x, 2);
  }
};

%PrepareFunctionForOptimization(Foo);
new Foo();

%OptimizeFunctionOnNextCall(Foo);
new Foo();
