//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

//only flag needed -ForceArrayBTree
function test0()
{
    var GiantPrintArray = [];
    var arrObj0 = {};
    var VarArr0 = Array();
    VarArr0[5] = 1;
    var i = 0;
    do {
        i++;
        Object.defineProperty(arrObj0, '5', {
            get: function () {
                GiantPrintArray.push('getter');
                WScript.Echo("Inside getter FAILED");
                return 5;
            },configurable: true
        });
        VarArr0[10] = 1;
        VarArr0.slice();
        arrObj0 = Object.prototype;
    } while (i < 2);
    delete arrObj0['5'];
    delete Object.prototype['5'];
}

function test1()
{
    var Debug = true;
    var ary1 = new Array(10);
    Object.defineProperty(Object.prototype, '5', {
            get: function () {
                if(Debug) WScript.Echo("Inside getter Object 1");
                return 3;
            }
     });
    Object.defineProperty(Array.prototype, '5', {
            get: function () {
                if(Debug) WScript.Echo("Inside getter Array 1");
                return 4;
            }
     });
    slAr = ary1.slice();
    if(ary1[5] != 4)
    {
       WScript.Echo("FAIL ary1[5] = ",ary1[5]);
       WScript.Echo("src  Array: ",ary1);
       WScript.Echo("dest Array: ",slAr);
    }
}
test0();
test1();
