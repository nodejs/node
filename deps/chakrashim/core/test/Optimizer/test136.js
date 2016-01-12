//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var GiantPrintArray = [];
    function makeArrayLength() {
        return 100;
    }
    var obj0 = {};
    var obj1 = {};
    var arrObj0 = {};
    var func1 = function(argObj0) {
        function v0(o) {
            for(var v1 = 0; v1 < 8; v1++) {
                function v2() {
                }
                var v3 = v2();
                GiantPrintArray.push(argObj0);
                GiantPrintArray.push(v3);
                o[0] = 0;
            }
        }
        v0(arrObj0);
    };
    obj0.method0 = func1;
    obj1.method1 = obj0.method0;
    method0 = obj1.method1;
    arrObj0[arrObj0.length] = -246;
    Object.prototype.length = makeArrayLength();
    method0();
    WScript.Echo(GiantPrintArray);
}
test0();
test0();
