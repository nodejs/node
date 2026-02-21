// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

var DefaultConstructorBenchmark = new BenchmarkSuite('LeafConstructors',
    [100], [
      new Benchmark('WithExplicitArguments', false, false, 0,
                    WithExplicitArguments),
    ]);


const Point = class Point {
  constructor(x, y, z) { this.x = x; this.y = y; this.z = z; }
}

const klasses = [
  class A extends Point { constructor(x, y, z) { super(x, y, z); } },
  class B extends Point { constructor(x, y, z) { super(x, y, z); } },
  class C extends Point { constructor(x, y, z) { super(x, y, z); } },
  class D extends Point { constructor(x, y, z) { super(x, y, z); } },
  class E extends Point { constructor(x, y, z) { super(x, y, z); } }
];

function WithExplicitArguments() {
  let result = null;
  for (const klass of klasses) {
    result = new klass(0, 1, 2);
  }
  return result;
};
