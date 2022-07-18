// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan --function-context-specialization --verify-heap

bar = function() { }

try {
  (function() {
     bar(...(function*(){ yield 1; yield 2; yield 3; })());
   })();
} catch(e) {}
