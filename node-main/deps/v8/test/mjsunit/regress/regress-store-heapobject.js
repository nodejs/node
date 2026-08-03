// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var o = {a: undefined};

function store(o, v) {
  o.a = v;
}

store(o, undefined);
store(o, undefined);

function f(bool) {
  var o = {a: undefined};
  if (bool) {
    store(o, 1);
  }
  return o;
};
%PrepareFunctionForOptimization(f);
f(false);
f(false);
%OptimizeFunctionOnNextCall(f);
f(true);
