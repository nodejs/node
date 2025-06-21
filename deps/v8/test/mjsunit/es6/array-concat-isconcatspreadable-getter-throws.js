// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

"use strict";
function MyError() {}
var obj = {};
Object.defineProperty(obj, Symbol.isConcatSpreadable, {
  get: function() { throw new MyError(); }
});

assertThrows(function() {
  [].concat(obj);
}, MyError);

assertThrows(function() {
  Array.prototype.concat.call(obj, 1, 2, 3);
}, MyError);
