//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function test0() {
    var obj0 = {};
    var arrObj0 = {};
    var func0 = function () {
    }
    var func1 = function (argMath0, argArrObj1, argArr2) {
        (function (argArrObj3, argMath4, argMath5) {
            b *= (f++);
        })(arrObj0, (obj0.length === arrObj0.prop0), (1 ? obj0.prop1 : this.prop0));
        if (shouldBailout) {
            return 'somestring'
        }
    }
    var func2 = function () {
        func1(1, 1, 1);
        (1 ? func0() : (1 ? (shouldBailout ? func0() : func0()) : (ary.pop())));
    }
    obj0.method0 = func2;
    var ary = new Array(10);
    var b = 1;
    var f = 1;
    if (shouldBailout) {
        func0 = obj0.method0;
    }
    func0();
};

test0();

// run code with bailouts enabled
shouldBailout = true;
try {
    test0();
}
catch(ex) {
    WScript.Echo(ex.message);
}



