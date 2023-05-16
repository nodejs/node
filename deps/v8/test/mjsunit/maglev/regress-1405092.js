// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --maglev

'use strict';

function foo(obj, ...args) {
  obj['apply'](...args);
}

var x = 0;

function bar() {
  try {
    this.x;
  } catch (e) {
    x++;
  }
}

%PrepareFunctionForOptimization(foo);
foo(bar);

%OptimizeMaglevOnNextCall(foo);
foo(bar);

assertEquals(2, x);
