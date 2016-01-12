//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var GiantPrintArray = [];
var reuseObjects = false;
var PolymorphicFuncObjArr = [];
var PolyFuncArr = [];
function GetPolymorphicFunction() {
    if (PolyFuncArr.length > 1) {
        var myFunc = PolyFuncArr.shift();
        PolyFuncArr.push(myFunc);
        return myFunc;
    }
    else {
        return PolyFuncArr[0];
    }
}
function GetObjectwithPolymorphicFunction() {
    if (reuseObjects) {
        if (PolymorphicFuncObjArr.length > 1) {
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
function InitPolymorphicFunctionArray(args) {
    PolyFuncArr = [];
    for (var i = 0; i < args.length; i++) {
        PolyFuncArr.push(args[i])
    }
}
;
function test0() {
    var obj0 = {};
    var obj1 = {};
    var arrObj0 = {};
    var func0 = function (argMath0) {
        function func3(argArr1) {
        }
        this.prop1;
    }
    var func1 = function (argObj2, argFunc3, argArr4, argArr5) {
        eval("'use strict';");
        func0(1);
    }
    var func2 = function () {
    }
    var ary = new Array(10);
    var f32 = new Float32Array(256);
    var f64 = new Float64Array(256);
    var IntArr0 = [131, 148, 6626500187963896832, 1496284996];
    var strvar5 = 1;
    function bar0() {
        func1.call(obj0, 1, 1, 1, 1);
    }
    InitPolymorphicFunctionArray(new Array(bar0));;
    var __polyobj = GetObjectwithPolymorphicFunction();;
    var __loopvar1 = 0;
    do {
        __loopvar1++;
        func0.call(obj1, 1);
    } while ((1) && __loopvar1 < 3)
    obj1.prop0 = {
        prop4: 1, prop3: 1, prop2: (IntArr0.push((func2.call(obj0) > ((obj1.length >= arrObj0.prop1) || (arrObj0.prop1 <= obj0.prop1))), ((true instanceof ((typeof Number == 'function') ? Number : Object)) !== true), 80, (typeof (obj0.prop0) == 'object'), (f64[(__polyobj.polyfunc.call(arrObj0)) & 255] * (f32[(('B!²E´' + '(CqÔc=VÌ'.indexOf(strvar5))) & 255] + ary[((((true instanceof ((typeof EvalError == 'function') ? EvalError : Object)) >= 0 ? (true instanceof ((typeof EvalError == 'function') ? EvalError : Object)) : 0)) & 0XF)])), (typeof ('$') != null)))
  , prop1: 1, prop0: 1
    };
};

// generate profile
test0();
// Run Simple JIT
test0();

// run JITted code
runningJITtedCode = true;
test0();

WScript.Echo('pass');