// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// http://ecma-international.org/ecma-262/6.0/#sec-json.stringify
// Step 4.b.iii.5.f.i

var calls = 0;

var num = new Number;
num.toString = function() {
  calls++;
  return '';
};
num.valueOf = function() {
  assertUnreachable();
};

JSON.stringify('', [num]);
assertEquals(1, calls);
