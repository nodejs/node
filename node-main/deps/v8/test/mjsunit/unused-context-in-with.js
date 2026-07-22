// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var x = 1;
function foo(object) {
  with(object) {
    x;
  }
  return 100;
}

assertEquals(100,foo("str"));
