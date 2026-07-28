// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo() {
  try {
    (function in_foo() {
      var x = {};
      var f = foo();
      f();
      f();
    })();
  } catch (e) {}
}
foo();
foo();
