//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var GiantPrintArray = [];
var obj1 = {};
var litObj1 = {};
var func1 = function () {
    for (var _strvar4 of ary) {
      var _oo1obj = (function () {
          ary[6] = 524724361;
          litObj0 = litObj1;
      })();
    var fPolyProp = function (o) {
        WScript.Echo(o.prop0 + ' ' + o.prop1);
    };
    fPolyProp(litObj0);
}
};
var func2 = function () {
    while (func1()) {
    }
};
obj1.method0 = func2;
obj1.method1 = func1;
var ary = Array();
ary[0] = true;
func1();
for (var __loopvar1 = 6;; 6) {
    Object.defineProperty(litObj0, '__getterprop1', {});
    break;
}
obj1.method0();
function v10() {
    obj1.method1(...[litObj1.prop1++]);
}
GiantPrintArray.push(v10());
