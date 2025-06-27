// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turboshaft-enable-debug-features

var obj = Object.create(Object.create({blu: 42}));
obj.bla = 1;

function bar(o) {
  return o.blu;
}

function foo(o) {
  o.bla;
  %TurbofanStaticAssert(bar(o) == 42);
}

%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
bar({});
bar({bla: 1});
bar({blu: 1});
bar({blo: 1});
foo(obj);
%OptimizeFunctionOnNextCall(foo);
foo(obj);
