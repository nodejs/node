// Copyright 2017 the V8 project authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

function __f_1()
{
    __v_1.p2 = 2147483648;
    __v_1.p3 = 3;
    __v_1.p4 = 4;
    __v_1.p5 = 2147483648;
    __v_1.p6 = 6;
}
function __f_2()
{
    delete __v_1.p6;
    delete __v_1.p5;
}
var __v_1 = { };
__f_1(__v_1);
__f_2(__v_1);
__f_1(__v_1);
