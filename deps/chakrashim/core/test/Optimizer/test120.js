//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var GiantPrintArray = [];
var reuseObjects = false;
var PolymorphicFuncObjArr = [];
var PolyFuncArr = [];
function GetPolymorphicFunction() {
    if(PolyFuncArr.length > 1) {
        var myFunc = PolyFuncArr.shift();
        PolyFuncArr.push(myFunc);
        return myFunc;
    }
    else {
        return PolyFuncArr[0];
    }
}
function GetObjectwithPolymorphicFunction() {
    if(reuseObjects) {
        if(PolymorphicFuncObjArr.length > 1) {
            var myFunc = PolymorphicFuncObjArr.shift();
            PolymorphicFuncObjArr.push(myFunc);
            return myFunc
        }
        else {
            return PolymorphicFuncObjArr[0];
        }
    }
    else {
        var obj = {};
        obj.polyfunc = GetPolymorphicFunction();
        PolymorphicFuncObjArr.push(obj)
        return obj
    }
};
function InitPolymorphicFunctionArray() {
    for(var i = 0; i < arguments.length; i++) {
        PolyFuncArr.push(arguments[i])
    }
}
;
function test0() {
    //letconst3.ecs
    function v79580() {
        var v79581 = -2147483646;
        {
            const v79581 = 1;
            GiantPrintArray.push(v79581);
        }
        GiantPrintArray.push(v79581);
    }
    v79580();
};

// generate profile
test0();
// Run Simple JIT
test0();
test0();
test0();
test0();
test0();

// run JITted code
runningJITtedCode = true;
test0();

WScript.Echo("pass");
