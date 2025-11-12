// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --no-lazy-feedback-allocation
// Flags: --invocation-count-for-turbofan=1

function f() {
  var obj = Object.freeze({});
  %_CreateDataProperty(obj, "foo", "bar");
}

// Should not crash
assertThrows(f, TypeError);
assertThrows(f, TypeError);
