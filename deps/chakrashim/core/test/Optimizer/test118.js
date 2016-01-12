//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;

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
function getFirst23Elements(value, index, array) {
    return index < 23;
}
function sumOfArrayElements(prev, curr, index, array) {
    return prev + curr;
}
;
function test0() {
    var obj0 = {};
    var obj1 = {};
    var arrObj0 = {};
    var func2 = function() {
        var __loopvar3 = 0;
        for(var strvar0 in i16) {
            if(strvar0.indexOf('method') != -1) continue;
            if(__loopvar3++ > 3) break;
            c = 1;
            obj1 = obj0;
            arrObj0 = obj0;
        }
        e = (ary.unshift(f, c, arrObj0.length, arrObj0.length, c, f, g, f, a, h, __loopvar3, h, this.prop0, arrObj0.prop0, obj1.length));
    }
    obj0.method0 = func2;
    arrObj0.method0 = func2;
    var ary = new Array(10);
    var i16 = new Int16Array(256);
    var a = 1;
    var e = 1;
    var f = 1;
    var g = 1;
    var h = 1;
    obj0.length = 1;
    function bar0() {
        (h /= (((~arrObj0.method0.call(obj1)) * (1 instanceof ((typeof Number == 'function') ? Number : Object)) - (obj1.prop0 &= 1)) + (-(1 & obj0.prop1))));
    }
    function bar1() {
        e <<= bar0.call(obj0);
        bar0.call(obj0);
    }

    var __protoObj2_proto = {};
    __protoObj2_proto.x = Math.pow(bar1(), 1);
    var __protoObj1__proto = Object.create(__protoObj2_proto);;
    var __protoinstanceobj__proto = Object.create(__protoObj1__proto);
    var __varforproto = __protoinstanceobj__proto.x + __protoinstanceobj__proto.x;
    __varforproto += __protoinstanceobj__proto.x + __protoinstanceobj__proto.x;
    __varforproto += __protoinstanceobj__proto.x + __protoinstanceobj__proto.x;
    GiantPrintArray.push(__varforproto);
    if(shouldBailout)
        __protoObj1__proto.__proto__ = { x: "hello" };
    __varforproto += __protoinstanceobj__proto.x + __protoinstanceobj__proto.x;
    GiantPrintArray.push(__varforproto);
    WScript.Echo('sumOfary = ' + ary.filter(getFirst23Elements).reduce(sumOfArrayElements, 0));;
};

// generate profile
test0();
test0();
// Run Simple JIT
test0();
test0();

// run JITted code
runningJITtedCode = true;
test0();

// run code with bailouts enabled
shouldBailout = true;
test0();


// Baseline output:
// sumOfary = -Infinity
// sumOfary = -Infinity
// sumOfary = -Infinity
// sumOfary = -Infinity
// sumOfary = -Infinity
// sumOfary = -Infinity
// 
// 
// Test output:
// sumOfary = -Infinity
// sumOfary = -Infinity
// sumOfary = -Infinity
// sumOfary = -Infinity
// sumOfary = Infinity
// sumOfary = Infinity
//
