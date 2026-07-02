// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-sum-precise

(function () {
  let __v_0 = new Set([1, 3]);
  let __v_1 = __v_0.values();
  __v_0.delete(3);
  __v_1.return = function () {
    Math.sumPrecise(__v_1);
  };
  __v_0.add();
  assertThrows(() => Math.sumPrecise(__v_1), TypeError);
})();
