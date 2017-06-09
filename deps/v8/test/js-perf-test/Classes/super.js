// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

var SuperBenchmark = new BenchmarkSuite('Super', [100], [
     new Benchmark('SuperMethodCall', false, false, 0, SuperMethodCall),
     new Benchmark('SuperGetterCall', false, false, 0, SuperGetterCall),
     new Benchmark('SuperSetterCall', false, false, 0, SuperSetterCall),
]);


class Base {
  constructor() {}
  get x() {
    return this._x++;
  }
  set x(v) {
    this._x += v;
    return this._x;
  }
  f() {
    return this._x++;
  }
}


class Derived extends Base {
  constructor() {
    super();
    this._x = 1;
  }
  SuperCall() {
    return super.f();
  }
  GetterCall() {
    return super.x;
  }
  SetterCall() {
    return super.x = 5;
  }
}


var derived = new Derived();

function SuperMethodCall() {
  return derived.SuperCall();
}

function SuperGetterCall() {
  return derived.GetterCall();
}

function SuperSetterCall() {
  return derived.SetterCall();
}
