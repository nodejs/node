// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var i = Math.pow(2, 31);
var a = [];
a[i] = 31;
var b = [];
b[i - 2] = 33;
try {
  // This is supposed to throw a RangeError.
  var c = a.concat(b);
  // If it didn't, ObservableSetLength will detect the problem.
  Object.observe(c, function() {});
  c.length = 1;
} catch(e) {
  assertTrue(e instanceof RangeError);
}
