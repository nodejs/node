//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var shouldBailout = false;
function test0() {
    var litObj0 = { prop1: 3.14159265358979 };
    var IntArr0 = new Array(-2664941844011450000, 7899446760907481000);
    var b = 1;
    var aliasOflitObj0 = litObj0;
    ;
    e = 1 > 1;
    IntArr0[('prop0' in aliasOflitObj0 >= 0 ? 'prop0' in aliasOflitObj0 : 0) & 15] = --e;
    v7082 = {};
    for(v7083 = 0; v7083 < 10; v7083++) {
        b += v7083;
    }
    v7084 = 'abcdefghij' + (v7082 += 1) + IntArr0[((shouldBailout ? IntArr0[({
        prop0: 1,
        prop1: 1
    } >= 0 ? {
        prop0: 1,
        prop1: 1
    } : 0) & 15] = 'x' : undefined, {
        prop0: 1,
        prop1: 1
    }) >= 0 ? {
        prop0: 1,
        prop1: 1
    } : 0) & 15]();
    v7085 = 'abcdefghij' + (v7082 += 1) + e;
    GiantPrintArray.push(v7084 + e);
}
try {
    test0();
    test0();
    test0();
    runningJITtedCode = true;
    test0();
    shouldBailout = true;
    test0();
}
catch(ex) {
    WScript.Echo(ex.message);
}
