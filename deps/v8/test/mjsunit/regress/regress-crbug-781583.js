// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function* generator(a) {
  a.pop().next();
}

function prepareGenerators(n) {
  var a = [{ next: () => 0 }];
  for (var i = 0; i < n; ++i) {
    a.push(generator(a));
  }
  return a;
}

var gens1 = prepareGenerators(10);
assertDoesNotThrow(() => gens1.pop().next());

%OptimizeFunctionOnNextCall(generator);

var gens2 = prepareGenerators(200000);
assertThrows(() => gens2.pop().next(), RangeError);
