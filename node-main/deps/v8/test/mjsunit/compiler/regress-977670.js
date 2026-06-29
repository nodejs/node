// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  var i;
  for (i in 'xxxxxxxx') {
    try { throw 42 } catch (e) {}
  }
  print(i);
  i['' + 'length'] = 42;
}

%PrepareFunctionForOptimization(foo);
foo();
foo();
foo();
%OptimizeFunctionOnNextCall(foo);
foo();
