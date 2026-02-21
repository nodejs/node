// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --enable-inspector --allow-natives-syntax

const Debug = debug.Debug;
Debug.setListener(() => {});

var a = [0,1,2,3,4,5,6,7,8,9];

var f = function() {
  a.forEach(function(v) {
    try {
      throw new Error();
    } catch (e) {
    }
  });
};

%PrepareFunctionForOptimization(f);
f();
f();
%OptimizeFunctionOnNextCall(f);
f();
f();
