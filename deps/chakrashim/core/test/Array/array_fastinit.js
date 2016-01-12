//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function InitPolymorphicFunctionArray(args) {
}
function test0() {
    var IntArr2 = new Array();
    IntArr2[5] = 4294967295;
    IntArr2[0] = 2.53425738368173E+18;
    IntArr2[IntArr2.length] = 3;
    function bar0() {
    }
    InitPolymorphicFunctionArray(new Array(bar0));;
    WScript.Echo(IntArr2.slice(0, 23).reduce(function (prev, curr) { { return prev + curr; } }));
};
test0();
test0();
test0();
