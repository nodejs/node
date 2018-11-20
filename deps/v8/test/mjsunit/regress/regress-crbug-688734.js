// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo(a) {
  a[0] = 3;
}
var v = [,6];
v.__proto__ = [];
foo(v);
delete v[0];
var count = 0;
v.__proto__.__defineSetter__(0, function() { count++; });
foo([1,,,2]);
foo(v);
assertEquals(1, count);
