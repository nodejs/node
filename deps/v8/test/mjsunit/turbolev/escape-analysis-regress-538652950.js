// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev
// Flags: --turbolev-escape-analysis

function MyObj() {
  this.x = 17;
}

function test(cond, n) {
  let o = new MyObj();
  o.x = 19;
  let i = 0;
  switch (cond) {
    case 1:
      o.x = 23;
      break;
    case 2:
      o.x = 27;
      break;
  }
  while (i < n) {
    let o_back = new MyObj();
    o.x = o_back;
    i++;
  }
  return o.x;
}

%PrepareFunctionForOptimization(MyObj);
%PrepareFunctionForOptimization(test);
test(1, 1);
test(2, 1);
%OptimizeFunctionOnNextCall(test);
test(1, 1);
