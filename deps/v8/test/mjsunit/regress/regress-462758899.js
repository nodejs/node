// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --allow-natives-syntax

function bar(do_throw) {
  if (do_throw) {
    throw 'lol';
  }
}

let arr = [1.5, undefined];

function foo(index, do_throw) {
    let undef = arr[index];
    try {
      bar(do_throw);
      undef = 42;
      bar();
    } catch {
      return undef;
    }
}

%PrepareFunctionForOptimization(foo);
foo(1, false);
foo(1, true);
%OptimizeMaglevOnNextCall(foo);
assertEquals(undefined, foo(1, true));
