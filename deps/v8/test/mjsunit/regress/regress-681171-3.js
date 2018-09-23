// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-opt --function-context-specialization --verify-heap

(function __f_5() {
    var __v_0 = { *['a']() { yield 2; } };
    var __v_3 = __v_0.a();
    __v_3.next();
})();
