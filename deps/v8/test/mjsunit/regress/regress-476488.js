// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan --expose-gc

function __f_0(message, a) {
  eval(), message;
  (function blue() {
    'use strict';
    eval(), eval(), message;
    gc();
  })();
}
__f_0();
