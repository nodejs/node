// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

var DefaultConstructorBenchmark = new BenchmarkSuite('DefaultConstructor',
    [100], [
      new Benchmark('NoSuperClass', false, false, 0, NoSuperClass),
      new Benchmark('WithSuperClass', false, false, 0, WithSuperClass),
      new Benchmark('WithSuperClassArguments', false, false, 0,
                    WithSuperClassArguments),
    ]);


class BaseClass {}


class DerivedClass extends BaseClass {}


function NoSuperClass() {
  return new BaseClass();
}


function WithSuperClass() {
  return new DerivedClass();
}


function WithSuperClassArguments() {
  return new DerivedClass(0, 1, 2, 3, 4);
}
