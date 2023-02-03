// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// Flags: --allow-natives-syntax
// Flags: --nouse-osr

function foo(obj) {
  var counter = 1;
  for (var i = 0; i < obj.length; i++) {
    %OptimizeOsr();
    %PrepareFunctionForOptimization(foo);
  }
  counter += obj;
  return counter;
}
%PrepareFunctionForOptimization(foo);

function bar() {
  var a = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12];
  for (var i = 0; i < 100; i++ ) {
    foo(a);
  }
}

try {
  bar();
} catch (e) {
}
