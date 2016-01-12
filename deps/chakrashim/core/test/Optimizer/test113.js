//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function test0() {
    var GiantPrintArray = [];
    var IntArr1 = [];
    for(var i = 0; i < 2; ++i) {
        var id30 = IntArr1;
        IntArr1 = IntArr1.pop();
        IntArr1 = id30;
        GiantPrintArray.push(IntArr1[IntArr1.length]);
        GiantPrintArray.push(test0a());
    }

    function test0a() {
        try {
        }
        catch(ex) {
        }
    }
};
test0();
test0();

WScript.Echo("pass");
