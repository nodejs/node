// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --always-opt --allow-natives-syntax

try {
} catch(e) {; }
(function __f_12() {
})();
(function __f_6() {
  function __f_3() {
  }
  function __f_4() {
    try {
    } catch (e) {
    }
  }
 __f_4();
  %OptimizeFunctionOnNextCall(__f_4);
 __f_4();
})();
