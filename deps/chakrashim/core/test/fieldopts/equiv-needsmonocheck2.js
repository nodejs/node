//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    litObj0.prop0 = {
        prop0: -6498345155050780000,
        prop1: 2147483650,
        prop2: this,
        prop3: uniqobj3
    };
    for (;;) {
        function _array2iterate() {
            _array2iterate();
        }
        litObj0.prop0.v2 = uniqobj3;
        litObj0.prop0.v3 = litObj0;
        litObj0.prop0.v4 = litObj0.prop0.prop3;
        GiantPrintArray.push(litObj0.prop0.v4);
        break;
    }
    obj6.lf0 = uniqobj3.prop3 && this;
    WScript.Echo(GiantPrintArray);
}
var GiantPrintArray = [];
var obj0 = {};
var litObj0 = {};
var func1 = function () {
};
var func3 = function () {
};
obj0.method1 = func1;
protoObj0 = Object();
var uniqobj3 = {
    40: -347315309.9,
    prop0: 1770794796,
    prop3: protoObj0,
    prop7: protoObj0
};
litObj0.prop0 = {
    prop0: -6498345155050780000,
    prop1: 2147483650,
    prop2: this,
    prop3: uniqobj3
};
for (;;) {
    litObj0.prop0.v2 = uniqobj3;
    litObj0.prop0.v3 = litObj0;
    litObj0.prop0.v4 = litObj0;
    break;
}
obj6 = {};
test0();
test0();
