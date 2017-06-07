// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbo --turbo-escape

var mp = Object.getPrototypeOf(0);

function getRandomProperty(v) {
  var properties;
  if (mp) { properties = Object.getOwnPropertyNames(mp); }
  if (properties.includes("constructor") && v.constructor.hasOwnProperty()) {; }
  if (properties.length == 0) { return "0"; }
  return properties[NaN];
}

var c = 0;

function f() {
  c++;
  if (c === 3) %OptimizeFunctionOnNextCall(f);
  if (c > 4) throw 42;
  for (var x of ["x"]) {
    getRandomProperty(0) ;
    f();
    %_DeoptimizeNow();
  }
}

assertThrowsEquals(f, 42);
