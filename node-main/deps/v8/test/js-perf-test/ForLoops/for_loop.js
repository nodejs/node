// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

new BenchmarkSuite('Let-Standard', [1000], [
  new Benchmark('Let-Standard', false, false, 0, LetLoop),
]);

new BenchmarkSuite('Var-Standard', [1000], [
  new Benchmark('Var-Standard', false, false, 0, VarLoop),
]);

var x = [-1, 1, 4];
var y = [-11, -1, 1, 2, 3, 4, 5, 6, 20, 44, 87, 99, 100];

function LetLoop() {
  "use strict";
  const ret = [];
  for (let i = 0; i < x.length; i++) {
    for (let z = 0; z < y.length; z++) {
      if (x[i] == y[z]) {
        ret.push(x[i]);
        break;
      }
    }
  }
  return ret;
}

function VarLoop() {
  "use strict";
  const ret = [];
  for (var i = 0; i < x.length; i++) {
    for (var z = 0; z < y.length; z++) {
      if (x[i] == y[z]) {
        ret.push(x[i]);
        break;
      }
    }
  }
  return ret;
}
