// Copyright 2015 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Flags: --allow-natives-syntax

var __v_7 = {"__proto__": this};
__v_9 = %CreatePrivateSymbol("__v_9");
this[__v_9] = "moo";
function __f_5() {
    __v_7[__v_9] = "bow-wow";
}
__f_5();
