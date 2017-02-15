// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function n(x,y){
  y = (y-(0x80000000|0)|0);
  return (x/y)|0;
};
var x = -0x80000000;
var y = 0x7fffffff;
n(x,y);
n(x,y);
%OptimizeFunctionOnNextCall(n);
assertEquals(x, n(x,y));
