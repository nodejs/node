// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

let cnt = 0;
let reg = /./g;
reg.exec = () => {
  // Note: it's still possible to trigger OOM by passing huge values here, since
  // the spec requires building a list of all captures in
  // https://tc39.github.io/ecma262/#sec-regexp.prototype-@@replace
  if (cnt++ == 0) return {length: 2 ** 16};
  cnt = 0;
  return null;
};

assertThrows(() => ''.replace(reg, () => {}), RangeError);
