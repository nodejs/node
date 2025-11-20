// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap --allow-natives-syntax --expose-gc

let constant = { a: 1 };

function update_array(array) {
  array.x = constant;
  %HeapObjectVerify(array);
  array[0] = undefined;
  %HeapObjectVerify(array);
  return array;
}

let ar1 = [1];
let ar2 = [2];
let ar3 = [3];
gc();
gc();

update_array(ar1);
constant = update_array(ar2);
update_array(ar3);
