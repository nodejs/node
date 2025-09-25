// Copyright 2025 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags:

(function() {
  let arr = [2, ,4];
  arr.__proto__ = [3, 3, 3];
  const result = Array.prototype.concat([1.1], arr);
  assertEquals([1.1, 2, 3, 4], result);
  print(result);
})();

(function() {
  let arr = [2.2, ,4.4];
  arr.__proto__ = [3.3, 3.3, 3.3];
  const result = Array.prototype.concat([1.1], arr);
  assertEquals([1.1, 2.2, 3.3, 4.4], result);
  print(result);
})();
