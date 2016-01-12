//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var loopInvariant = 0;
    var obj0 = {};
    var protoObj0 = {};
    var protoObj1 = {};
    var obj2 = {};
    var func0 = function() {
    };
    var func4 = function() {
    };
    obj0.method0 = func0;
    obj0.method1 = obj0.method0;
    obj2.method0 = func4;
    Object.prototype.method0 = obj0.method0;
    var i32 = new Int32Array();
    var f64 = new Float64Array();
    var FloatArr0 = Array();
    var VarArr0 = [
        obj0,
        -806936368,
        -77,
        -1052351948922210000
    ];
    function v5() {
        var __loopvar2 = loopInvariant;
        do {
            if(__loopvar2 >= 2) {
                break;
            }
            __loopvar2++;
            function func10() {
            }
            var uniqobj5 = func10(FloatArr0.unshift(VarArr0[__loopvar2 + 1]));
            var uniqobj6 = [obj2];
            var uniqobj7 = uniqobj6[0];
            uniqobj7.method0();
        } while(~((i32[new obj0.method1(Object.prototype.prop4++).prop4 & 255] * (typeof protoObj0.prop2 != 'undefined') - ((typeof protoObj1.prop6 != 'undefined') instanceof (typeof EvalError == 'function' ? EvalError : Object))) * (f64[FloatArr0.unshift(test0.caller, (typeof protoObj1.prop6 != 'undefined') instanceof (typeof EvalError == 'function' ? EvalError : Object), VarArr0[loopInvariant + 1]) & this.prop5 <= Object.prototype.length & 255] - 200)));
    }
    v5();
    v5();
    for(var __loopvar2 = 0; obj2; obj2) {
        __loopvar2++;
        if(__loopvar2 >= 3) {
            break;
        }
        function func13() {
        }
        obj2 = new func13();
    }
    v5();
}
test0();
test0();

WScript.Echo("pass");
