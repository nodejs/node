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
    var obj0 = {};
    var obj1 = {};
    var arrObj0 = {};
    var func0 = function(argArrObj0, argFunc1, argMath2, argArr3) {
        arrObj0.prop0 = argArr3.push(arrObj0.prop0);
        GiantPrintArray.push(arrObj0.prop0);
        var _y = argArr3.length;
        var _x = argArr3[_y];
        GiantPrintArray.push(_x);
    }
    var func1 = function(argMath4) {
        (obj0.length = (c++));
    }
    var func2 = function(argMath5, argArrObj6, argFunc7) {
        GiantPrintArray.push('obj1.prop0 = ' + (obj1.prop0 | 0));
    }
    obj0.method0 = func1;
    var f64 = new Float64Array(256);
    var FloatArr0 = [];
    var c = 1;
    var aliasOfobj0 = obj0;;
    var aliasOfarrObj0 = arrObj0;;
    var aliasOfFloatArr0 = FloatArr0;;
    function bar0(argFunc8, argArrObj9, argMath10, argArrObj11) {
        arrObj0.prop0 = 1;
    }
    if((bar0(1, 1, 1, 1) >= func2.call(arrObj0, obj0.method0.call(obj1, 1), 1, 1))) {
    }
    else {
        aliasOfFloatArr0[(2)] = 1;
    }
    var __loopvar1 = 0;
    for(var strvar0 in f64) {
        if(strvar0.indexOf('method') != -1) continue;
        if(__loopvar1++ > 3) break;
        func0.call(aliasOfobj0, 1, 1, 1, aliasOfFloatArr0);
        // Snippet switch2
        aliasOfarrObj0.prop0 = (function() {
            switch(Object.keys(obj0).length) {
                case 1:
                    return 1;
                case 2:
                    return aliasOfarrObj0.length;
                case 3:
                    return 1;
                case 4:
                case 5:
                case "1":
                    return 1;
                case "2":
                    return 1;
                case "3":
                    return 1;
                case "4":
                    return 1;
            }
        })();
    }
};

// generate profile
test0();

// run JITted code
runningJITtedCode = true;
test0();

WScript.Echo("pass");
