// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

with ({}) {
  eval("var x = 23");
  assertEquals(23, x);
}
assertEquals(23, x);
