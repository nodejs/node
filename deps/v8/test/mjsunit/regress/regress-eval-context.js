// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

(function() {
  'use strict';
  var x = 0;

  {
    let x = 1;
    assertEquals(1, eval("x"));
  }

  {
    let y = 2;
    assertEquals(0, eval("x"));
  }

  assertEquals(0, eval("x"));
})();
