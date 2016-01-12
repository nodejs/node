//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// Check the case where not all of the upstream equiv set's types are equivalent
// at a downstream access.

var FixedFuncArr = [];
function bar() {
}
FixedFuncArr.push(bar);
function GetFunction() {
    var myFunc = FixedFuncArr.shift();
    FixedFuncArr.push(myFunc);
    return myFunc;
}
function PolyMorphicObjGenerator() {
    var obj = {};
    obj.fixedfunc1 = GetFunction();
    return obj;
}
function test0() {
    var _isntObj0 = PolyMorphicObjGenerator();
    var _protoObj0 = Object.create(_isntObj0);
    var GiantPrintArray = [];
    var arrObj0 = {};
    var func2 = function () {
        arrObj0.prop0;
        arrObj0.v2 = 1924086187;
        _protoObj0.fixedfunc1();
        GiantPrintArray.push(arrObj0.v2);
    };
    arrObj0.prop0 = 1458470962.1;
    CollectGarbage();
    CollectGarbage();
    func2();
    func2();
    func2();
    WScript.Echo(GiantPrintArray);
}
test0();
test0();
