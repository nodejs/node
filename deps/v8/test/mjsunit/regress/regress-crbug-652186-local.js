// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan

function f() {
  var x = 1;
  return eval("eval('var x = 2'); x;");
}
f();
