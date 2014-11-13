// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
'use strict';

var SuperBenchmark = new BenchmarkSuite('Super', [100], [
     new Benchmark('SuperMethodCall', false, false, 0, SuperMethodCall),
     new Benchmark('SuperGetterCall', false, false, 0, SuperGetterCall),
     new Benchmark('SuperSetterCall', false, false, 0, SuperSetterCall),
]);


function Base() { }
Base.prototype = {
  constructor: Base,
  get x() {
    return this._x++;
  },
  set x(v) {
    this._x += v;
    return this._x;
  }
}

Base.prototype.f = function() {
  return this._x++;
}.toMethod(Base.prototype);

function Derived() {
  this._x = 1;
}
Derived.prototype = Object.create(Base.prototype);
Object.setPrototypeOf(Derived, Base);

Derived.prototype.SuperCall = function() {
  return super.f();
}.toMethod(Derived.prototype);

Derived.prototype.GetterCall = function() {
  return super.x;
}.toMethod(Derived.prototype);

Derived.prototype.SetterCall = function() {
  return super.x = 5;
}.toMethod(Derived.prototype);

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
