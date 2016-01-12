//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var NotNegZero = 0;
var NegZero = 0;

function checkisnegativezero(x, str)
{
    // this is a quick way to check if a number is -0
    if(x != 0 || 1/x >= 0)
    {
        NotNegZero++;
    }
    else 
    {
        NegZero++;
    }
}

var Y = 0;
var X = -5;
var one = 1;

var A = new Array();
function foo(x, y) {
    checkisnegativezero(x % y);
    foo2(x);
}
function foo2(x) {
    checkisnegativezero(x % 2);
}


for (var i = 0; i < 2000; i++)
    foo(2, 2);

foo(-2, 2);

if (NotNegZero != 4000 || NegZero != 2)
    WScript.Echo("FAILED\n");
else
    WScript.Echo("Passed\n");
