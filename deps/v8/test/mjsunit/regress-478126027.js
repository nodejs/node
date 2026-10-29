// Copyright 2026 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --verify-heap

function __f_0(
    __v_9, __v_10, __v_11, __v_12, __v_13,
    __v_14, __v_15, __v_16, __v_17, __v_18,
    __v_19, __v_20, __v_21, __v_22, __v_23,
    __v_24, __v_25, __v_26) {
    Object.seal(arguments);
}

__f_0(
    __f_0, __f_0, __f_0, __f_0, __f_0,
    __f_0, __f_0, __f_0, __f_0, __f_0,
    __f_0, __f_0, __f_0, __f_0, __f_0,
    __f_0, __f_0, __f_0);
