// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --ignition --verify-heap --expose-gc

// Tests that verify heap works for BytecodeArrays in the large object space.

// Creates a list of variable declarations and calls it through eval to
// generate a large BytecodeArray.
var s = "";
for (var i = 0; i < 65535; i++) {
  s += ("var a" + i + ";");
}

(function() { eval(s); })();
gc();
