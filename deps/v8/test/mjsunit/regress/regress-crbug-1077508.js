// Copyright 2020 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

const array = [, , , 0, 1, 2];
const comparefn = () => {
  Array.prototype.__defineSetter__("0", function () {});
  Array.prototype.__defineSetter__("1", function () {});
  Array.prototype.__defineSetter__("2", function () {});
};

array.sort(comparefn);

assertArrayEquals([, , , , , , ], array);
