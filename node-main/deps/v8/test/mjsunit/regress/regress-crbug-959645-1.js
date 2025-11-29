// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax --modify-field-representations-inplace

function f(array, x) {
  array.x = x;
  array[0] = 1.1;
  return array;
}

f([1], 1);
f([2], 1);
%HeapObjectVerify(f([3], undefined));
