//-------------------------------------------------------------------------------------------------------
// Copyright (C) Microsoft. All rights reserved.
// Licensed under the MIT license. See LICENSE.txt file in the project root for full license information.
//-------------------------------------------------------------------------------------------------------

var x = "glo";

function test(yes, param)
{
    function nested() { return param + x; }
    if (yes)
    {
        let x = nested() + " yes";
        let f = function() { WScript.Echo(x); }
        f();
    }
    else
    {
        for (var i = 0; i < 2; i++)
        {
            let x;
            if ( i == 0) {x = "0"; }
            let f2 = function() { WScript.Echo(x); }
            let f3 = function() { WScript.Echo("f3" + x); }
            f2();
            f3();
        }
    }
}


test(true, "test1");
test(true, "test2");
test(false, "test3");



