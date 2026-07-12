// Copyright 2023 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Fast array fill should be robust against evil object coercions.

(function TestStart() {
  let arr = [1,2,3,4];
  arr.fill(42, { toString() { arr.length = 0; } });
  assertEquals(4, arr.length);
  assertEquals([42,42,42,42], arr);
})();

(function TestEnd() {
  let arr = [1,2,3,4];
  arr.fill(42, 0, { toString() { arr.length = 0; return 4; } });
  assertEquals(4, arr.length);
  assertEquals([42,42,42,42], arr);
})();
