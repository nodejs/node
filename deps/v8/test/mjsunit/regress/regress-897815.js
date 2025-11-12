// Copyright 2018 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

(function __f_19350() {
  function __f_19351() {
    function __f_19352() {
    }
  };
  %PrepareFunctionForOptimization(__f_19351);
  try {
    __f_19350();
  } catch (e) {
  }
  %OptimizeFunctionOnNextCall(__f_19351);
})();
