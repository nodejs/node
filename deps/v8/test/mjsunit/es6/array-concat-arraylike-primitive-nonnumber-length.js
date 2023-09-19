// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
var obj = {
  "1": "A",
  "3": "B",
  "5": "C"
};
obj[Symbol.isConcatSpreadable] = true;
obj.length = {toString: function() { return "SIX"; }, valueOf: null };
assertEquals([], [].concat(obj));
obj.length = {toString: null, valueOf: function() { return "SIX"; } };
assertEquals([], [].concat(obj));
