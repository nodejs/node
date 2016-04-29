// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --min-preparse-length=0

function f() {
  var a, b;
  [ a, b ] = [1, 2];
  assertEquals(1, a);
  assertEquals(2, b);
}

f();
