// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// See: http://code.google.com/p/v8/issues/detail?id=4377

// Switch statements should introduce their own lexical scope

'use strict';

switch (1) { case 1: let x = 2; }

assertThrows(function() { return x; }, ReferenceError);

{
  let result;
  let x = 1;
  switch (x) {
    case 1:
      let x = 2;
      result = x;
      break;
    default:
      result = 0;
      break;
  }
  assertEquals(1, x);
  assertEquals(2, result);
}

{
  let result;
  let x = 1;
  switch (eval('x')) {
    case 1:
      let x = 2;
      result = x;
      break;
    default:
      result = 0;
      break;
  }
  assertEquals(1, x);
  assertEquals(2, result);
}
