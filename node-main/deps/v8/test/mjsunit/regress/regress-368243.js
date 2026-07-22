// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo(a, c){
  for(var f in c) {
    if ("object" === typeof c[f]) {
      a[f] = c[f];
      foo(a[f], c[f]);
    }
  }
};
%PrepareFunctionForOptimization(foo);

c = {
  "one" : { x : 1},
  "two" : { x : 2},
  "thr" : { x : 3, z : 4},
};

foo({}, c);
foo({}, c);
%OptimizeFunctionOnNextCall(foo);
foo({}, c);
