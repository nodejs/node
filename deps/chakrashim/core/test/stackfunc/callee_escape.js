//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var glo;
function test(param)
{
    function nested()
    {
        escape();
        return param;
    }
    function escape()
    {
        if (!glo)
            glo = arguments.callee.caller;
    }

    nested();
}


test("test1");
WScript.Echo(glo());
glo = undefined;


