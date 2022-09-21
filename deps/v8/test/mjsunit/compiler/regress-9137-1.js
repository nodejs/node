// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbofan
// Flags: --no-flush-bytecode --no-stress-flush-code

function changeMap(obj) {
  obj.blub = 42;
}

function foo(obj) {
  return obj.bind(changeMap(obj));
}

%NeverOptimizeFunction(changeMap);
%PrepareFunctionForOptimization(foo);
foo(function(){});
foo(function(){});
%OptimizeFunctionOnNextCall(foo);
foo(function(){});
%PrepareFunctionForOptimization(foo);
%OptimizeFunctionOnNextCall(foo);
foo(function(){});
assertOptimized(foo);
