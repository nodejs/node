// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

var ar;
Object.defineProperty(Array.prototype, 3,
    { get() { Object.freeze(ar); } });

function foo() {
  ar = [1, 2, 3];
  ar.length = 4;
  ar.pop();
}

assertThrows(foo, TypeError);
assertThrows(foo, TypeError);
assertThrows(foo, TypeError);
assertThrows(foo, TypeError);
