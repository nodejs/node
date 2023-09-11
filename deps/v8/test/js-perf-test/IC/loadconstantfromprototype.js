// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('LoadConstantFromPrototype', [1000], [
  new Benchmark('LoadConstantFromPrototype', false, false, 0, LoadConstantFromPrototype)
]);

function Foo() {};

Foo.prototype.bar = {};
Foo.prototype.covfefe = function() {};
Foo.prototype.baz = 1;

function LoadConstantFromPrototype() {
  let foo = new Foo();

  for (let i = 0; i < 1000; ++i) {
    foo.bar;
    foo.covfefe;
    foo.baz;
  }
}
