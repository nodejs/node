// Copyright 2019 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --always-turbofan

__v_1 = {};
__v_1.__defineGetter__('x', function () { });
__proto__ = __v_1;
function __f_4() {
    __v_1 = {};
}
function __f_3() {
    'use strict';
    x = 42;
}
__f_4()
try {
    __f_3();
} catch (e) { }

__proto__ = __v_1;
assertThrows(() => __f_3(), ReferenceError);
