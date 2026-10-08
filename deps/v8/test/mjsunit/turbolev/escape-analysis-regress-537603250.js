// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --turbolev-escape-analysis --turbolev

function opaque() {}
%NeverOptimizeFunction(opaque);

class MyClass extends Uint16Array { }
const glob_obj = new MyClass();

function bar(a, b) {
  opaque(new Uint8Array([ 1, 2, 3, 4 ]));

  b.a = a;

  return {};
}

function foo() {
  for (let i = 0; i < 5; i++) {
    for (let j = 0; j < 5; j++) {
      %OptimizeOsr();
    }


    bar(glob_obj, bar(3.5, glob_obj));
  }
}

%PrepareFunctionForOptimization(MyClass);
%PrepareFunctionForOptimization(bar);
%PrepareFunctionForOptimization(foo);
foo();
