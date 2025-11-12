// Copyright 2016 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";

class Test {
  m() {
    super.length = 10;
  }
}

var array = [];
Test.prototype.m.call(array);
assertEquals(10, array.length);
