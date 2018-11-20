// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:  --allow-natives-syntax --deopt-every-n-times=1

function __f_0() {
  var x = [];
  x[21] = 1;
  x[21] + 0;
}

for (var i = 0; i < 3; i++) __f_0();
%OptimizeFunctionOnNextCall(__f_0);
for (var i = 0; i < 10; i++) __f_0();
%OptimizeFunctionOnNextCall(__f_0);
__f_0();
%GetScript("foo");
