// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function f1(h_also_eval) {
  this.x = h_also_eval;
}

function f2(h, h_eval) {
  var o = new f1(h());
  // During the last call to f3 with g2 as an argument, this store is
  // bi-morphic, including a version that refers to the old map (before
  // the replacement of f1's prototype). As a result, during load elimination
  // we see two stores with incompatible representations: One in the
  // constructor, and one in the impossible branch of the bi-morphic store
  // site.
  o.x = h_eval;
};
function f3(h) {
  %PrepareFunctionForOptimization(f2);
  f2(h, h());
  %OptimizeFunctionOnNextCall(f2);
  f2(h, h());
}

function g1() {
  return {};
};
function g2() {
  return 4.2;
};

f3(g1);
f3(g2);

f3(g1);
f1.prototype = {};
f3(g2);
