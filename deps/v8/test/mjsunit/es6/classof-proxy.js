// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

function test_function(o) {
  if (%_ClassOf(o) === "Function") {
    return true;
  } else {
    return false;
  }
}

var non_callable = new Proxy({}, {});
var callable = new Proxy(function(){}.__proto__, {});
var constructable = new Proxy(function(){}, {});

assertFalse(test_function(non_callable));
assertTrue(test_function(callable));
assertTrue(test_function(constructable));

%OptimizeFunctionOnNextCall(test_function);

assertFalse(test_function(non_callable));
assertTrue(test_function(callable));
assertTrue(test_function(constructable));
