// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function f() {
  try {
    throw 1;
  } catch (e) {
    var a = 0;
    var b = 0;
    var c = 0;
    var x = 1;
    var result = eval('eval("x")').toString();
    assertEquals("1", result);
  }
  var x = 2;
  var result = eval('eval("x")').toString();
  assertEquals("2", result);
})();
