// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


Debug = debug.Debug;
Debug.setListener(function() {});

function f() {
  for (var i = 0; i < 100; i++) {
    %OptimizeOsr();
    %PrepareFunctionForOptimization(f);
  }
}
%PrepareFunctionForOptimization(f);

Debug.setBreakPoint(f, 0, 0);
f();
%PrepareFunctionForOptimization(f);
f();
Debug.setListener(null);
