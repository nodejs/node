//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

function FAILED()
{
    WScript.Echo("FAILED");
    throw (-1);
}

var e = -2147483648;
var l;
function test0(){
  l =((- e) << 1 );
  var m = (- e);
  
  return m;
};


for (var i = 0; i < 1000; i++)
{
    if (test0() != 2147483648)
    {
        FAILED();
    }
}

WScript.Echo("Passed");