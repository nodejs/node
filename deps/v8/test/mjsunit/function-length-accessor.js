// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --harmony-scoping

function foo(a, b, c, d) {
  "use strict"
  const x = 10;
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  // long comment to trigger lazy compilation.
  x = 20; // This will trigger compile error with harmony scoping.
}

assertThrows("foo.length()");
