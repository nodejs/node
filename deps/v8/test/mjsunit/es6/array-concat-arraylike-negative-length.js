// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var obj = {
  "length": -6,
  "1": "A",
  "3": "B",
  "5": "C"
};
obj[Symbol.isConcatSpreadable] = true;
assertEquals([], [].concat(obj));
obj.length = -6.7;
assertEquals([], [].concat(obj));
obj.length = "-6";
assertEquals([], [].concat(obj));
