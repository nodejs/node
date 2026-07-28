// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-maglev-loop-peeling --turbolev --turbolev-escape-analysis

function MyObj() {
  this.x = 17;
}

function test(cond, n) {
  let o = new MyObj();
  let o_back = new MyObj();

  o.x = 19;
  switch (cond) {
    case 1:
      o.x = 23;
      break;
    case 2:
      o.x = 27;
      break;
  }
  let i = 0;
  while (i < n) {
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
assertEquals(test(1, 1), new MyObj());
