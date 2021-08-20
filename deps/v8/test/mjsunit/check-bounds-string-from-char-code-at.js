// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax --opt --no-always-opt
// Flags: --no-stress-flush-bytecode

let string = "foobar";

function f(useArrayIndex) {
  let index = useArrayIndex ? '1': '4294967296';
  return string.charCodeAt(index);
}

%PrepareFunctionForOptimization(f);
f(true);
f(true);

%OptimizeFunctionOnNextCall(f);
f(false);
assertUnoptimized(f);

%PrepareFunctionForOptimization(f);
f(true);
f(true);

%OptimizeFunctionOnNextCall(f);
f(true);

// no deopt here
f(false);
assertOptimized(f);
