// Copyright 2021 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  const a = {};
  const b = [];
  const unused = {__proto__: [], p1: a, p2: 0, p3: 0, p4: 0};

  function inline(x) { x.gaga; }

  inline(a);
  inline(b);

  b.p1 = 42;
}


%PrepareFunctionForOptimization(foo);
for (var i = 0; i < 10; i++) foo();
%OptimizeFunctionOnNextCall(foo);
foo();
