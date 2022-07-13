// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --opt --allow-natives-syntax

(() => {
  function f(u) {
    for (var j = 0; j < 20; ++j) {
      print('' + u.codePointAt());
    }
  };
  %PrepareFunctionForOptimization(f);
  f("test");
  f("foo");
  %OptimizeFunctionOnNextCall(f);
  f("");
})();
