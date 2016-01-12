//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var GiantPrintArray = [];
function makeArrayLength() {
    return 100;
}
var obj0 = {};
var obj1 = {};
var func0 = function () {
};
var func4 = function () {
    obj1.method0 = --protoObj0.length;
    protoObj0.v1 = obj1.method0;
    GiantPrintArray.push(protoObj0.v1);
};
obj1.method0 = func0;
obj1.method1 = obj1.method0;
protoObj0 = Object.create(obj0);
protoObj0.length = makeArrayLength();
obj1.method1((func4()));
obj1 = protoObj0;
func4();
if (GiantPrintArray[0] !== 99 || GiantPrintArray[1] !== 98) {
    WScript.Echo('fail');
}
else {
    WScript.Echo('pass');
}
