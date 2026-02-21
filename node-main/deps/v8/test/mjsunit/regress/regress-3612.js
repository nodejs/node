// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var a = [1];
var getterValue = 2;
var endIndex = 0xffff;
Object.defineProperty(a, endIndex, {
  get: function() {
    this[1] = 3;
    return getterValue;
  },
  set: function(val) {
    getterValue = val;
  },
  configurable: true,
  enumerable: true
});
a.reverse();
assertFalse(a.hasOwnProperty(1));
assertEquals(3, a[endIndex-1]);
