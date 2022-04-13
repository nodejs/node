// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-opt

var __v_3 = {};
function __f_0() {
  var __v_30 = -0;
  __v_30.__defineGetter__("0", function() { return undefined; });
  __v_30 = 0;
  __v_3 = 0;
  assertTrue(Object.is(0, __v_30));
}
__f_0();
