//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------


(function ()
{
var s2 = -1662342659;
var s4 = 0;
var s5 = 0;
var s6 = 0;
var s7 = 0;
for (var b = 0; b < 2; b++)
{
    s5 = s2;

    s6 = s2 + 1
    s7 = -s5;

    s2 = s6;
    s4 = s7;
}



WScript.Echo("s2 = " + s2);
WScript.Echo("s4 = " + s4);
}

)();

