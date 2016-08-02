// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --min-preparse-length=0

let xxx = 1;
let f = undefined;
{
  let inner_x = xxx;
  f = function() { return inner_x; };
}

assertSame(1, f());

xxx = 42;
{
  f = function() { return inner_x1; };
  let inner_x1 = xxx;
}

assertSame(42, f());

xxx = 31;
{
  let inner_x1 = xxx;
  try {
    throw new Error();
  } catch (e) {
    f = function() { return inner_x1; };
  }
}
assertSame(31, f());
