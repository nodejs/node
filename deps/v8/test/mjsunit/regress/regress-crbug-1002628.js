// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-opt

"use strict";
var __v_0 = {};
try {
    __v_0 = this;
    Object.freeze(__v_0);
}
catch (e) {
}

function f() {
    x = { [Symbol.toPrimitive]: () => FAIL };
}
try {
    f()
} catch (e) { }
assertThrows(() => f(), ReferenceError);
