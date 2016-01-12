//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var obj0 = {};
    var obj1 = {};
    var func1 = function () {
        function _oo2constructor() {
            ary.reverse();
            this.func1 = function () {
            };
        }
        var _oo2obj1 = _oo2constructor();
        var _oo2obj2 = new _oo2constructor();
        _oo2obj2.func1(ary.pop());
    };
    var func2 = function () {
        return func1(func1());
    };
    var func3 = function () {
    };
    obj0.method1 = func3;
    obj1.method0 = func2;
    var ary = new Array(10);
    var IntArr1 = [];
    ary[0] = 1638249354;
    var v11 = obj0.method1();
    delete ary[v11];
    function func12() {
    }
    var uniqobj8 = func12(IntArr1.unshift(obj1.method0(), func1()));
    WScript.Echo(ary.slice().reduce(function () {
    }));
    WScript.Echo(ary.slice());
}
test0();
test0();
