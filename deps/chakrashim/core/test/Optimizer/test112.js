//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
var PolyFuncArr = [];
function GetPolymorphicFunction() {
    var myFunc = PolyFuncArr.shift();
    return myFunc;
}
function GetObjectwithPolymorphicFunction() {
    var obj = {};
    obj.polyfunc = GetPolymorphicFunction();
    return obj;
}
function InitPolymorphicFunctionArray() {
    for(var i = 0; i < arguments.length; i++) {
        PolyFuncArr.push(arguments[i]);
    }
}
function leaf() {
}
var obj0 = {};
var arrObj0 = {};
var func0 = function(argObj0) {
    !((Math.atan(-2), f64[!i16[obj0.prop1 & 255] & 255], ui32[leaf.call(obj0) & 255], arrObj0[((shouldBailout ? arrObj0[(!i16[obj0.prop1 & 255] >= 0 ? !i16[obj0.prop1 & 255] : 0) & 15] = 'x' : undefined, !i16[obj0.prop1 & 255]) >= 0 ? !i16[obj0.prop1 & 255] : 0) & 15], g /= argObj0.length ? this.prop1 instanceof (typeof Function == 'function' ? Function : Object) : this.prop0, argObj0.length--) * ((argObj0.prop0 |= typeof argObj0.length == 'undefined') + arrObj0[5]), b % +(1 % (2 >= obj0.prop1)) > (typeof obj0.prop1 == 'number'), leaf.call(arrObj0) / ((argObj0.prop0 -= obj0.prop1 * arrObj0.prop0 + 116 & (argObj0.prop0 <= g && a === arrObj0.length) ? -2 <= -26918378 < (-0 instanceof (typeof Error == 'function' ? Error : Object)) : shouldBailout ? leaf() : leaf()) == 0 ? 1 : argObj0.prop0 -= obj0.prop1 * arrObj0.prop0 + 116 & (argObj0.prop0 <= g && a === arrObj0.length) ? -2 <= -26918378 < (-0 instanceof (typeof Error == 'function' ? Error : Object)) : shouldBailout ? leaf() : leaf()), leaf.call(argObj0), ++this.prop1, ++this.prop1 ? ++this.prop1 : g);
};
var func2 = function() {
};
var i16 = new Int16Array(256);
var ui32 = new Uint32Array(256);
var f64 = new Float64Array(256);
var b = 1;
var g = 1602848414.1;
function bar0() {
    func0(obj0);
}
try {
    InitPolymorphicFunctionArray(bar0);
    var __polyobj = GetObjectwithPolymorphicFunction();
    switch(~(obj0.prop1 > this.prop0)) {
        default:
            __polyobj.polyfunc();
    }
    arrObj0(func2(__polyobj.polyfunc()), __polyobj.polyfunc());
}
catch(ex) {
}

WScript.Echo("pass");
