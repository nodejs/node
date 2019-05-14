// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --always-opt

function f() {
  var obj = Object.freeze({});
  %_CreateDataProperty(obj, "foo", "bar");
}

// Should not crash
assertThrows(f, TypeError);
