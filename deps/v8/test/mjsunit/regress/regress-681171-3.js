// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation --function-context-specialization --verify-heap
// Flags: --invocation-count-for-turbofan=1

(function __f_5() {
    var __v_0 = { *['a']() { yield 2; } };
    var __v_3 = __v_0.a();
    __v_3.next();
    __v_3.next();
})();
