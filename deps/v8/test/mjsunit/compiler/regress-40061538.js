// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function forin (a)
{
  for (var c in a)
  {
    a["b"] = a.b;
    return a[c];
  }
}

let a = {a: 1, b: 2};
let b = {a: 1, b: 2};
let c = {a: 1.1, b: 2};

%PrepareFunctionForOptimization(forin);
%OptimizeFunctionOnNextCall(forin);
forin(a);

%OptimizeFunctionOnNextCall(forin);
forin(a);

var x = forin(b);
assertEquals(1, x);
b["a"] = 1.2;
assertEquals(1, x);
