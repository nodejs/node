//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

eval = function(){};

Get_ed = function () { return "ed"; }
Get_Fail = function () { return "Fail"; }

function foo()
{
    var Pa = "Pa" + "ss";
    var Pass = Pa + Get_ed();
    var PaFail = Pa + Get_Fail();
    eval();
    WScript.Echo(Pass);
}
foo()
