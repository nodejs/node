// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

try {
  throw 1
} catch (v) {
  function foo() { return v; }
  foo();
  v = 2
}
assertEquals(2, foo());
