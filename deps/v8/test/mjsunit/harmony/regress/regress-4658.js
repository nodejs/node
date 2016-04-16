// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-do-expressions

(function testWithSimpleLoopVariable() {
  var f = (x, y = (do { var s=0; for (var e of x) s += e; s; })) => y*(y+1);
  var result = f([1,2,3]);  // core dump here, if not fixed.
  assertEquals(result, 42);
})();

(function testWithComplexLoopVariable() {
  var f = (x, i=x[0]-1, a=[],
           y = (do { var s=0;
                     for (a[i] of x) s += a[i++];
                     s;
                   })) => y*(a[0]+a[1]*a[2]);
  var result = f([1,2,3]);  // core dump here, if not fixed.
  assertEquals(result, 42);
})();
