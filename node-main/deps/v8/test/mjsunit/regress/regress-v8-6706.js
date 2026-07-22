// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const str = "a-b-c";

function test(re) {
  assertArrayEquals(["a", "b", "c"], re[Symbol.split](str));
}

!function() {
  const re = /-/y;
  re.lastIndex = 1;
  test(re);
}();

!function() {
  const re = /-/y;
  re.lastIndex = 3;
  test(re);
}();

!function() {
  const re = /-/y;
  re.lastIndex = -1;
  test(re);
}();

!function() {
  const re = /-/y;
  test(re);
}();

!function() {
  const re = /-/g;
  test(re);
}();
