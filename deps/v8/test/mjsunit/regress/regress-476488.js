// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --no-lazy-feedback-allocation --expose-gc
// Flags: --invocation-count-for-turbofan=1

function __f_0(message, a) {
  eval(), message;
  (function blue() {
    'use strict';
    eval(), eval(), message;
    gc();
  })();
}
__f_0();
__f_0();
