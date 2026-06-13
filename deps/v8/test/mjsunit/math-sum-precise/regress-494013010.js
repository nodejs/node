// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Flags: --js-sum-precise

(function () {
  let set = new Set([1, 3]);
  let it = set.values();
  set.delete(3);
  assertEquals(Math.sumPrecise(it), 1);
  set.add(3);
  assertEquals(Math.sumPrecise(it), -0);
})();
