//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var litObj0 = { prop1: 3.14159265358979 };
    function v1() {
        var v5 = { prop1: 0.1 };
        var __objtypespecfunc = function () {
        };
        litObj0.__objtypespecmethod0 = __objtypespecfunc;
        litObj0.prop1 = 1;
        litObj0.prop2 = 1;
        litObj0 = {};
    }
    v1();
    v1();
}
test0();
test0();
test0();

WScript.Echo('pass');
