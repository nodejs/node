// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file is added after mjsunit.js on the number fuzzer in order to
// ignore optimization state assertions, which are invalid in many test cases
// when gc timings are fuzzed.

(function () {
  assertUnoptimized = function assertUnoptimized() {};
  assertOptimized = function assertOptimized() {};
})();
