// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
function MyError() {}
var obj = {
  "length": { valueOf: function() {
    throw new MyError();
  }, toString: null
},
"1": "A",
"3": "B",
"5": "C"
};
obj[Symbol.isConcatSpreadable] = true;
assertThrows(function() {
  [].concat(obj);
}, MyError);
