// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var test = function() {
  var t = Date.now();  // Just any non-constant double value.
  var o = {
    ['p']: 1,
    t
  };
};

function caller() {
  test();
}
caller();
caller();
%OptimizeFunctionOnNextCall(caller);
caller();
