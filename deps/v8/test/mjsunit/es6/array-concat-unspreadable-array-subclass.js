// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
class A extends Array {
  get [Symbol.isConcatSpreadable]() { return false; }
}
var obj = [].concat(new A(1, 2, 3), new A(4, 5, 6), new A(7, 8, 9));
assertEquals(3, obj.length);
assertEquals(3, obj[0].length);
assertEquals(3, obj[1].length);
assertEquals(3, obj[2].length);
