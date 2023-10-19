// Copyright 2022 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function foo(arr, value) {
    if (arr !== value) throw new Error('bad value: ' + arr);
}
function slice_array(arr) {
  return arr.slice();
}
for (var i = 0; i < 1e5; ++i) {
  var arr = [];
  var sliced = slice_array(arr);
  foo(arr !== sliced, true);
  try {
    foo(sliced.length);
  } catch (e) {}
}
