// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
class NonArray {
  constructor() { Array.apply(this, arguments); }
};

var obj = new NonArray(1,2,3);
var result = Array.prototype.concat.call(obj, 4, 5, 6);
assertEquals(Array, result.constructor);
assertEquals([obj,4,5,6], result);
assertFalse(result instanceof NonArray);
