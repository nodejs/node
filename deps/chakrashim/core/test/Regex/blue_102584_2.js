//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

// bailout on implicit call for CoerseRegex
var i;
function test0(a) {
    var x = 0;
    for(i = 0;i < 4;i++) {
        x += "".match({});
        x += a[i];
    }
    return x;
};

function test1(a) {
    var x = 0;
    for(i = 0;i < 4;i++) {
        x += /a/.exec({});
        x += a[i];
    }
    return x;
};

function test2(a) {
    var x = 0;
    for(i = 0;i < 4;i++) {
        x += "".replace({}, "a");
        x += a[i];
    }
    return x;
};

var arr = [1,2,3,4,5,6];
test0(arr);
test1(arr);
test2(arr);
Object.prototype.toString = function(x) {  WScript.Echo("implicit" + i); return ""; }
test0(arr);
test1(arr);
test2(arr);
