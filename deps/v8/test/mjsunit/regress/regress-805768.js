// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function foo() {
  var a = [''];
  print(a[0]);
  return a;
}

function bar(a) { a[0] = "bazinga!"; }

for (var i = 0; i < 5; i++) bar([]);

%OptimizeFunctionOnNextCall(bar);
bar(foo());
assertEquals([''], foo());
