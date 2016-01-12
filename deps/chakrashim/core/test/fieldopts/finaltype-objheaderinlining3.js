//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var obj0 = {};
    var obj1 = {};
    var protoObj1 = {};
    var func3 = function () {
    };
    obj1.method0 = func3;
    var VarArr0 = Array();
    protoObj1 = Object.create(obj1);
    protoObj1.prop0 = -3503483882018380000;
    VarArr0[0] = -689066480;
    VarArr0[1] = -766274957.9;
    for (var _strvar23 in VarArr0) {
        protoObj1.length = -51;
        for (var _strvar0 in protoObj1) {
            if (_strvar0.indexOf('method') != -1) {
                continue;
            }
            protoObj1[_strvar0] = typeof obj0.prop0;
            protoObj1.method0.call();
            protoObj1 = {
                method0: function () {
                },
                method1: function () {
                }
            };
            protoObj1.prop0 = (protoObj1.prop1);
        }
    }
}
test0();

WScript.Echo('pass');
