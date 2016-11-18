// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-async-await

try {
} catch(e) {; }
function __f_7(expected, run) {
  var __v_10 = run();
};
__f_7("[1,2,3]", () => (function() {
      return (async () => {[...await arguments] })();
      })());
