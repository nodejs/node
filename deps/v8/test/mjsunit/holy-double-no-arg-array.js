// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  function f() {
    return new Array();
  }
  var a = f();
  a[0] = 0.5;
  var b = f();
  b[2] = 0.5;
  assertEquals(undefined, b[0]);
})();
