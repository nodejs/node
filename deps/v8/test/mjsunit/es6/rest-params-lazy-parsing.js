// Copyright 2014 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --min-preparse-length=0

function variadic(co, ...values) {
  var sum = 0;
  while (values.length) {
    sum += co * values.pop();
  }
  return sum;
}

var arrowVariadic = (co, ...values) => {
  var sum = 0;
  while (values.length) {
    sum += co * values.pop();
  }
  return sum;
}

assertEquals(1, variadic.length);
assertEquals(1, arrowVariadic.length);

assertEquals(90, variadic(2, 1, 2, 3, 4, 5, 6, 7, 8, 9));
assertEquals(74, variadic(2, 1, 2, 3, 4, 5, 6, 7, 9));
assertEquals(110, variadic(2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));

assertEquals(90, arrowVariadic(2, 1, 2, 3, 4, 5, 6, 7, 8, 9));
assertEquals(74, arrowVariadic(2, 1, 2, 3, 4, 5, 6, 7, 9));
assertEquals(110, arrowVariadic(2, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10));
