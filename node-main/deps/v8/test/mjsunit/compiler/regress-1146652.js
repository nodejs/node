// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function IsDataView(obj) {
  return obj.getFloat64;
}
%NeverOptimizeFunction(IsDataView);

function bar(obj) {
  if (IsDataView(obj)) obj.getFloat64(0);
}

%PrepareFunctionForOptimization(bar);
bar(new DataView(new ArrayBuffer(42)));

const proxy = new Proxy({}, {});
function foo() { bar(proxy) }

%PrepareFunctionForOptimization(foo);
foo();

%OptimizeFunctionOnNextCall(foo);
foo();
